//=============================================================================
//
// Adventure Game Studio (AGS)
//
// Copyright (C) 1999-2011 Chris Jones and 2011-20xx others
// The full list of copyright holders can be found in the Copyright.txt
// file, which is part of this source code distribution.
//
// The AGS source code is provided under the Artistic License 2.0.
// A copy of this license can be found in the file License.txt and at
// http://www.opensource.org/licenses/artistic-license-2.0.php
//
//=============================================================================

#include "media/audio/audiodefines.h"
#include "media/audio/clip_mystaticogg.h"
#include "media/audio/audiointernaldefs.h"
#include "media/audio/soundcache.h"
#include "util/mutex_lock.h"

#include "platform/base/agsplatformdriver.h"

#include <math.h>
#include <SDL2/sdl.h>

extern ALint allocate_almixer_channel();
extern void release_almixer_channel(ALint channel);

int MYSTATICOGG::poll()
{
	AGS::Engine::MutexLock _lock(_mutex);

    if (this->almixerChannel >= 0 && !done && _destroyThis)
    {
      internal_destroy();
      _destroyThis = false;
    }

    if ((this->almixerChannel < 0) || (!ready))
        ; // Do nothing
    else if (ALmixer_IsPlayingChannel(this->almixerChannel)) {
        if (!repeat)
        {
            done = 1;
            if (psp_audio_multithreaded)
                internal_destroy();
        }
    }
    else get_pos();  // call this to keep the last_but_one stuff up to date

    return done;
}

void MYSTATICOGG::adjust_stream()
{    
    // volume
    float volumef = (float)get_final_volume() / 255.0f;
    if (volumef > 1.0f) { volumef = 1.0f; }
    if (volumef < -1.0f) { volumef = -1.0f; }
    ALmixer_SetVolumeChannel(this->almixerChannel, volumef);
    
    int sid = ALmixer_GetSource(this->almixerChannel);
    
    // panning
    //  via http://stackoverflow.com/a/23189797/84262
    float balance = ((float)this->panning / 255.0f) * 2.0f - 1.0f;
    if (balance > 1.0f) { balance = 1.0f; }
    if (balance < -1.0f) { balance = -1.0f; }
    alSource3f(sid, AL_POSITION, balance, 0.0f, sqrtf(1.0f - powf(balance, 2.0f)));
    
    // repeat cannot be adjusted
    
    alSourcef(sid, AL_PITCH, speed/1000.0f);
}

void MYSTATICOGG::adjust_volume()
{
    adjust_stream();
}

void MYSTATICOGG::set_volume(int newvol)
{
    vol = newvol;
    adjust_stream();
}

void MYSTATICOGG::set_speed(int new_speed)
{
    speed = new_speed;
    adjust_stream();
}

void MYSTATICOGG::internal_destroy()
{
    if (this->almixerChannel >= 0) {
        ALmixer_HaltChannel(this->almixerChannel);
        release_almixer_channel(this->almixerChannel);
        this->almixerChannel = -1;
    }
    
    if (this->almixerData) {
        ALmixer_FreeData(this->almixerData);
        this->almixerData = nullptr;
    }
    
    if (this->rw_ops) {
        // Turns out that almixer handily frees this.
        // SDL_RWclose(reinterpret_cast<SDL_RWops *>(this->rw_ops));
        this->rw_ops = nullptr;
    }

    if (mp3buffer != NULL) {
        sound_cache_free(mp3buffer, false);
        mp3buffer = NULL;
    }

    _destroyThis = false;
    done = 1;
}

void MYSTATICOGG::destroy()
{
	AGS::Engine::MutexLock _lock(_mutex);

    if (psp_audio_multithreaded && _playing && !_audio_doing_crossfade)
      _destroyThis = true;
    else
      internal_destroy();

	_lock.Release();

    while (!done)
      AGSPlatformDriver::GetDriver()->YieldCPU();

    // Allow the last poll cycle to finish.
	_lock.Acquire(_mutex);
}

void MYSTATICOGG::seek(int pos)
{
	AGS::Engine::MutexLock _lock;
    if (psp_audio_multithreaded) {
		_lock.Acquire(_mutex);
    }
    ALmixer_SeekChannel(this->almixerChannel, pos);
}

int MYSTATICOGG::get_pos()
{
    return get_pos_ms();
}

int MYSTATICOGG::get_pos_ms()
{
    int source = ALmixer_GetSource(this->almixerChannel);
    ALfloat offsetSeconds;
    alGetSourcef(source, AL_SEC_OFFSET, &offsetSeconds);
    
    int result = (int)(offsetSeconds*1000.0f);
    return result;
}

int MYSTATICOGG::get_length_ms()
{
    return ALmixer_GetTotalTime(this->almixerData);
}

void MYSTATICOGG::restart()
{
    if (this->almixerChannel < 0) { return; }
    
    ALmixer_SeekChannel(this->almixerChannel, 0);
    ALmixer_ResumeChannel(this->almixerChannel);
    
    done = 0;
    
    if (!psp_audio_multithreaded)
        poll();
}

int MYSTATICOGG::get_voice()
{
    printf("OGG STUB: get_voice\n");
    // only used by super to change some parameters.
    return -1;
}

int MYSTATICOGG::get_sound_type() {
    return MUS_OGG;
}

int MYSTATICOGG::play_from(int position)
{
    _playing = true;
    
    this->almixerChannel = allocate_almixer_channel();
    this->almixerChannel = ALmixer_PlayChannel(this->almixerChannel, this->almixerData, this->repeat ? -1 : 0);
    
    if (position > 0) {
        ALmixer_SeekChannel(this->almixerChannel, position);
    }

    if (!psp_audio_multithreaded)
      poll();

    return 1;
}

int MYSTATICOGG::play() {
    return play_from(0);
}

void MYSTATICOGG::set_panning(int newPanning) {
    panning = newPanning;
    this->adjust_stream();
}

void MYSTATICOGG::pause() {
    ALmixer_PauseChannel(this->almixerChannel);
    paused = 1;
}

void MYSTATICOGG::resume() {
    ALmixer_ResumeChannel(this->almixerChannel);
    paused = 0;
}

int MYSTATICOGG::load_from_buffer(char *buffer, long muslen) {
    this->done = 0;
    this->mp3buffer = buffer;
    this->mp3buffersize = muslen;
    
    this->rw_ops = reinterpret_cast<ALmixer_RWops *>(SDL_RWFromConstMem(mp3buffer, muslen));
    if (!rw_ops) { return -1; }
    
    this->almixerData = ALmixer_LoadAll_RW(this->rw_ops, "ogg", AL_FALSE);
    if (!this->almixerData) { return -1; }
    
    this->ready = true;
    return 0;
}

MYSTATICOGG::MYSTATICOGG() : SOUNDCLIP() {
    this->mp3buffer = nullptr;
    this->mp3buffersize = -1;
    
    this->rw_ops = nullptr;
    this->almixerData = nullptr;
    this->almixerChannel = -1;

    this->done = 0;

    this->ready = false;
}
