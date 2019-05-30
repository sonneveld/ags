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

#include "media/audio/clip_openal.h"

#include <cmath>

#include "media/audio/audiodefines.h"
#include "media/audio/audiointernaldefs.h"
#include "platform/base/agsplatformdriver.h"

#include "media/audio/audio_core.h"

#include "core/assets.h"
#include "main/engine.h"


void OPENAL_SOUNDCLIP::poll() { }


void OPENAL_SOUNDCLIP::configure_slot()
{
    if (slot_ < 0) { return; }

    auto vol_f = static_cast<float>(get_final_volume()) / 255.0f;
    if (vol_f < 0.0f) { vol_f = 0.0f; }
    if (vol_f > 1.0f) { vol_f = 1.0f; }

    auto speed_f = static_cast<float>(speed) / 1000.0f;
    if (speed_f <= 0.0) { speed_f = 1.0f; }

    /* Sets the pan position, ranging from 0 (left) to 255 (right). 128 is considered centre */
    auto panning_f = (static_cast<float>(panning-128) / 255.0f) * 2.0f;
    if (panning_f < -1.0f) { panning_f = -1.0f; }
    if (panning_f > 1.0f) { panning_f = 1.0f; }

    audio_core_slot_configure(slot_, repeat, vol_f, speed_f, panning_f);
}


void OPENAL_SOUNDCLIP::adjust_volume()
{
    configure_slot();
}

void OPENAL_SOUNDCLIP::set_volume(int newvol)
{
    vol = newvol;
    adjust_volume();
}

void OPENAL_SOUNDCLIP::set_speed(int new_speed)
{
    speed = new_speed;
    configure_slot();

}

void OPENAL_SOUNDCLIP::destroy()
{
    if (slot_ < 0) { return; }
    audio_core_slot_stop(slot_);
    slot_ = -1;
}

void OPENAL_SOUNDCLIP::seek(int pos_ms)
{
    if (slot_ < 0) { return; }
    audio_core_slot_seek_ms(slot_, (float)pos_ms);
}

int OPENAL_SOUNDCLIP::get_pos()
{
    if (slot_ < 0) { return -1; }
    return (int)std::round(audio_core_slot_get_pos_ms(slot_));
}

int OPENAL_SOUNDCLIP::get_pos_ms()
{
    if (slot_ < 0) { return -1; }
    return (int)std::round(audio_core_slot_get_pos_ms(slot_));
}

void OPENAL_SOUNDCLIP::set_panning(int newPanning)
{
    panning = newPanning;
    configure_slot();

}

int OPENAL_SOUNDCLIP::get_length_ms()
{
    if (slot_ < 0) { return -1; }
    return (int)std::round(audio_core_asset_length_ms(assetPath, assetExtension));
}

int OPENAL_SOUNDCLIP::play_from(int position)
{
    if (slot_ < 0) { return 0; }

    configure_slot(); // volume, speed, panning, repeat
    audio_core_slot_seek_ms(slot_, (float)position);
    audio_core_slot_play(slot_);
    return 1;
}

int OPENAL_SOUNDCLIP::play() 
{
    return play_from(0);
}

void OPENAL_SOUNDCLIP::pause()
{
    if (slot_ < 0) { return ; }
    audio_core_slot_pause(slot_);
}
void OPENAL_SOUNDCLIP::resume()
{
    if (slot_ < 0) { return; }
    audio_core_slot_play(slot_);
}

bool OPENAL_SOUNDCLIP::is_active()
{
    if (slot_ < 0) { return false; }
    auto status = audio_core_slot_get_play_state(slot_);
    return status != Stopped;
}

OPENAL_SOUNDCLIP::OPENAL_SOUNDCLIP() : SOUNDCLIP(), slot_(-1) {}




SOUNDCLIP *my_load_openal(const AssetPath &asset_name, const char *extension_hint, int voll, bool loop)
{
    // still need to return null if doesn't exist, because we use this behaviour to load different audio formats.
    if (!gameAssetLibrary->DoesAssetExist(asset_name.second)) { return nullptr; }

    auto slot = audio_core_slot_initialise(asset_name, String(extension_hint));
    if (slot < 0) { return nullptr; }

    auto clip = new OPENAL_SOUNDCLIP();
    clip->slot_ = slot;
    clip->assetPath = asset_name;
    clip->assetExtension = extension_hint;
    clip->vol = voll;
    clip->repeat = loop;

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