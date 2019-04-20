/*
Simple DirectMedia Layer
Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#include "../../SDL_internal.h"

#if SDL_AUDIO_DRIVER_NINTENDO_SWITCH

#include <nn/audio.h>

extern "C" {
#include "SDL_audio.h"
#include "SDL_timer.h"
#include "../SDL_audio_c.h"
#include "../SDL_sysaudio.h"
#include "SDL_assert.h"
#include "SDL_log.h"
#include "SDL_nintendoswitch_audio.h"
}

static Uint8 *
NINTENDOSWITCHAUDIO_GetDeviceBuf(_THIS)
{
    SDL_assert(_this->hidden->nextAudioBuffer != NULL);
    return (Uint8 *) nn::audio::GetAudioOutBufferDataPointer(_this->hidden->nextAudioBuffer);
}

static void
NINTENDOSWITCHAUDIO_PlayDevice(_THIS)
{
    nn::audio::AppendAudioOutBuffer(&_this->hidden->audioOut, _this->hidden->nextAudioBuffer);
}

static void
NINTENDOSWITCHAUDIO_WaitDevice(_THIS)
{
    /* wait until we have an audio buffer to fill in. */
    while ((_this->hidden->nextAudioBuffer = nn::audio::GetReleasedAudioOutBuffer(&_this->hidden->audioOut)) == NULL) {
        if (!SDL_AtomicGet(&_this->enabled)) {
            return;  /* oh well. */
        }
        _this->hidden->event.TimedWait(nn::TimeSpan::FromMilliSeconds(_this->hidden->maxWait));
    }
}

static int
NINTENDOSWITCHAUDIO_CaptureFromDevice(_THIS, void *buffer, int buflen)
{
    if (_this->hidden->remainingDataLen > 0) {
        const int cpy = SDL_min(buflen, _this->hidden->remainingDataLen);
        SDL_memcpy(buffer, _this->hidden->remainingData, cpy);
        _this->hidden->remainingData += cpy;
        _this->hidden->remainingDataLen -= cpy;
        return cpy;
    }

    /* need to get more from the hardware. Block if necessary. */
    nn::audio::AudioInBuffer *audiobuf;
    while ((audiobuf = nn::audio::GetReleasedAudioInBuffer(&_this->hidden->audioIn)) == NULL) {
        if (!SDL_AtomicGet(&_this->enabled)) {
            return 0;  /* oh well. */
        }
        _this->hidden->event.TimedWait(nn::TimeSpan::FromMilliSeconds(_this->hidden->maxWait));
    }

    /* we have a new buffer. Eat what we can, store off the rest.*/
    const Uint8 *data = (const Uint8 *) nn::audio::GetAudioInBufferDataPointer(audiobuf);
    const int datalen = (int) nn::audio::GetAudioInBufferDataSize(audiobuf);
    const int cpy = SDL_min(buflen, datalen);
    SDL_assert(cpy > 0);
    SDL_memcpy(buffer, data, cpy);

    const int leftover = datalen - cpy;
    _this->hidden->remainingDataLen = leftover;
    SDL_assert(_this->hidden->remainingDataLen <= _this->hidden->remainingAllocLen);
    if (leftover > 0) {
        SDL_memcpy(_this->hidden->remainingData, data + cpy, leftover);
    }

    nn::audio::AppendAudioInBuffer(&_this->hidden->audioIn, audiobuf);  /* requeue it! */

    return cpy;
}

static void
NINTENDOSWITCHAUDIO_FlushCapture(_THIS)
{
    nn::audio::AudioInBuffer *audiobuf;
    while ((audiobuf = nn::audio::GetReleasedAudioInBuffer(&_this->hidden->audioIn)) != NULL) {
        // just requeue it immediately without reading the data.
        nn::audio::AppendAudioInBuffer(&_this->hidden->audioIn, audiobuf);
    }
}

static void
NINTENDOSWITCHAUDIO_CloseDevice(_THIS)
{
    #define CLOSE_AUDIO_DEVICE(typ) { \
        if (_this->hidden->started) { \
            nn::audio::StopAudio##typ(&_this->hidden->audio##typ); \
        } \
        if (_this->hidden->opened) { \
            nn::audio::CloseAudio##typ(&_this->hidden->audio##typ); \
        } \
    }

    if (_this->iscapture) {
        CLOSE_AUDIO_DEVICE(In);
    } else {
        CLOSE_AUDIO_DEVICE(Out);
    }

    #undef CLOSE_AUDIO_DEVICE

    if (_this->hidden->opened) {
        nn::os::DestroySystemEvent(_this->hidden->event.GetBase());
    }

    for (int i = 0; i < SDL_arraysize(_this->hidden->buffer); i++) {
        free(_this->hidden->buffer[i]); /* must be free, not SDL_free; we used posix_memalign. */
    }

    SDL_free(_this->hidden->remainingData);

    SDL_free(_this->hidden);
}

static int
NINTENDOSWITCHAUDIO_OpenDevice(_THIS, void *handle, const char *devname, int iscapture)
{
    /* Initialize all variables that we clean on shutdown */
    _this->hidden = (struct SDL_PrivateAudioData *)
        SDL_malloc((sizeof *_this->hidden));
    if (_this->hidden == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_zerop(_this->hidden);

    // this is a little obnoxious.
    #define OPEN_AUDIO_DEVICE(typ, queuecount, setparamcode) { \
        nn::audio::Audio##typ##Parameter params; \
        nn::audio::InitializeAudio##typ##Parameter(&params); \
        setparamcode \
        nn::audio::SampleFormat fmt = nn::audio::SampleFormat_Invalid; \
        nn::Result rc; \
        if (handle) { \
            rc = nn::audio::OpenAudio##typ(&_this->hidden->audio##typ, &_this->hidden->event, (const char *) handle, params); \
        } else { \
            rc = nn::audio::OpenDefaultAudio##typ(&_this->hidden->audio##typ, &_this->hidden->event, params); \
        } \
        if (rc.IsFailure()) { \
            return SDL_SetError("Open %s device '%s' failed: err=%d", iscapture ? "capture" : "playback", handle ? (const char *) handle : "[default]", rc.GetDescription()); \
        } \
        _this->hidden->opened = SDL_TRUE; \
        fmt = nn::audio::GetAudio##typ##SampleFormat(&_this->hidden->audio##typ); \
        switch (fmt) { \
            case nn::audio::SampleFormat_PcmInt16: _this->spec.format = AUDIO_S16SYS; break; \
            case nn::audio::SampleFormat_PcmInt32: _this->spec.format = AUDIO_S32SYS; break; \
            case nn::audio::SampleFormat_PcmFloat: _this->spec.format = AUDIO_F32SYS; break; \
            default: return SDL_SetError("Device's format is unsupported"); \
        } \
        _this->spec.channels = nn::audio::GetAudio##typ##ChannelCount(&_this->hidden->audio##typ); \
        _this->spec.freq = nn::audio::GetAudio##typ##SampleRate(&_this->hidden->audio##typ); \
        SDL_CalculateAudioSpec(&_this->spec); \
        _this->hidden->maxWait = (int64_t) ((_this->spec.samples * 1000) / _this->spec.freq); \
        const size_t datalen = _this->spec.samples * _this->spec.channels * nn::audio::GetSampleByteSize(fmt); \
        const size_t bufferlen = nn::util::align_up(datalen, nn::audio::Audio##typ##Buffer::SizeGranularity); \
        for (int i = 0; i < SDL_arraysize(_this->hidden->buffer); i++) { \
            if (posix_memalign(&_this->hidden->buffer[i], nn::audio::Audio##typ##Buffer::AddressAlignment, bufferlen) != 0) { \
                return SDL_OutOfMemory(); \
            } \
            SDL_memset(_this->hidden->buffer[i], _this->spec.silence, bufferlen); \
            nn::audio::SetAudio##typ##BufferInfo(&_this->hidden->audioBuffer##typ[i], _this->hidden->buffer[i], bufferlen, datalen); \
        } \
        for (int i = 0; i < (queuecount); i++) { \
            nn::audio::AppendAudio##typ##Buffer(&_this->hidden->audio##typ, &_this->hidden->audioBuffer##typ[i]); \
        } \
        _this->hidden->remainingAllocLen = bufferlen; \
        if (_this->iscapture) { \
            if ((_this->hidden->remainingData = (Uint8 *) SDL_malloc(bufferlen)) == NULL) { \
                return SDL_OutOfMemory(); \
            } \
        } \
        rc = nn::audio::StartAudio##typ(&_this->hidden->audio##typ); \
        if (rc.IsFailure()) { \
            return SDL_SetError("Start %s device '%s' failed: err=%d", iscapture ? "capture" : "playback", handle ? (const char *)handle : "[default]", rc.GetDescription()); \
        } \
        _this->hidden->started = SDL_TRUE; \
    }

    if (iscapture) {
        OPEN_AUDIO_DEVICE(In, SDL_arraysize(_this->hidden->audioBufferIn), {});
    } else {
        const int chans = (_this->spec.channels > 2) ? 6 : 0;  /* Switch only supports 2 and 6 channels (zero==default). */
        const int numToQueue = SDL_arraysize(_this->hidden->audioBufferOut) - 1;
        OPEN_AUDIO_DEVICE(Out, numToQueue, { params.channelCount = chans; });
        /* this will queue on first GetDeviceBuf, and then cycle with the others. */
        _this->hidden->nextAudioBuffer = &_this->hidden->audioBufferOut[numToQueue];
    }

    #undef OPEN_AUDIO_DEVICE

    return 0;  /* good to go. */
}

static void
NINTENDOSWITCHAUDIO_DetectDevices(void)
{
    #define LIST_AUDIO_DEVICES(typ, maxdevs, iscapture) { \
        nn::audio::Audio##typ##Info info[maxdevs]; \
        const int numdevs = nn::audio::ListAudio##typ##s(info, SDL_arraysize(info)); \
        for (int i = 0; i < numdevs; i++) { \
            char *handle = SDL_strdup(info[i].name); \
            if (handle) { \
                SDL_AddAudioDevice(iscapture, handle, handle); \
            } \
        } \
    }

    const size_t MAX_DEVS = 16;
    LIST_AUDIO_DEVICES(Out, MAX_DEVS, SDL_FALSE);
    LIST_AUDIO_DEVICES(In, MAX_DEVS, SDL_TRUE);

    #undef LIST_AUDIO_DEVICES
}

static void
NINTENDOSWITCHAUDIO_FreeDeviceHandle(void *handle)
{
    SDL_free(handle);
}

static int
NINTENDOSWITCHAUDIO_Init(SDL_AudioDriverImpl * impl)
{
    /* Set the function pointers */
    impl->DetectDevices = NINTENDOSWITCHAUDIO_DetectDevices;
    impl->OpenDevice = NINTENDOSWITCHAUDIO_OpenDevice;
    impl->PlayDevice = NINTENDOSWITCHAUDIO_PlayDevice;
    impl->WaitDevice = NINTENDOSWITCHAUDIO_WaitDevice;
    impl->GetDeviceBuf = NINTENDOSWITCHAUDIO_GetDeviceBuf;
    impl->CaptureFromDevice = NINTENDOSWITCHAUDIO_CaptureFromDevice;
    impl->FlushCapture = NINTENDOSWITCHAUDIO_FlushCapture;
    impl->CloseDevice = NINTENDOSWITCHAUDIO_CloseDevice;
    impl->FreeDeviceHandle = NINTENDOSWITCHAUDIO_FreeDeviceHandle;
    impl->HasCaptureSupport = 1;

    return 1;   /* this audio target is available. */
}

AudioBootStrap NINTENDOSWITCHAUDIO_bootstrap = {
    "nintendoswitch", "Nintendo Switch audio", NINTENDOSWITCHAUDIO_Init, 0
};

#endif  /* SDL_AUDIO_DRIVER_NINTENDO_SWITCH */

/* vi: set ts=4 sw=4 expandtab: */
