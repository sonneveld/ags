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
#include <future>

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
// sound caching
// slot id generation id
// pre-determine music sizes
// safer slot look ups (with gen id)
// generate/load mod/midi offsets


namespace ags = AGS::Common;
namespace agsdbg = AGS::Common::Debug;
namespace agseng = AGS::Engine;


const auto SampleDefaultBufferSize = 64 * 1024;
const auto SourceMinimumQueuedBuffers = 2;

const auto GlobalGainScaling = 0.7f;


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


enum AudioCoreSlotCommandType { Nothing, Initialise, Play, Pause, StopAndRelease, Seek};

struct AudioCoreSlotCommand 
{
    int handle = -1;

    AudioCoreSlotCommandType command = AudioCoreSlotCommandType::Nothing;

    std::vector<char> sampledata;
    AGS::Common::String sampleext;

    float seekPosMs = -1.0f;
};



class OpenALDecoder
{
private:
    ALuint source_;

    bool repeat_;

    AudioCorePlayState playState_ = PlayStateInitial;

    std::future<std::vector<char>> sampleBufFuture_ {};
    std::vector<char> sampleData_ {};
    AGS::Common::String sampleExt_ = "";
    ALenum sampleOpenAlFormat_ = 0;
    SoundSampleUniquePtr sample_ = nullptr;

    AudioCorePlayState onLoadPlayState_ = PlayStatePaused;
    float onLoadPositionMs = 0.0f;

    float processedBuffersDurationMs_ = 0.0f;

    bool EOS_ = false;

    static float buffer_duration_ms(ALuint bufferID);
    static ALenum openalFormatFromSample(const SoundSampleUniquePtr &sample);
    void DecoderUnqueueProcessedBuffers();

public:
    OpenALDecoder(ALuint source, std::future<std::vector<char>> sampleBufFuture, AGS::Common::String sampleExt, bool repeat);
    void Poll();
    void Play();
    void Pause();
    void Stop();
    void Seek(float pos_ms);
    AudioCorePlayState GetPlayState();
    float GetPositionMs();
};


struct AudioCoreSlot
{
    AudioCoreSlot(int handle, ALuint source, OpenALDecoder decoder) : handle_(handle), source_(source), decoder_(std::move(decoder)) {}

    int handle_ = -1;
    std::atomic<ALuint> source_ {0};

    OpenALDecoder decoder_;
};



static struct 
{
    ALCdevice *alcDevice = nullptr;
    ALCcontext *alcContext = nullptr;

    std::thread audio_core_thread;
    bool audio_core_thread_running = false;

    // slot ids
    int nextId = 0;

    std::mutex mixer_mutex_m;
    std::condition_variable mixer_cv;
    std::unordered_map<int, std::unique_ptr<AudioCoreSlot>> slots_;

    std::vector<ALuint> freeBuffers_;

} global_;





AGS::Common::String GetFileExtension(AGS::Common::String path)
{
    auto pathComponents = path.Split("\\/");
    if (pathComponents.size() <= 0) { return ""; }
    auto baseName = pathComponents[pathComponents.size() - 1];
    auto i = baseName.FindCharReverse('.');
    if (i < 0) { return ""; }
    return baseName.Mid(i+1);
}


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


// -------------------------------------------------------------------------------------------------
// AUDIO ASSET INFO
// -------------------------------------------------------------------------------------------------

float audio_core_asset_length_ms(const AssetPath &asset_path)
{
    auto &asset_lib = asset_path.first;
    auto &asset_name = asset_path.second;

    WithAssetLibrary w(asset_lib);
    if (!AGS::Common::AssetManager::DoesAssetExist(asset_name))  { return -1; }

    const auto sz = AGS::Common::AssetManager::GetAssetSize(asset_name);
    if (sz <= 0) { return -1; }

    auto extension = GetFileExtension(asset_name);

    std::vector<char> clipData(sz);
    auto s = AGS::Common::AssetManager::OpenAsset(asset_name, AGS::Common::kFile_Open, AGS::Common::kFile_Read);
    if (!s) { return -1; }
    s->Read(clipData.data(), sz);
    delete (s);
    s = nullptr;

    auto sample = SoundSampleUniquePtr(Sound_NewSampleFromMem((Uint8 *)clipData.data(), clipData.size(), extension.GetCStr(), nullptr, 32*1024));
    if (sample == nullptr) { return -1; }

    return Sound_GetDuration(sample.get());
}

int audio_core_asset_sound_type(const AssetPath &asset_path)
{
    auto &asset_name = asset_path.second;

    auto extension = GetFileExtension(asset_name);

    if (extension.CompareNoCase("mp3") == 0) {
        return MUS_MP3;
    } else if (extension.CompareNoCase("ogg") == 0) {
        return MUS_OGG;
    } else if (extension.CompareNoCase("mid") == 0) {
        return MUS_MIDI;
    } else if (extension.CompareNoCase("wav") == 0) {
        return MUS_WAVE;
    } else if (extension.CompareNoCase("voc") == 0) {
        return MUS_WAVE;
    } else if (extension.CompareNoCase("mod") == 0) {
        return MUS_MOD;
    } else if (extension.CompareNoCase("s3m") == 0) {
        return MUS_MOD;
    } else if (extension.CompareNoCase("it") == 0) {
        return MUS_MOD;
    } else if (extension.CompareNoCase("xm") == 0) {
        return MUS_MOD;
    } 

    return -1;
}


// -------------------------------------------------------------------------------------------------
// INIT/SHUTDOWN
// -------------------------------------------------------------------------------------------------

void audio_core_init() 
{
   /* InitAL opens a device and sets up a context using default attributes, making
    * the program ready to call OpenAL functions. */

    /* Open and initialize a device */
    global_.alcDevice = alcOpenDevice(nullptr);
    if (!global_.alcDevice) { throw std::runtime_error("PlayStateError opening device"); }

    global_.alcContext = alcCreateContext(global_.alcDevice, nullptr);
    if (!global_.alcContext) { throw std::runtime_error("PlayStateError creating context"); }

    if (alcMakeContextCurrent(global_.alcContext) == ALC_FALSE) { throw std::runtime_error("PlayStateError setting context"); }

    const ALCchar *name = nullptr;
    if (alcIsExtensionPresent(global_.alcDevice, "ALC_ENUMERATE_ALL_EXT"))
        name = alcGetString(global_.alcDevice, ALC_ALL_DEVICES_SPECIFIER);
    if (!name || alcGetError(global_.alcDevice) != AL_NO_ERROR)
        name = alcGetString(global_.alcDevice, ALC_DEVICE_SPECIFIER);
    printf("Opened \"%s\"\n", name);


    // SDL_Sound
    Sound_Init();


    global_.audio_core_thread_running = true;
    global_.audio_core_thread = std::thread(audio_core_entry);
}

void audio_core_shutdown()
{
    global_.audio_core_thread_running = false;
    global_.audio_core_thread.join();

    // SDL_Sound
    Sound_Quit();

    alcMakeContextCurrent(nullptr);
    if(global_.alcContext) {
        alcDestroyContext(global_.alcContext);
        global_.alcContext = nullptr;
    }

    if (global_.alcDevice) {
        alcCloseDevice(global_.alcDevice);
        global_.alcDevice = nullptr;
    }
}


// -------------------------------------------------------------------------------------------------
// SLOTS
// -------------------------------------------------------------------------------------------------

static int avail_slot_id()
{
    auto result = global_.nextId;
    global_.nextId += 1;
    return result;
}

// if none free, add a new one.
int audio_core_slot_initialise(const AssetPath &asset_path, bool repeat)  
{
    auto &asset_lib = asset_path.first;
    auto &asset_name = asset_path.second;

    WithAssetLibrary w(asset_lib);
    if (!AGS::Common::AssetManager::DoesAssetExist(asset_name))  { return -1; }

    const auto sz = AGS::Common::AssetManager::GetAssetSize(asset_name);
    if (sz <= 0) { return -1; }

    auto extension = GetFileExtension(asset_name);

    std::vector<char> clipData(sz);
    auto s = AGS::Common::AssetManager::OpenAsset(asset_name, AGS::Common::kFile_Open, AGS::Common::kFile_Read);
    if (!s) { return -1; }
    s->Read(clipData.data(), sz);
    delete (s);
    s = nullptr;

    auto handle = avail_slot_id();

    ALuint source_;
    // new source!
    alGenSources(1, &source_);
    dump_al_errors();

    auto promise = std::promise<std::vector<char>>();
    promise.set_value(std::move(clipData));

    auto decoder = OpenALDecoder(source_, promise.get_future(), extension, repeat);

    std::lock_guard<std::mutex> lk(global_.mixer_mutex_m);
    global_.slots_[handle] = std::make_unique<AudioCoreSlot>(handle, source_, std::move(decoder));
    global_.mixer_cv.notify_all();

    return handle;
}


// -------------------------------------------------------------------------------------------------
// CONFIG
// -------------------------------------------------------------------------------------------------

void audio_core_set_master_volume(float newvol) 
{
    alListenerf(AL_GAIN, newvol*GlobalGainScaling);
    dump_al_errors();
}

void audio_core_slot_configure(int slot_handle, float volume, float speed, float panning)
{
    ALuint source_ = global_.slots_[slot_handle]->source_;

    alSourcef(source_, AL_GAIN, volume*0.7f);
    dump_al_errors();

    alSourcef(source_, AL_PITCH, speed);
    dump_al_errors();

    if (panning != 0.0f) {
        // https://github.com/kcat/openal-soft/issues/194
        alSourcef(source_, AL_ROLLOFF_FACTOR, 0.0f);
        dump_al_errors();
        alSourcei(source_, AL_SOURCE_RELATIVE, AL_TRUE);
        dump_al_errors();
        alSource3f(source_, AL_POSITION, panning, 0.0f, 0.0f);
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

void audio_core_slot_play(int slot_handle)
{
    std::lock_guard<std::mutex> lk(global_.mixer_mutex_m);
    global_.slots_[slot_handle]->decoder_.Play();
    global_.mixer_cv.notify_all();
}

void audio_core_slot_pause(int slot_handle)
{
    std::lock_guard<std::mutex> lk(global_.mixer_mutex_m);
    global_.slots_[slot_handle]->decoder_.Pause();
    global_.mixer_cv.notify_all();
}

void audio_core_slot_stop(int slot_handle)
{
    std::lock_guard<std::mutex> lk(global_.mixer_mutex_m);
    global_.slots_[slot_handle]->decoder_.Stop();
    global_.mixer_cv.notify_all();
}

void audio_core_slot_seek_ms(int slot_handle, float pos_ms)
{
    std::lock_guard<std::mutex> lk(global_.mixer_mutex_m);
    global_.slots_[slot_handle]->decoder_.Seek(pos_ms);
    global_.mixer_cv.notify_all();
}



// -------------------------------------------------------------------------------------------------
// STATUS
// -------------------------------------------------------------------------------------------------

float audio_core_slot_get_pos_ms(int slot_handle)
{
    std::lock_guard<std::mutex> lk(global_.mixer_mutex_m);
    return global_.slots_[slot_handle]->decoder_.GetPositionMs();
    global_.mixer_cv.notify_all();
}
AudioCorePlayState audio_core_slot_get_play_state(int slot_handle)
{
    std::lock_guard<std::mutex> lk(global_.mixer_mutex_m);
    return global_.slots_[slot_handle]->decoder_.GetPlayState();
    global_.mixer_cv.notify_all();
}


// -------------------------------------------------------------------------------------------------
// AUDIO PROCESSING
// -------------------------------------------------------------------------------------------------


static void audio_core_entry()
{
    std::unique_lock<std::mutex> lk(global_.mixer_mutex_m);

    while (global_.audio_core_thread_running) {
        
        // burn off any errors for new loop
        dump_al_errors();

        for (auto &entry : global_.slots_) {
            auto &slot = entry.second;

            try {
                slot->decoder_.Poll();
            } catch (const std::exception& e) {
                std::cout << "Caught exception \"" << e.what() << "\"\n";
            }
        }

        global_.mixer_cv.wait_for(lk, std::chrono::milliseconds(50));
    }
}


// -------------------------------------------------------------------------------------------------
// DECODER
// -------------------------------------------------------------------------------------------------


float OpenALDecoder::buffer_duration_ms(ALuint bufferID) {
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


ALenum OpenALDecoder::openalFormatFromSample(const SoundSampleUniquePtr &sample) {
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


void OpenALDecoder::DecoderUnqueueProcessedBuffers() 
{
    for (;;) {
        ALint buffersProcessed = -1;
        alGetSourcei(source_, AL_BUFFERS_PROCESSED, &buffersProcessed);
        dump_al_errors();
        if (buffersProcessed <= 0) { break; }

        ALuint b;
        alSourceUnqueueBuffers(source_, 1, &b);
        dump_al_errors();

        processedBuffersDurationMs_ += buffer_duration_ms(b);

        global_.freeBuffers_.push_back(b);
    }
}



OpenALDecoder::OpenALDecoder(ALuint source, std::future<std::vector<char>> sampleBufFuture, AGS::Common::String sampleExt, bool repeat)
    : source_(source), sampleBufFuture_(std::move(sampleBufFuture)), sampleExt_(sampleExt), repeat_(repeat) {

}

void OpenALDecoder::Poll()
{
    if (playState_ == AudioCorePlayState::PlayStateError) { return; }

    if (playState_ == AudioCorePlayState::PlayStateInitial) {

        if (sampleBufFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) { return; }

        sampleData_ = std::move(sampleBufFuture_.get());

        auto sample = SoundSampleUniquePtr(Sound_NewSampleFromMem((uint8_t *)sampleData_.data(), sampleData_.size(), sampleExt_.GetCStr(), nullptr, SampleDefaultBufferSize));
        if(!sample) { playState_ = AudioCorePlayState::PlayStateError; return; }

        auto bufferFormat = openalFormatFromSample(sample);

        if (bufferFormat <= 0) {
        #ifdef AUDIO_CORE_DEBUG
            agsdbg::Printf(ags::kDbgMsg_Init, "audio_core_sample_load: RESAMPLING");
        #endif
            auto desired = Sound_AudioInfo { AUDIO_S16SYS, sample->actual.channels, sample->actual.rate };

            Sound_FreeSample(sample.get());
            sample = SoundSampleUniquePtr(Sound_NewSampleFromMem((uint8_t *)sampleData_.data(), sampleData_.size(), sampleExt_.GetCStr(), &desired, SampleDefaultBufferSize));

            if(!sample) { playState_ = AudioCorePlayState::PlayStateError; return; }

            bufferFormat = openalFormatFromSample(sample);
        }

        if (bufferFormat <= 0) { playState_ = AudioCorePlayState::PlayStateError; return; }

        sample_ = std::move(sample);
        sampleOpenAlFormat_ = bufferFormat;

        playState_ = onLoadPlayState_;
        if (onLoadPositionMs >= 0.0f) {
            Seek(onLoadPositionMs);
        }

    }


    if (playState_ != PlayStatePlaying) { return; }

    // buffer management
    DecoderUnqueueProcessedBuffers();

    // generate extra buffers if none are free
    if (global_.freeBuffers_.size() < SourceMinimumQueuedBuffers) {
        std::array<ALuint, SourceMinimumQueuedBuffers> genbufs;
        alGenBuffers(genbufs.size(), genbufs.data());
        dump_al_errors();
        for (const auto &b : genbufs) {
            global_.freeBuffers_.push_back(b);
        }
    }

    // decode and attach buffers
    while (!EOS_) {
        ALint buffersQueued = -1;
        alGetSourcei(source_, AL_BUFFERS_QUEUED, &buffersQueued);
        dump_al_errors();

        if (buffersQueued >= SourceMinimumQueuedBuffers) { break; }

        auto it = std::prev(global_.freeBuffers_.end());
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

        global_.freeBuffers_.erase(it);
    }


    // setup play state

    ALint state = AL_INITIAL;
    alGetSourcei(source_, AL_SOURCE_STATE, &state);
    dump_al_errors();

    if (state != AL_PLAYING) {
        alSourcePlay(source_);
        dump_al_errors();
    }

    // if end of stream and still not playing, we done here.
    ALint buffersRemaining = 0;
    alGetSourcei(source_, AL_BUFFERS_QUEUED, &buffersRemaining);
    dump_al_errors();
    if (EOS_ && buffersRemaining <= 0) {
        playState_ = PlayStateFinished;
    }

}

void OpenALDecoder::Play() 
{
    switch (playState_) {
        case PlayStateError:
            break;
        case PlayStateInitial:
            onLoadPlayState_ = PlayStatePlaying;
            break;
        case PlayStateStopped:
            Seek(0.0f);
        case PlayStatePaused:
            playState_ = AudioCorePlayState::PlayStatePlaying;
            alSourcePlay(source_);
            dump_al_errors();
            break;
        default:
            break;
    }
}

void OpenALDecoder::Pause()
{
    switch (playState_) {
        case PlayStateError:
            break;
        case PlayStateInitial:
            onLoadPlayState_ = PlayStatePaused;
            break;
        case PlayStatePlaying:
            playState_ = AudioCorePlayState::PlayStatePaused;
            alSourcePause(source_);
            dump_al_errors();
            break;
        default:
            break;
    }
}

void OpenALDecoder::Stop()
{
    switch (playState_) {
        case PlayStateError:
            break;
        case PlayStateInitial:
            onLoadPlayState_ = PlayStateStopped;
            break;
        case PlayStatePlaying:
            playState_ = AudioCorePlayState::PlayStateStopped;
            alSourceStop(source_);
            dump_al_errors();
            break;
        default:
            break;
    }
}

void OpenALDecoder::Seek(float pos_ms)
{
    switch (playState_) {
        case PlayStateError:
            break;
        case PlayStateInitial:
            onLoadPositionMs = pos_ms;
            break;
        default:
            alSourceStop(source_);
            dump_al_errors();
            DecoderUnqueueProcessedBuffers();
            Sound_Seek(sample_.get(), pos_ms);
            processedBuffersDurationMs_ = pos_ms;
            // will play when buffers are added in poll
    }
}

AudioCorePlayState OpenALDecoder::GetPlayState()
{
    return playState_;
}

float OpenALDecoder::GetPositionMs()
{
    float alSecOffset = 0.0f;
    // disabled with mojoal
    // if source available:
    alGetSourcef(source_, AL_SEC_OFFSET, &alSecOffset);
    dump_al_errors();
    auto positionMs_ = processedBuffersDurationMs_ + alSecOffset*1000.0f;
#ifdef AUDIO_CORE_DEBUG
    printf("proc:%f plus:%f = %f\n", processedBuffersDurationMs_, alSecOffset*1000.0f, positionMs_);
#endif
    return positionMs_;
}

