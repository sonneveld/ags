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
#include "media/audio/audiointernaldefs.h"
#include "platform/base/agsplatformdriver.h"

#include "media/audio/audio_core.h"
#include "media/audio/clip_openal.h"

void OPENAL_SOUNDCLIP::poll() { }

void OPENAL_SOUNDCLIP::adjust_volume()
{
    if (core_channel < 0) { return; }
    auto vol_f = static_cast<float>(get_final_volume()) / 255.0f;
    if (vol_f < 0.0f) { vol_f = 0.0f; }
    if (vol_f > 1.0f) { vol_f = 1.0f; }
    audio_core_channel_set_volume(core_channel, vol_f);
}

void OPENAL_SOUNDCLIP::set_volume(int newvol)
{
    if (core_channel < 0) { return; }
    vol = newvol;
    auto vol_f = static_cast<float>(get_final_volume()) / 255.0f;
    if (vol_f < 0.0f) { vol_f = 0.0f; }
    if (vol_f > 1.0f) { vol_f = 1.0f; }
    audio_core_channel_set_volume(core_channel, vol_f);
}

void OPENAL_SOUNDCLIP::set_speed(int new_speed)
{
    if (core_channel < 0) { return; }
    speed = new_speed;
    auto speed_f = static_cast<float>(speed) / 1000.0;
    if (speed_f <= 0.0) { speed_f = 1.0f; }
    audio_core_channel_set_speed(core_channel, speed_f);
}

void OPENAL_SOUNDCLIP::destroy()
{
    if (core_channel < 0) { return; }
    audio_core_channel_stop(core_channel);
    audio_core_channel_delete(core_channel);
    core_channel = -1;
}

void OPENAL_SOUNDCLIP::seek(int pos)
{
    if (core_channel < 0) { return; }
    audio_core_channel_seek(core_channel, pos);
}

int OPENAL_SOUNDCLIP::get_pos()
{
    if (core_channel < 0) { return -1; }
    return audio_core_channel_get_pos(core_channel);
}

int OPENAL_SOUNDCLIP::get_pos_ms()
{
    if (core_channel < 0) { return -1; }
    return audio_core_channel_get_pos_ms(core_channel);
}

void OPENAL_SOUNDCLIP::set_panning(int newPanning)
{
    if (core_channel < 0) { return; }
    panning = newPanning;
    /* Sets the pan position, ranging from 0 (left) to 255 (right). 128 is considered centre */ 
    auto panning_f = (static_cast<float>(panning-128) / 255.0f) * 2.0f;
    if (panning_f < -1.0f) { panning_f = -1.0f; }
    if (panning_f > 1.0f) { panning_f = 1.0f; }
    audio_core_channel_set_panning(core_channel, panning_f);
}

int OPENAL_SOUNDCLIP::get_length_ms()
{
    return audio_core_sample_get_length_ms(core_sample);
}

int OPENAL_SOUNDCLIP::play_from(int position)
{
    if (core_channel < 0) { return 0; }
    auto vol_f = static_cast<float>(get_final_volume()) / 255.0f;
    if (vol_f < 0.0f) { vol_f = 0.0f; }
    if (vol_f > 1.0f) { vol_f = 1.0f; }
    auto res = audio_core_channel_play_from(core_channel, core_sample, position, repeat, vol_f);
    return (res==0) ? 1 : 0;
}

int OPENAL_SOUNDCLIP::play() 
{
    return play_from(0);
}

void OPENAL_SOUNDCLIP::pause()
{
    audio_core_channel_pause(core_channel);
}
void OPENAL_SOUNDCLIP::resume()
{
    audio_core_channel_resume(core_channel);
}

bool OPENAL_SOUNDCLIP::is_active()
{
    return audio_core_is_channel_active(core_channel);
}

OPENAL_SOUNDCLIP::OPENAL_SOUNDCLIP() : SOUNDCLIP() 
{
    core_channel = -1;
    core_sample = -1;
}




SOUNDCLIP *my_load_openal(const AssetPath &asset_name, const char *extension_hint, int voll, bool loop)
{
    auto core_sample = audio_core_sample_load(asset_name, extension_hint);
    if (core_sample <= 0) { return nullptr; }
    auto core_channel = audio_core_channel_allocate();
    if (core_channel <= 0) { return nullptr; }

    auto clip = new OPENAL_SOUNDCLIP();
    clip->vol = voll;
    clip->repeat = loop;
    // clip->speed = ?
    // clip->panning = ?
    clip->core_channel = core_channel;
    clip->core_sample = core_sample;

    return clip;
}

SOUNDCLIP *my_load_wave(const AssetPath &asset_name, int voll, int loop) 
{
    auto *clip = my_load_openal(asset_name, "WAV", voll, loop);
    if (!clip) { return nullptr; }
    clip->legacy_music_type = MUS_WAVE;
    return clip;
}

SOUNDCLIP *my_load_mp3(const AssetPath &asset_name, int voll)
{
    auto *clip = my_load_openal(asset_name, "MP3", voll, false);
    if (!clip) { return nullptr; }
    clip->legacy_music_type = MUS_MP3;
    return clip;
}

SOUNDCLIP *my_load_static_mp3(const AssetPath &asset_name, int voll, bool loop)
{
    auto *clip = my_load_openal(asset_name, "MP3", voll, loop);
    if (!clip) { return nullptr; }
    clip->legacy_music_type = MUS_MP3;
    return clip;
}

SOUNDCLIP *my_load_static_ogg(const AssetPath &asset_name, int voll, bool loop)
{
    auto *clip = my_load_openal(asset_name, "OGG", voll, loop);
    if (!clip) { return nullptr; }
    clip->legacy_music_type = MUS_OGG;
    return clip;
}

SOUNDCLIP *my_load_ogg(const AssetPath &asset_name, int voll)
{
    auto *clip = my_load_openal(asset_name, "OGG", voll, false);
    if (!clip) { return nullptr; }
    clip->legacy_music_type = MUS_OGG;
    return clip;
}

SOUNDCLIP *my_load_midi(const AssetPath &asset_name, int repet)
{
    auto *clip = my_load_openal(asset_name, "MIDI", 0, repet);
    if (!clip) { return nullptr; }
    clip->legacy_music_type = MUS_MIDI;
    return clip;
}

SOUNDCLIP *my_load_mod(const AssetPath &asset_name, int repet)
{
    auto *clip = my_load_openal(asset_name, "MOD", 0, repet);
    if (!clip) { return nullptr; }
    clip->legacy_music_type = MUS_MOD;
    return clip;
}