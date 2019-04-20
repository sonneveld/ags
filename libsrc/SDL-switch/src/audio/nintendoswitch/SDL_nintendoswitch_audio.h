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

#ifndef SDL_nintendoswitch_audio_h_
#define SDL_nintendoswitch_audio_h_

#include "../SDL_sysaudio.h"

/* Hidden "this" pointer for the audio functions */
#define _THIS   SDL_AudioDevice *_this

struct SDL_PrivateAudioData
{
    /* if iscapture, use audioIn, otherwise use audioOut. */
    union {
        nn::audio::AudioIn audioIn;
        nn::audio::AudioOut audioOut;
    };
    union {
        nn::audio::AudioInBuffer audioBufferIn[2];
        nn::audio::AudioOutBuffer audioBufferOut[2];
    };
    void *buffer[2];  /* allocated memory backing queueable audio buffers. */
    nn::os::SystemEvent event;
    SDL_bool opened;
    SDL_bool started;
    nn::audio::AudioOutBuffer *nextAudioBuffer;   /* for playback. */
    Uint8 *remainingData;  /* for capture. */
    size_t remainingDataLen;
    size_t remainingAllocLen;
    int64_t maxWait;
};

#endif /* SDL_nintendoswitch_audio_h_ */

/* vi: set ts=4 sw=4 expandtab: */
