#include "media/audio/audio_core.h"

#include <unordered_map>
#include <map>
#include <list>
#include <array>
#include <unordered_set>
#include <mutex>
#include <iostream>
#include <deque>
#include <chrono>

#include "core/platform.h"

#include "SDL.h"
#include "SDL_sound.h"

#include "media/audio/openal.h"

#include "allegro.h"

#include "ac/asset_helper.h"
#include "media/audio/audiodefines.h"
#include "debug/out.h"
#include "ac/common.h"
#include "main/engine.h"
#include "util/stream.h"
#include "util/thread.h"

// #define AUDIO_CORE_DEBUG

// TODO:
// slot id generation id
// pre-determine music sizes
// safer slot look ups (with gen id)
// generate/load mod/midi offsets


namespace ags = AGS::Common;
namespace agsdbg = AGS::Common::Debug;
namespace agseng = AGS::Engine;


const auto SampleDefaultBufferSize = 64 * 1024;
const auto SourceMinimumQueuedBuffers = 3;

struct SoundSampleDeleterFunctor {
    void operator()(Sound_Sample* p) {
        Sound_FreeSample(p);
#ifdef AUDIO_CORE_DEBUG
        agsdbg::Printf(ags::kDbgMsg_Init, "SoundSampleDeleterFunctor");
#endif
    }
};

using SoundSampleUniquePtr = std::unique_ptr<Sound_Sample, SoundSampleDeleterFunctor>;


static void audio_core_entry();
agseng::Thread *audio_core_thread_ptr = nullptr;
bool audio_core_thread_running = false;

enum AudioCoreSlotCommandType { Nothing, Initialise, Configure, Play, Pause, StopAndRelease, Seek};


struct AudioCoreSlotCommand {
    int handle = -1;

    AudioCoreSlotCommandType command = AudioCoreSlotCommandType::Nothing;

    AssetPath assetPath {};
    ags::String assetExtension {};
    AGS::Common::Stream *hackstream = nullptr;
    size_t hacksize = 0;

    float seekPosMs = -1.0f;

    bool repeat = false;
    float volume = 1.0f;
    float speed = 1.0f;
    float panning = 0.0f;
};

struct AudioCoreSlotStatus {
    int handle_ = -1;
    AudioCorePlayState playState = Initial;
    float positionMs = 0.0f;
};


// slot ids
std::mutex ids_mutex_;
int nextId = 0;

// in
std::mutex config_mutex_;
std::condition_variable config_wakeup_cv_;
float masterVolumeConfig_ = 1.0f;
std::deque<AudioCoreSlotCommand> commandList_;


// out
std::mutex status_mutex_;
std::unordered_map<int, AudioCoreSlotStatus> statuses_;



// -------------------------------------------------------------------------------------------------
// AUDIO ASSET INFO
// -------------------------------------------------------------------------------------------------


float audio_core_asset_length_ms(const AssetPath &assetPath, const AGS::Common::String &extension)
{
    // https://wiki.libsdl.org/SDL_RWops
    return -1;
}

int audio_core_asset_sound_type(const AssetPath &assetPath, const AGS::Common::String &extension)
{
    // https://wiki.libsdl.org/SDL_RWops
    return 0;
}


// -------------------------------------------------------------------------------------------------
// INIT/SHUTDOWN
// -------------------------------------------------------------------------------------------------

void audio_core_init() 
{
    audio_core_thread_ptr = new agseng::Thread();
    audio_core_thread_running = true;
    audio_core_thread_ptr->CreateAndStart(audio_core_entry, false);
}

void audio_core_shutdown()
{
    audio_core_thread_running = false;
    if (!audio_core_thread_ptr) { return; }
    audio_core_thread_ptr->Stop();
}


// -------------------------------------------------------------------------------------------------
// SLOTS
// -------------------------------------------------------------------------------------------------


static int avail_slot_id()
{
    auto result = nextId;
    nextId += 1;
    return result;
}

// if none free, add a new one.
int audio_core_slot_initialise(const AssetPath &assetPath, const ags::String &extension, AGS::Common::Stream *hackStream, size_t hacksize)  
{
    auto handle = avail_slot_id();

    auto cmd = AudioCoreSlotCommand {handle, AudioCoreSlotCommandType::Initialise};
    cmd.assetPath = assetPath;
    cmd.assetExtension = extension;
    cmd.hackstream = hackStream;
    cmd.hacksize = hacksize;

    std::lock_guard<std::mutex> lk(config_mutex_);
    commandList_.push_back(cmd);
    config_wakeup_cv_.notify_all();

    return handle;
}


// -------------------------------------------------------------------------------------------------
// CONFIG
// -------------------------------------------------------------------------------------------------

// TODO notify on change.

void audio_core_set_master_volume(float newvol) {
    std::lock_guard<std::mutex> lk(config_mutex_);
    masterVolumeConfig_ = newvol;
    config_wakeup_cv_.notify_all();
}


void audio_core_slot_play(int slot_handle)
{
    auto cmd = AudioCoreSlotCommand {slot_handle, AudioCoreSlotCommandType::Play};

    std::lock_guard<std::mutex> lk(config_mutex_);
    commandList_.push_back(cmd);
    config_wakeup_cv_.notify_all();
}

void audio_core_slot_pause(int slot_handle)
{
    auto cmd = AudioCoreSlotCommand {slot_handle, AudioCoreSlotCommandType::Pause};

    std::lock_guard<std::mutex> lk(config_mutex_);
    commandList_.push_back(cmd);
    config_wakeup_cv_.notify_all();
}

void audio_core_slot_stop(int slot_handle)
{
    auto cmd = AudioCoreSlotCommand {slot_handle, AudioCoreSlotCommandType::StopAndRelease};

    std::lock_guard<std::mutex> lk(config_mutex_);
    commandList_.push_back(cmd);
    config_wakeup_cv_.notify_all();
}

void audio_core_slot_seek_ms(int slot_handle, float pos_ms)
{
    auto cmd = AudioCoreSlotCommand {slot_handle, AudioCoreSlotCommandType::Seek};
    cmd.seekPosMs = pos_ms;

    std::lock_guard<std::mutex> lk(config_mutex_);
    commandList_.push_back(cmd);
    config_wakeup_cv_.notify_all();
}

void audio_core_slot_configure(int slot_handle, bool repeat, float volume, float speed, float panning)
{
    auto cmd = AudioCoreSlotCommand {slot_handle, AudioCoreSlotCommandType::Configure};
    cmd.repeat = repeat;
    cmd.volume = volume;
    cmd.speed = speed;
    cmd.panning = panning;

    std::lock_guard<std::mutex> lk(config_mutex_);
    commandList_.push_back(cmd);
    config_wakeup_cv_.notify_all();
}


// -------------------------------------------------------------------------------------------------
// STATUS
// -------------------------------------------------------------------------------------------------

float audio_core_slot_get_pos_ms(int slot_handle)
{
    std::lock_guard<std::mutex> lk(status_mutex_);
    auto &status = statuses_[slot_handle];
    return status.positionMs;

}
AudioCorePlayState audio_core_slot_get_play_state(int slot_handle)
{
    std::lock_guard<std::mutex> lk(status_mutex_);
    auto &status = statuses_[slot_handle];
    return status.playState;
}


// -------------------------------------------------------------------------------------------------
// AUDIO PROCESSING
// -------------------------------------------------------------------------------------------------


std::vector<ALuint> freeBuffers_;



void dump_al_errors() 
{
    bool errorFound = false;
    for (;;) {
        auto err = alGetError();
        if (err == AL_NO_ERROR) { break; }
        errorFound = true;
        agsdbg::Printf(ags::kDbgMsg_Init, "error: %s", alGetString(err));
    }
    assert(!errorFound);
}


float buffer_duration_ms(ALuint bufferID) {
    ALint sizeInBytes;
    ALint channels;
    ALint bits;
    ALint frequency;

    alGetBufferi(bufferID, AL_SIZE, &sizeInBytes);
    alGetBufferi(bufferID, AL_CHANNELS, &channels);
    alGetBufferi(bufferID, AL_BITS, &bits);
    alGetBufferi(bufferID, AL_FREQUENCY, &frequency);

    auto lengthInSamples = sizeInBytes * 8 / (channels * bits);
    return 1000.0f * (float)lengthInSamples / (float)frequency;
}


ALenum openalFormatFromSample(const SoundSampleUniquePtr &sample) {
  if (sample->desired.channels == 1) {
    switch (sample->desired.format) {
      case AUDIO_U8:
        return AL_FORMAT_MONO8;
      case AUDIO_S16SYS:
        return AL_FORMAT_MONO16;
      case AUDIO_F32SYS:
        if (alIsExtensionPresent("AL_EXT_float32")) {
          return alGetEnumValue("AL_FORMAT_MONO_FLOAT32");
        }
    }
  } else if (sample->desired.channels == 2) {
    switch (sample->desired.format) {
      case AUDIO_U8:
        return AL_FORMAT_STEREO8;
      case AUDIO_S16SYS:
        return AL_FORMAT_STEREO16;
      case AUDIO_F32SYS:
        if (alIsExtensionPresent("AL_EXT_float32")) {
          return alGetEnumValue("AL_FORMAT_STEREO_FLOAT32");
        }
    }
  }

#ifdef AUDIO_CORE_DEBUG
  agsdbg::Printf(ags::kDbgMsg_Init, "openalFormatFromSample: bad format chan:%d format: %d", sample->actual.channels, sample->actual.format);
#endif

  return 0;
}


struct AudioCoreSlot {

    int handle_ = -1;
    bool released_ = false;

    AudioCorePlayState playState_ = Initial;
    bool EOS_ = false;

    AssetPath assetPath_ {};
    ags::String assetExtension_ {};

    std::vector<uint8_t> sampleBuffer_ {};
    SoundSampleUniquePtr sample_ = nullptr;
    ALenum sampleOpenAlFormat_ = 0;

    ALuint source_ = 0;

    float unqueuedBufferPosMs_ = 0.0f;

    float positionMs_ = 0.0f;

    bool repeat_ = false;
    float volume_ = 1.0f;
    float speed_ = 1.0f;
    float panning_ = 0.0f;
    bool needs_resending_ = true;


    // unqueue buffers, returns time in ms
    void UnqueueProcessedBuffers() 
    {
        if (source_ <= 0) { return; }

        for (;;) {
            ALint buffersProcessed = -1;
            alGetSourcei(source_, AL_BUFFERS_PROCESSED, &buffersProcessed);
            dump_al_errors();
            if (buffersProcessed <= 0) { break; }

            ALuint b;
            alSourceUnqueueBuffers(source_, 1, &b);
            dump_al_errors();

            this->unqueuedBufferPosMs_ += buffer_duration_ms(b);

            freeBuffers_.push_back(b);
        }
    }

    void ClearSource() 
    {
        if (source_ <= 0) { return; }

        alSourceStop(source_);  // stop so all buffers can be unqueued
        dump_al_errors();

        UnqueueProcessedBuffers();

        alDeleteSources(1, &source_);
        dump_al_errors();

        source_ = 0;
    }



    void LoadSample(const AssetPath &assetPath, const AGS::Common::String &assetExtension, AGS::Common::Stream *hackStream, size_t hacksize) 
    {
        // auto &assetlib = config.assetPath.first;
        // auto &assetname = config.assetPath.second;

        // WithAssetLibrary w(assetlib);
        // ags::AssetLocation loc;
        // if (!ags::AssetManager::GetAssetLocation(assetname, loc)) { return; }
        // const auto sz = ags::AssetManager::GetAssetSize(assetname);

        {
            // auto s = std::unique_ptr<Stream>(ags::AssetManager::OpenAsset(assetname, ags::kFile_Open, ags::kFile_Read));
            sampleBuffer_.resize(hacksize);
            hackStream->Read(sampleBuffer_.data(), sampleBuffer_.size());
            delete hackStream;
        }

        auto sample = SoundSampleUniquePtr(Sound_NewSampleFromMem(sampleBuffer_.data(), sampleBuffer_.size(), assetExtension.GetCStr(), nullptr, SampleDefaultBufferSize));
        if(!sample) { return; }

        auto bufferFormat = openalFormatFromSample(sample);

        if (bufferFormat <= 0) {
        #ifdef AUDIO_CORE_DEBUG
            agsdbg::Printf(ags::kDbgMsg_Init, "audio_core_sample_load: RESAMPLING");
        #endif
            auto desired = Sound_AudioInfo { AUDIO_S16SYS, sample->actual.channels, sample->actual.rate };

            Sound_FreeSample(sample.get());
            sample = SoundSampleUniquePtr(Sound_NewSampleFromMem(sampleBuffer_.data(), sampleBuffer_.size(), assetExtension.GetCStr(), &desired, SampleDefaultBufferSize));

            if(!sample) { return; }

            bufferFormat = openalFormatFromSample(sample);
        }

        if (bufferFormat <= 0) { return; }

        assetPath_ = assetPath;
        assetExtension_ = assetExtension;
        sample_ = std::move(sample);
        sampleOpenAlFormat_ = bufferFormat;
    }



    void Initialise(int handle, const AssetPath &assetPath, const AGS::Common::String &extension, AGS::Common::Stream *hackStream, size_t hackSize)
    {
        // We get a fresh source in-case it's in an error state.
        ClearSource();

        handle_ = handle;
        released_ = false;
        playState_ = Initial;
        EOS_ = false;

        assetPath_ = {};
        assetExtension_.Empty();
        
        sampleBuffer_.resize(0);
        sample_ = nullptr;
        sampleOpenAlFormat_ = 0;

        source_ = 0;
        unqueuedBufferPosMs_ = 0.0f;
        positionMs_ = 0.0f;

        repeat_ = false;
        volume_ = 1.0f;
        speed_ = 1.0f;
        panning_ = 0.0f;
        needs_resending_ = true;

        // new source!
        alGenSources(1, &source_);
        dump_al_errors();

        LoadSample(assetPath, extension, hackStream, hackSize);
    }

    void Configure(bool repeat, float volume, float speed, float panning) {
        repeat_ = repeat;
        volume_ = volume;
        speed_ = speed;
        panning_ = panning;
        needs_resending_ = true;
    }

    void Seek(float pos_ms) {

        if (!source_ || !sample_) { Stop(); return; }

        alSourceStop(source_);  // stop so all buffers can be unqueued
        dump_al_errors();

        UnqueueProcessedBuffers();

        Sound_Seek(sample_.get(), pos_ms);
        unqueuedBufferPosMs_ = pos_ms;

        // wait until Update() to get played again.
    }

    void Play() {
        if (!source_ || !sample_) { Stop(); return; }

        playState_ = Playing;
    }

    void Pause() {
        if (!source_ || !sample_) { Stop(); return; }

        playState_ = Paused;
    }

    void Stop() {
        ClearSource();
        playState_ = Stopped;
        source_ = 0;
        sample_ = nullptr;
    }

    void StopAndRelease() {
        Stop();
        released_ = true;
    }

    void Update() {
        if (playState_ == Stopped) { /* stopped already */ Stop(); return; }
        if (!source_ || !sample_) { /* bad state */ Stop(); return; }

        if (playState_ == Initial) { /* waiting to be played */ return; }

        if (needs_resending_) {
            alSourcef(source_, AL_GAIN, volume_*0.7f);
            dump_al_errors();

            alSourcef(source_, AL_PITCH, speed_);
            dump_al_errors();

            if (panning_ != 0.0f) {
                // https://github.com/kcat/openal-soft/issues/194
                alSourcef(source_, AL_ROLLOFF_FACTOR, 0.0f);
                dump_al_errors();
                alSourcei(source_, AL_SOURCE_RELATIVE, AL_TRUE);
                dump_al_errors();
                alSource3f(source_, AL_POSITION, panning_, 0.0f, 0.0f);
                dump_al_errors();
            }
            else {
                alSourcef(source_, AL_ROLLOFF_FACTOR, 1.0f);
                dump_al_errors();
                alSourcei(source_, AL_SOURCE_RELATIVE, AL_FALSE);
                dump_al_errors();
                alSource3f(source_, AL_POSITION, 0.0f, 0.0f, 0.0f);
                dump_al_errors();
            }
        }
        needs_resending_ = false;


        // buffer management
        UnqueueProcessedBuffers();

        // generate extra buffers if none are free
        if (freeBuffers_.size() < SourceMinimumQueuedBuffers) {
            std::array<ALuint, SourceMinimumQueuedBuffers> genbufs;
            alGenBuffers(genbufs.size(), genbufs.data());
            dump_al_errors();
            for (const auto &b : genbufs) {
                freeBuffers_.push_back(b);
            }
        }

        // decode and attach buffers
        while (!EOS_) {
            ALint buffersQueued = -1;
            alGetSourcei(source_, AL_BUFFERS_QUEUED, &buffersQueued);
            dump_al_errors();

            if (buffersQueued >= SourceMinimumQueuedBuffers) { break; }

            auto it = std::prev(freeBuffers_.end());
            auto b = *it;

            auto sz = Sound_Decode(sample_.get());

            if (sz <= 0) {
                EOS_ = true;
                // if repeat, then seek to start.
                if (repeat_) {
                    auto res = Sound_Rewind(sample_.get());
                    auto success = (res != 0);
                    EOS_ = !success;
                }
                continue;
            }

            alBufferData(b, sampleOpenAlFormat_, sample_->buffer, sz, sample_->desired.rate);
            dump_al_errors();

            alSourceQueueBuffers(source_, 1, &b);
            dump_al_errors();

            freeBuffers_.erase(it);
        }


        // setup play state

        ALint state = AL_INITIAL;
        alGetSourcei(source_, AL_SOURCE_STATE, &state);
        dump_al_errors();

        switch (playState_) {
        case Playing:
            if (state != AL_PLAYING) {
                alSourcePlay(source_);
                dump_al_errors();
            }
            break;

        case Paused:
            if (state != AL_PAUSED) {
                alSourcePause(source_);
                dump_al_errors();
            }
            break;

        case Stopped:
        case Initial:
            // dunno how we got here.
            break;


        }


        // update status

        float alSecOffset = 0.0f;
        // disabled with mojoal
        // if source available:
        //      alGetSourcef(slot.source_, AL_SEC_OFFSET, &alSecOffset);
        //      dump_al_errors();
        positionMs_ = unqueuedBufferPosMs_ + alSecOffset;

        // if end of stream and still not playing, we done here.
        ALint buffersRemaining = 0;
        alGetSourcei(source_, AL_BUFFERS_QUEUED, &buffersRemaining);
        dump_al_errors();
        if (EOS_ && buffersRemaining <= 0) {
            Stop();
        }

    }
};


// internal
// no mutex, should only be accessed through thread
// TODO, only exist in context of thread.
std::unordered_map<int, AudioCoreSlot> slots_;


static void audio_core_entry()
{
    ALCdevice *alcDevice = nullptr;
    ALCcontext *alcContext = nullptr;

    try {


        /* InitAL opens a device and sets up a context using default attributes, making
        * the program ready to call OpenAL functions. */

        /* Open and initialize a device */
        alcDevice = alcOpenDevice(nullptr);
        if (!alcDevice) { throw std::runtime_error("Error opening device"); }

        alcContext = alcCreateContext(alcDevice, nullptr);
        if (!alcContext) { throw std::runtime_error("Error creating context"); }

        if (alcMakeContextCurrent(alcContext) == ALC_FALSE) { throw std::runtime_error("Error setting context"); }

        const ALCchar *name = nullptr;
        if (alcIsExtensionPresent(alcDevice, "ALC_ENUMERATE_ALL_EXT"))
            name = alcGetString(alcDevice, ALC_ALL_DEVICES_SPECIFIER);
        if (!name || alcGetError(alcDevice) != AL_NO_ERROR)
            name = alcGetString(alcDevice, ALC_DEVICE_SPECIFIER);
        printf("Opened \"%s\"\n", name);


        // SDL_Sound
        Sound_Init();


        while (audio_core_thread_running) {

            // burn off any errors for new loop
            dump_al_errors();

            float frameMasterVolume;
            std::deque<AudioCoreSlotCommand> frameCommands;
            int frameNextId;

            // READ CONFIG and COMMANDS
            {
                std::lock_guard<std::mutex> lk_ids(ids_mutex_);
                std::lock_guard<std::mutex> lk_config(config_mutex_);

                frameNextId = nextId;

                frameMasterVolume = masterVolumeConfig_;
                frameCommands = std::move(commandList_);
            }


            auto hasCommands = !frameCommands.empty();


            for (auto &cmd : frameCommands) {
                auto &slot = slots_[cmd.handle];
                switch(cmd.command) {
                    case AudioCoreSlotCommandType::Initialise:  slot.Initialise(cmd.handle, cmd.assetPath, cmd.assetExtension, cmd.hackstream, cmd.hacksize); break;
                    case AudioCoreSlotCommandType::Configure: slot.Configure(cmd.repeat, cmd.volume, cmd.speed, cmd.panning); break;
                    case AudioCoreSlotCommandType::Play: slot.Play(); break;
                    case AudioCoreSlotCommandType::Pause: slot.Pause(); break;
                    case AudioCoreSlotCommandType::StopAndRelease: slot.StopAndRelease(); break;
                    case AudioCoreSlotCommandType::Seek: slot.Seek(cmd.seekPosMs); break;
                }
            }


            // global config

            auto currentMasterVolume = 0.1f;
            alGetListenerf(AL_GAIN, &currentMasterVolume);
            dump_al_errors();
            if (currentMasterVolume != frameMasterVolume * 0.7f) {
                alListenerf(AL_GAIN, frameMasterVolume*0.7f);
                dump_al_errors();
            }

            

            // UPDATE all the slots
            for (auto &entry : slots_) {
                auto &slot = entry.second;
                slot.Update();
            }


            // WRITE STATUS
            {
                std::lock_guard<std::mutex> lk(status_mutex_);

                statuses_.clear();
                for (auto &entry : slots_) {
                    auto &slot = entry.second;
                    auto &status = statuses_[slot.handle_];
                    status.playState = slot.playState_;
                    status.positionMs = slot.positionMs_;
                }


            }


            // delete any 
            auto it = slots_.cbegin();
            while (it != slots_.cend())
            {
                auto &slot = it->second;

                if (slot.playState_ == Stopped && slot.released_) {
                    it = slots_.erase(it);
                }
                else {
                    ++it;
                }
            }


            {
                using namespace std::chrono_literals;
                std::unique_lock<std::mutex> lk(config_mutex_);
                config_wakeup_cv_.wait_for(lk, 100ms, [] { return !commandList_.empty(); });
            }
        }

    }
    catch (const std::exception& e) {
        std::cout << "Caught exception \"" << e.what() << "\"\n";
    }
    catch (...) {
        std::cout << "uhoh";
    }


    // SDL_Sound
    Sound_Quit();

    alcMakeContextCurrent(nullptr);
    if(alcContext) {
        alcDestroyContext(alcContext);
        alcContext = nullptr;
    }

    if (alcDevice) {
        alcCloseDevice(alcDevice);
        alcDevice = nullptr;
    }

}
