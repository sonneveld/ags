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

#include "ac/viewframe.h"

#include <set>

#include "ac/gamesetupstruct.h"
#include "debug/debug_log.h"
#include "ac/spritecache.h"
#include "gfx/bitmap.h"
#include "script/runtimescriptvalue.h"
#include "ac/dynobj/cc_audioclip.h"
#include "ac/draw.h"
#include "ac/game_version.h"
#include "media/audio/audio_system.h"

using AGS::Common::Bitmap;
using AGS::Common::Graphics;

extern GameSetupStruct game;
extern ViewStruct*views;
extern SpriteCache spriteset;
extern CCAudioClip ccDynamicAudioClip;


int ViewFrame_GetFlipped(ScriptViewFrame *svf) {
  if (views[svf->view].loops[svf->loop].frames[svf->frame].flags & VFLG_FLIPSPRITE)
    return 1;
  return 0;
}

int ViewFrame_GetGraphic(ScriptViewFrame *svf) {
  return views[svf->view].loops[svf->loop].frames[svf->frame].pic;
}

void ViewFrame_SetGraphic(ScriptViewFrame *svf, int newPic) {
  views[svf->view].loops[svf->loop].frames[svf->frame].pic = newPic;
}

ScriptAudioClip* ViewFrame_GetLinkedAudio(ScriptViewFrame *svf) 
{
  int soundIndex = views[svf->view].loops[svf->loop].frames[svf->frame].sound;
  if (soundIndex < 0)
    return nullptr;

  return &game.audioClips[soundIndex];
}

void ViewFrame_SetLinkedAudio(ScriptViewFrame *svf, ScriptAudioClip* clip) 
{
  int newSoundIndex = -1;
  if (clip != nullptr)
    newSoundIndex = clip->id;

  views[svf->view].loops[svf->loop].frames[svf->frame].sound = newSoundIndex;
}

int ViewFrame_GetSound(ScriptViewFrame *svf) {
  // convert audio clip to old-style sound number
  return get_old_style_number_for_sound(views[svf->view].loops[svf->loop].frames[svf->frame].sound);
}

void ViewFrame_SetSound(ScriptViewFrame *svf, int newSound) 
{
  if (newSound < 1)
  {
    views[svf->view].loops[svf->loop].frames[svf->frame].sound = -1;
  }
  else
  {
    // convert sound number to audio clip
    ScriptAudioClip* clip = GetAudioClipForOldStyleNumber(game, false, newSound);
    if (clip == nullptr)
      quitprintf("!SetFrameSound: audio clip aSound%d not found", newSound);

    views[svf->view].loops[svf->loop].frames[svf->frame].sound = clip->id + (game.IsLegacyAudioSystem() ? 0x10000000 : 0);
  }
}

int ViewFrame_GetSpeed(ScriptViewFrame *svf) {
  return views[svf->view].loops[svf->loop].frames[svf->frame].speed;
}

int ViewFrame_GetView(ScriptViewFrame *svf) {
  return svf->view + 1;
}

int ViewFrame_GetLoop(ScriptViewFrame *svf) {
  return svf->loop;
}

int ViewFrame_GetFrame(ScriptViewFrame *svf) {
  return svf->frame;
}

//=============================================================================

void precache_view(int view) 
{
    if (view < 0) 
        return;

    for (int i = 0; i < views[view].numLoops; i++) {
        for (int j = 0; j < views[view].loops[i].numFrames; j++)
            spriteset.Precache(views[view].loops[i].frames[j].pic);
    }
}


void load_view(AGS::Common::String view_name)
{
    //auto viewpath = AGS::Common::String::FromFormat("view/%s", view_name.GetCStr());
    //auto in = gameAssetLibrary->OpenAsset(viewpath);
    //if (in != nullptr) {
    //    auto numviews = in->ReadInt32();
    //    for (int i = 0; i < numviews; i++) {
    //        spriteset.LoadSprite(in);
    //    }
    //    delete in;
    //}
}

void load_loop_in_background(int view_index, int loop_index)
{

  auto &view = views[view_index];
  auto &loop = view.loops[loop_index];

  for (int j = 0; j < loop.numFrames; j++) {
    spriteset.PreloadSprite(loop.frames[j].pic, 1000+j);
  }

}


void load_view_in_background(int view_index)
{
    if (view_index < 0) { return; }

    //std::set<int> sprites_seen;
    //int total_sprites = 0;
    //int unloaded_sprites = 0;
    //for (int i = 0; i < views[view_index].numLoops && i < 8; i++) {
    //    for (int j = 0; j < views[view_index].loops[i].numFrames; j++) {
    //        auto pic = views[view_index].loops[i].frames[j].pic;
    //        if (sprites_seen.find(pic) == sprites_seen.end()) {
    //            if (!spriteset.IsLoaded(pic)) {
    //                unloaded_sprites += 1;
    //            }
    //            total_sprites += 1;
    //            sprites_seen.insert(pic);
    //        }
    //    }
    //}

    //// if there's less than N, it might be worth just loading the sprite file directly
    //if (unloaded_sprites >= 3) {
    //    int view_id = view_index + 1;
    //    auto viewpath = AGS::Common::String::FromFormat("%d", view_id);
    //    load_view_in_background(viewpath);
    //}

    // view may have been modified, so mop up any others.
    for (int i = 0; i < views[view_index].numLoops && i < 8; i++) {
        for (int j = 0; j < views[view_index].loops[i].numFrames; j++) {
            spriteset.PreloadSprite(views[view_index].loops[i].frames[j].pic, 2000+j);
        }
    }
}

// the specified frame has just appeared, see if we need
// to play a sound or whatever
void CheckViewFrame (int view, int loop, int frame, int sound_volume) {
    ScriptAudioChannel *channel = nullptr;
    if (game.IsLegacyAudioSystem())
    {
        if (views[view].loops[loop].frames[frame].sound > 0)
        {
            if (views[view].loops[loop].frames[frame].sound < 0x10000000)
            {
                ScriptAudioClip* clip = GetAudioClipForOldStyleNumber(game, false, views[view].loops[loop].frames[frame].sound);
                if (clip)
                    views[view].loops[loop].frames[frame].sound = clip->id + 0x10000000;
                else
                {
                    views[view].loops[loop].frames[frame].sound = 0;
                    return;
                }
            }
            channel = play_audio_clip_by_index(views[view].loops[loop].frames[frame].sound - 0x10000000);
        }
    }
    else
    {
        if (views[view].loops[loop].frames[frame].sound >= 0) {
            // play this sound (eg. footstep)
            channel = play_audio_clip_by_index(views[view].loops[loop].frames[frame].sound);
        }
    }
    if (sound_volume != SCR_NO_VALUE && channel != nullptr)
    {
        AudioChannelsLock lock;
        auto* ch = lock.GetChannel(channel->id);
        if (ch)
            ch->set_volume_percent(ch->get_volume() * sound_volume / 100);
    }
    
}

// draws a view frame, flipped if appropriate
void DrawViewFrame(Bitmap *ds, const ViewFrame *vframe, int x, int y, bool alpha_blend)
{
    if (alpha_blend && (loaded_game_file_version >= kGameVersion_330))
    {
        Bitmap *vf_bmp = spriteset[vframe->pic];
        Bitmap *src = vf_bmp;
        if (vframe->flags & VFLG_FLIPSPRITE)
        {
            src = new Bitmap(vf_bmp->GetWidth(), vf_bmp->GetHeight(), vf_bmp->GetColorDepth());
            src->FlipBlt(vf_bmp, 0, 0, Common::kBitmap_HFlip);
        }
        draw_sprite_support_alpha(ds, true, x, y, src, (game.SpriteInfos[vframe->pic].Flags & SPF_ALPHACHANNEL) != 0);
        if (src != vf_bmp)
            delete src;
    }
    else
    {
        if (vframe->flags & VFLG_FLIPSPRITE)
            ds->FlipBlt(spriteset[vframe->pic], x, y, Common::kBitmap_HFlip);
        else
            ds->Blit(spriteset[vframe->pic], x, y, Common::kBitmap_Transparency);
    }
}

//=============================================================================
//
// Script API Functions
//
//=============================================================================

#include "debug/out.h"
#include "script/script_api.h"
#include "script/script_runtime.h"

// int (ScriptViewFrame *svf)
RuntimeScriptValue Sc_ViewFrame_GetFlipped(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_INT(ScriptViewFrame, ViewFrame_GetFlipped);
}

// int (ScriptViewFrame *svf)
RuntimeScriptValue Sc_ViewFrame_GetFrame(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_INT(ScriptViewFrame, ViewFrame_GetFrame);
}
// int (ScriptViewFrame *svf)
RuntimeScriptValue Sc_ViewFrame_GetGraphic(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_INT(ScriptViewFrame, ViewFrame_GetGraphic);
}

// void (ScriptViewFrame *svf, int newPic)
RuntimeScriptValue Sc_ViewFrame_SetGraphic(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_VOID_PINT(ScriptViewFrame, ViewFrame_SetGraphic);
}

// ScriptAudioClip* (ScriptViewFrame *svf)
RuntimeScriptValue Sc_ViewFrame_GetLinkedAudio(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_OBJ(ScriptViewFrame, ScriptAudioClip, ccDynamicAudioClip, ViewFrame_GetLinkedAudio);
}

// void (ScriptViewFrame *svf, ScriptAudioClip* clip)
RuntimeScriptValue Sc_ViewFrame_SetLinkedAudio(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_VOID_POBJ(ScriptViewFrame, ViewFrame_SetLinkedAudio, ScriptAudioClip);
}

// int (ScriptViewFrame *svf)
RuntimeScriptValue Sc_ViewFrame_GetLoop(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_INT(ScriptViewFrame, ViewFrame_GetLoop);
}

// int (ScriptViewFrame *svf)
RuntimeScriptValue Sc_ViewFrame_GetSound(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_INT(ScriptViewFrame, ViewFrame_GetSound);
}

// void (ScriptViewFrame *svf, int newSound)
RuntimeScriptValue Sc_ViewFrame_SetSound(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_VOID_PINT(ScriptViewFrame, ViewFrame_SetSound);
}

// int (ScriptViewFrame *svf)
RuntimeScriptValue Sc_ViewFrame_GetSpeed(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_INT(ScriptViewFrame, ViewFrame_GetSpeed);
}

// int (ScriptViewFrame *svf)
RuntimeScriptValue Sc_ViewFrame_GetView(void *self, const RuntimeScriptValue *params, int32_t param_count)
{
    API_OBJCALL_INT(ScriptViewFrame, ViewFrame_GetView);
}


void RegisterViewFrameAPI()
{
    ccAddExternalObjectFunction("ViewFrame::get_Flipped",       Sc_ViewFrame_GetFlipped);
    ccAddExternalObjectFunction("ViewFrame::get_Frame",         Sc_ViewFrame_GetFrame);
    ccAddExternalObjectFunction("ViewFrame::get_Graphic",       Sc_ViewFrame_GetGraphic);
    ccAddExternalObjectFunction("ViewFrame::set_Graphic",       Sc_ViewFrame_SetGraphic);
    ccAddExternalObjectFunction("ViewFrame::get_LinkedAudio",   Sc_ViewFrame_GetLinkedAudio);
    ccAddExternalObjectFunction("ViewFrame::set_LinkedAudio",   Sc_ViewFrame_SetLinkedAudio);
    ccAddExternalObjectFunction("ViewFrame::get_Loop",          Sc_ViewFrame_GetLoop);
    ccAddExternalObjectFunction("ViewFrame::get_Sound",         Sc_ViewFrame_GetSound);
    ccAddExternalObjectFunction("ViewFrame::set_Sound",         Sc_ViewFrame_SetSound);
    ccAddExternalObjectFunction("ViewFrame::get_Speed",         Sc_ViewFrame_GetSpeed);
    ccAddExternalObjectFunction("ViewFrame::get_View",          Sc_ViewFrame_GetView);

    /* ----------------------- Registering unsafe exports for plugins -----------------------*/

    ccAddExternalFunctionForPlugin("ViewFrame::get_Flipped",       (void*)ViewFrame_GetFlipped);
    ccAddExternalFunctionForPlugin("ViewFrame::get_Frame",         (void*)ViewFrame_GetFrame);
    ccAddExternalFunctionForPlugin("ViewFrame::get_Graphic",       (void*)ViewFrame_GetGraphic);
    ccAddExternalFunctionForPlugin("ViewFrame::set_Graphic",       (void*)ViewFrame_SetGraphic);
    ccAddExternalFunctionForPlugin("ViewFrame::get_LinkedAudio",   (void*)ViewFrame_GetLinkedAudio);
    ccAddExternalFunctionForPlugin("ViewFrame::set_LinkedAudio",   (void*)ViewFrame_SetLinkedAudio);
    ccAddExternalFunctionForPlugin("ViewFrame::get_Loop",          (void*)ViewFrame_GetLoop);
    ccAddExternalFunctionForPlugin("ViewFrame::get_Sound",         (void*)ViewFrame_GetSound);
    ccAddExternalFunctionForPlugin("ViewFrame::set_Sound",         (void*)ViewFrame_SetSound);
    ccAddExternalFunctionForPlugin("ViewFrame::get_Speed",         (void*)ViewFrame_GetSpeed);
    ccAddExternalFunctionForPlugin("ViewFrame::get_View",          (void*)ViewFrame_GetView);
}
