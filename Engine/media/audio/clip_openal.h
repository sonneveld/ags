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

#ifndef AC_MEDIA_AUDIO_CLIP_OPENAL_H__
#define AC_MEDIA_AUDIO_CLIP_OPENAL_H__

#include "media/audio/soundclip.h"
#include "ac/asset_helper.h"

struct OPENAL_SOUNDCLIP final : public SOUNDCLIP
{
public:
    int slot_ = -1;
    AssetPath assetPath {};
    int lengthMs = -1;

    OPENAL_SOUNDCLIP();
    void destroy() override;

    virtual int play_from(int position) override;
    int play() override;
    void pause() override;
    void resume() override;
    bool is_active() override;

    void seek(int pos) override;

    void poll() override;

    void set_volume(int newvol) override;
    void set_speed(int new_speed) override;
    void set_panning(int newPanning) override;

    int get_pos() override;    
    int get_pos_ms() override;
    int get_length_ms() override;

protected:
    void adjust_volume() override;

private: 
    void configure_slot();
};

SOUNDCLIP *my_load_openal(const AssetPath &asset_name, const char *extension_hint, int voll, bool loop);

SOUNDCLIP *my_load_wave(const AssetPath &asset_name, int voll, int loop);
SOUNDCLIP *my_load_mp3(const AssetPath &asset_name, int voll);
SOUNDCLIP *my_load_static_mp3(const AssetPath &asset_name, int voll, bool loop);
SOUNDCLIP *my_load_static_ogg(const AssetPath &asset_name, int voll, bool loop);
SOUNDCLIP *my_load_ogg(const AssetPath &asset_name, int voll);
SOUNDCLIP *my_load_midi(const AssetPath &asset_name, int repet);
SOUNDCLIP *my_load_mod(const AssetPath &asset_name, int repet);

#endif // AC_MEDIA_AUDIO_CLIP_OPENAL_H__
