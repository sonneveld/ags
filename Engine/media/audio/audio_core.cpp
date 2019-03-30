#include <unordered_map>
#include <map>
#include <list>

#ifdef WINDOWS_VERSION
#include "SDL.h"
#else
#include "SDL2/SDL.h"
#endif
#include "SDL_sound.h"

#include "media/audio/openal.h"

#include "allegro.h"

#include "ac/asset_helper.h"
#include "media/audio/audiodefines.h"
#include "debug/out.h"
#include "ac/common.h"

//         No = 0,
//         SlowFade = 1,
//         SlowishFade = 2,
//         MediumFade = 3,
//         FastFade = 4

// #define AUDIO_CORE_DEBUG

static int core_sample_next_handle = 1000;
static int core_channel_next_handle = 2000;

/* InitAL opens a device and sets up a context using default attributes, making
 * the program ready to call OpenAL functions. */
static int InitAL()
{
    const ALCchar *name;
    ALCdevice *device;
    ALCcontext *ctx;

    /* Open and initialize a device */
    device = NULL;
    if(!device)
        device = alcOpenDevice(NULL);
    if(!device)
    {
        fprintf(stderr, "Could not open a device!\n");
        return 1;
    }

    ctx = alcCreateContext(device, NULL);
    if(ctx == NULL || alcMakeContextCurrent(ctx) == ALC_FALSE)
    {
        if(ctx != NULL)
            alcDestroyContext(ctx);
        alcCloseDevice(device);
        fprintf(stderr, "Could not set a context!\n");
        return 1;
    }

    name = NULL;
    if(alcIsExtensionPresent(device, "ALC_ENUMERATE_ALL_EXT"))
        name = alcGetString(device, ALC_ALL_DEVICES_SPECIFIER);
    if(!name || alcGetError(device) != AL_NO_ERROR)
        name = alcGetString(device, ALC_DEVICE_SPECIFIER);
    printf("Opened \"%s\"\n", name);

    return 0;
}


/* CloseAL closes the device belonging to the current context, and destroys the
 * context. */
static void CloseAL(void)
{
    ALCdevice *device;
    ALCcontext *ctx;

    ctx = alcGetCurrentContext();
    if(ctx == NULL)
        return;

    device = alcGetContextsDevice(ctx);

    alcMakeContextCurrent(NULL);
    alcDestroyContext(ctx);
    alcCloseDevice(device);
}


void audio_core_init() 
{
    if (InitAL()) {
        quit("Could not initialise audio.");
    }

    Sound_Init();
}

void audio_core_shutdown()
{
    Sound_Quit();
    CloseAL();
}


void audio_core_set_master_volume(float newvol) {
  alListenerf(AL_GAIN, newvol);
}


#if 0
static Sint64 mysizefunc(SDL_RWops * context)
{
  return (long)context->hidden.unknown.data2;
}

static Sint64 myseekfunc(SDL_RWops *context, Sint64 offset, int whence)
{
  return SDL_SetError("Can't seek in this kind of SDL_RWops");
}

static size_t myreadfunc(SDL_RWops *context, void *ptr, size_t size, size_t maxnum)
{
    auto bytes = pack_fread(ptr, size*maxnum, (PACKFILE*)context->hidden.unknown.data1);
    return bytes/size;
}

static size_t mywritefunc(SDL_RWops *context, const void *ptr, size_t size, size_t num)
{
  return 0;
}

static int myclosefunc(SDL_RWops *context)
{
  if(context->type != SDL_RWOPS_UNKNOWN)
  {
    return SDL_SetError("Wrong kind of SDL_RWops for myclosefunc()");
  }
    pack_fclose((PACKFILE*)context->hidden.unknown.data1);
  SDL_FreeRW(context);
  return 0;
}
#endif

ALenum openalFormatFromSample(Sound_Sample *sample) {
  if (sample->actual.channels == 1) {
    switch (sample->actual.format) {
      case AUDIO_U8:
        return AL_FORMAT_MONO8;
      case AUDIO_S16SYS:
        return AL_FORMAT_MONO16;
      case AUDIO_F32SYS:
        if (alIsExtensionPresent("AL_EXT_float32")) {
          return alGetEnumValue("AL_FORMAT_MONO_FLOAT32");
        }
    }
  } else if (sample->actual.channels == 2) {
    switch (sample->actual.format) {
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

  AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init, "openalFormatFromSample: bad format chan:%d format: %d", sample->actual.channels, sample->actual.format);

  return 0;
}


struct AudioCoreSample {
  int handle;
  AssetPath asset_name;
  ALuint buffer;
  float duration_ms;
};

static const int samples_soft_max = 16;

std::list<AudioCoreSample> samples {};
std::unordered_map<int, std::list<AudioCoreSample>::iterator > sample_by_id {};
std::map<AssetPath, std::list<AudioCoreSample>::iterator > sample_by_asset_name {};

// https://www.geeksforgeeks.org/lru-cache-implementation/
static void use_sample(const AudioCoreSample &sample)
{
    // not present in cache 
    if (sample_by_id.find(sample.handle) == sample_by_id.end()) 
    { 
        // cache is full 
        if (samples.size() >= samples_soft_max) 
        { 
            //delete least recently used element (and deletable)
            auto rit = samples.end();
            do {
              rit--;
              auto sampToRemove = *rit;
              alDeleteBuffers(1, &sampToRemove.buffer);
              auto err = alGetError();
              if (err == AL_NO_ERROR) {
                samples.erase(rit);
                sample_by_id.erase(sampToRemove.handle); 
                sample_by_asset_name.erase(sampToRemove.asset_name); 
                break;
              }
            } while (rit != samples.begin());
        } 
    } 
    // present in cache 
    else
        samples.erase(sample_by_id[sample.handle]); 
  
    // update reference 
    samples.push_front(sample); 
    sample_by_id[sample.handle] = samples.begin(); 
    sample_by_asset_name[sample.asset_name] = samples.begin(); 
}

int audio_core_sample_load(const AssetPath asset_name, const char *extension) {

  auto search = sample_by_asset_name.find(asset_name);
  if (search != sample_by_asset_name.end()) {
    auto samp = *search->second;
    use_sample(samp);
#ifdef AUDIO_CORE_DEBUG
    AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init, "audio_core_sample_load: name:%s core-sample:%d al-buffer:%d (CACHE HIT)", 
      samp.asset_name.second.GetCStr(), samp.handle, samp.buffer);
#endif
    return samp.handle;
  }

  auto in = PackfileFromAsset(asset_name);
  if (!in) { 
    AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init, "audio_core_sample_load: not found: %s", asset_name.second.GetCStr());
    return -1; 
  }

  auto size = in->normal.todo;
  auto data = (Uint8 *)malloc(size);
  int read = pack_fread(data, size, in);

  pack_fclose(in);


#if 0
  auto rw = SDL_RWFromConstMem(buf, assetSize);

  auto rw = SDL_AllocRW();
  rw->type = SDL_RWOPS_UNKNOWN;
  rw->size = mysizefunc;
  rw->seek = myseekfunc;
  rw->read = myreadfunc;
  rw->write = mywritefunc;
  rw->close = myclosefunc;
  rw->hidden.unknown.data1 = (void *)in;
  rw->hidden.unknown.data2 = (void*)in->normal.todo;
#endif

  auto sample =  Sound_NewSampleFromMem(data, size, extension, nullptr, 64*1024);

  if(!sample) { 
    AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init, "audio_core_sample_load: Sound_NewSample: error: %s", SDL_GetError());
    Sound_FreeSample(sample);
    return -1; 
  }

  auto bufferFormat = openalFormatFromSample(sample);

  if (bufferFormat <= 0) {
#ifdef AUDIO_CORE_DEBUG
    AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init, "audio_core_sample_load: RESAMPLING");
#endif
    auto desired = Sound_AudioInfo {
      AUDIO_S16SYS, sample->actual.channels, sample->actual.rate
    };
    Sound_FreeSample(sample);
    sample = Sound_NewSampleFromMem(data, size, extension, &desired, 64*1024);

    if(!sample) { 
      AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init, "audio_core_sample_load: Sound_NewSample: error: %s", SDL_GetError());
      Sound_FreeSample(sample);
      return -1; 
    }

    bufferFormat = openalFormatFromSample(sample);
  }

  if (bufferFormat <= 0) {
    quit("could not convert to supported audio buffer format.");
    return -1;
  }

  auto slen = Sound_DecodeAll(sample);
  if(!sample->buffer || slen == 0)
  {
      AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init, "audio_core_sample_load: Sound_DecodeAll: error: %s", SDL_GetError());
      Sound_FreeSample(sample);
      return -1;
  }

  auto duration_ms = 1000.0f * slen * 8 / SDL_AUDIO_BITSIZE(sample->actual.format) / sample->actual.rate / sample->actual.channels;

  ALuint buffer = 0;
  alGenBuffers(1, &buffer);
  alBufferData(buffer, bufferFormat, sample->buffer, slen, sample->actual.rate);
  Sound_FreeSample(sample);

  auto handle = core_sample_next_handle++;
  use_sample({handle, asset_name, buffer, duration_ms});

  int totalBytes = 0;
  for (auto s : samples) {
    ALint bufSizeBytes = 0;
		alGetBufferi(s.buffer, AL_SIZE, &bufSizeBytes);
    totalBytes += bufSizeBytes;
  }

#ifdef AUDIO_CORE_DEBUG
  AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init, "audio_core_sample_load: name:%s duration:%fms size:%d bytes (total:%d bytes) core-sample:%d al-buffer:%d",
    asset_name.second.GetCStr(), duration_ms, slen, totalBytes, handle, buffer);
#endif

  return handle;
}

int audio_core_sample_get_length_ms(int core_sample) 
{
  return sample_by_id[core_sample]->duration_ms;  
}

int audio_core_sample_get_sound_type(int core_sample) 
{ 
  return MUS_OGG; 
}



struct AudioCoreChannel {
  int handle;
  ALuint source;
};

std::unordered_map<int, AudioCoreChannel> channel_by_id {};

int audio_core_channel_allocate() { 
  ALuint source = 0;
  alGenSources((ALuint)1, &source);

  auto err = alGetError();
  if (err!=AL_NO_ERROR) {
    AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init, "audio_core_channel_allocate: error: %s", alGetString(err));
    return -1;
  }

  auto handle = core_channel_next_handle++;
  channel_by_id.insert( {handle, {handle, source}});

#ifdef AUDIO_CORE_DEBUG
  AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init, "audio_core_channel_allocate: source %d ", source);
#endif

  return handle;
}

void audio_core_channel_delete(int core_channel) {
  auto source = channel_by_id[core_channel].source;
  alSourceStop(source);
  alDeleteSources(1, &source);
  channel_by_id.erase(core_channel);

#ifdef AUDIO_CORE_DEBUG
  AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init, "audio_core_channel_delete: source %d ", source);
#endif
}

int audio_core_channel_play_from(int core_channel, int core_sample, int position, bool repeat, float volume) { 
  auto source = channel_by_id[core_channel].source;
  auto buffer = sample_by_id[core_sample]->buffer;

  alSourcei(source, AL_LOOPING, repeat ? AL_TRUE : AL_FALSE);
  alSourcef(source, AL_GAIN, volume);
  alSourcei(source, AL_BUFFER, buffer);
  alSourcePlay(source);

  auto err = alGetError();
  if (err!=AL_NO_ERROR) {
    AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init, "audio_core_channel_play_from: error: %s", alGetString(err));
    return -1;
  }

#ifdef AUDIO_CORE_DEBUG
  AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init, "audio_core_channel_play_from: core-channel %d core-sample: %d al-source %d al-buffer: %d", core_channel, core_sample, source, buffer);
#endif

  return 0; 
}

/*
int channel_queue_music(sample)  (how does repeat interact?)
*/

void audio_core_channel_seek(int core_channel, int pos) 
{ 
  auto source = channel_by_id[core_channel].source;
  alSourcei(source, AL_SAMPLE_OFFSET, pos);

#ifdef AUDIO_CORE_DEBUG
  AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init, "audio_core_channel_seek: source %d pos %d", source, pos);
#endif
}

void audio_core_channel_pause(int core_channel) 
{
  auto source = channel_by_id[core_channel].source;
  ALint state = AL_INITIAL;
  alGetSourcei(source,AL_SOURCE_STATE,&state);

  auto err = alGetError();
  if (err!=AL_NO_ERROR) {
    return;
  }

  if (state != AL_PLAYING) { return; }

  alSourcePause(source);

#ifdef AUDIO_CORE_DEBUG
  AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init, "audio_core_channel_pause: source %d", source);
#endif
}

void audio_core_channel_resume(int core_channel)
{
  auto source = channel_by_id[core_channel].source;
  ALint state = AL_INITIAL;
  alGetSourcei(source,AL_SOURCE_STATE,&state);

  auto err = alGetError();
  if (err!=AL_NO_ERROR) {
    return;
  }

  if (state != AL_PAUSED) { return; }

  alSourcePlay(source);

#ifdef AUDIO_CORE_DEBUG
  AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init, "audio_core_channel_resume: source %d", source);
#endif
}

void audio_core_channel_stop(int core_channel) 
{ 
  auto source = channel_by_id[core_channel].source;
  alSourceStop(source);

  auto err = alGetError();
  if (err!=AL_NO_ERROR) {
    AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init, "audio_core_channel_stop: error: %s", alGetString(err));
    return;
  }

#ifdef AUDIO_CORE_DEBUG
  AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init, "audio_core_channel_stop: source %d ", source);
#endif
}

int audio_core_channel_get_pos(int core_channel) 
{ 
  auto source = channel_by_id[core_channel].source;
  ALint sampoffset = 0;
  alGetSourcei( source, AL_SAMPLE_OFFSET, &sampoffset );
  auto err = alGetError();
  if (err!=AL_NO_ERROR) {
    return -1;
  }
  return sampoffset;
}

int audio_core_channel_get_pos_ms(int core_channel) 
{ 
  auto source = channel_by_id[core_channel].source;
  ALfloat secoffset = 0;
  alGetSourcef( source, AL_SEC_OFFSET, &secoffset );
  auto err = alGetError();
  if (err!=AL_NO_ERROR) {
    return -1;
  }
  return secoffset * 1000.0f;
}

bool audio_core_is_channel_active(int core_channel) 
{ 
  auto source = channel_by_id[core_channel].source;
  ALint state = AL_INITIAL;
  alGetSourcei(source,AL_SOURCE_STATE,&state);
  auto err = alGetError();
  if (err!=AL_NO_ERROR) {
    return false;
  }
  return state == AL_PLAYING || state == AL_PAUSED;
 }

void audio_core_channel_set_volume(int core_channel, float volume) 
{
  auto source = channel_by_id[core_channel].source;
  alSourcef(source, AL_GAIN, volume);
}

void audio_core_channel_set_speed(int core_channel, float speed) 
{
  auto source = channel_by_id[core_channel].source;
  alSourcef(source, AL_PITCH, speed);
}

void audio_core_channel_set_panning(int core_channel, float panning) 
{
  auto source = channel_by_id[core_channel].source;
  if (panning != 0.0f) {
    // https://github.com/kcat/openal-soft/issues/194
    alSourcef(source, AL_ROLLOFF_FACTOR, 0.0f);
    alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
    alSource3f(source, AL_POSITION, panning, 0.0f, 0.0f);
#ifdef AUDIO_CORE_DEBUG
    AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init, "audio_core_channel_set_panning: source: %d panning: %f", source, panning);
#endif
  } else {
    alSourcef(source, AL_ROLLOFF_FACTOR, 1.0f);
    alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE);
    alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
#ifdef AUDIO_CORE_DEBUG
    AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init, "audio_core_channel_set_panning: source: %d panning: CENTRE", source);
#endif
  }
}


void audio_core_set_midi_pos(int position)
{
  quit("audio_core_set_midi_pos not supported");
}
int audio_core_get_midi_pos()
{
  quit("audio_core_get_midi_pos not supported");
  return -1;
}
