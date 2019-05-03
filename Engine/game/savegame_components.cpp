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

#include <map>

#include "ac/audiocliptype.h"
#include "ac/character.h"
#include "ac/common.h"
#include "ac/dialogtopic.h"
#include "ac/draw.h"
#include "ac/dynamicsprite.h"
#include "ac/game.h"
#include "ac/gamesetupstruct.h"
#include "ac/gamestate.h"
#include "ac/gui.h"
#include "ac/mouse.h"
#include "ac/movelist.h"
#include "ac/roomstatus.h"
#include "ac/screenoverlay.h"
#include "ac/spritecache.h"
#include "ac/view.h"
#include "ac/system.h"
#include "ac/dynobj/cc_serializer.h"
#include "debug/out.h"
#include "game/savegame_components.h"
#include "gfx/bitmap.h"
#include "gui/animatingguibutton.h"
#include "gui/guibutton.h"
#include "gui/guiinv.h"
#include "gui/guilabel.h"
#include "gui/guilistbox.h"
#include "gui/guimain.h"
#include "gui/guislider.h"
#include "gui/guitextbox.h"
#include "plugin/agsplugin.h"
#include "plugin/plugin_engine.h"
#include "script/cc_error.h"
#include "script/script.h"
#include "util/filestream.h" // TODO: needed only because plugins expect file handle
#include "media/audio/audio_system.h"

using namespace Common;

extern GameSetupStruct game;
extern color palette[256];
extern DialogTopic *dialog;
extern AnimatingGUIButton animbuts[MAX_ANIMATING_BUTTONS];
extern int numAnimButs;
extern ViewStruct *views;
extern ScreenOverlay screenover[MAX_SCREEN_OVERLAYS];
extern int numscreenover;
extern Bitmap *dynamicallyCreatedSurfaces[MAX_DYNAMIC_SURFACES];
extern RoomStruct thisroom;
extern RoomStatus troom;
extern Bitmap *raw_saved_screen;
extern MoveList *mls;


namespace AGS
{
namespace Engine
{

namespace SavegameComponents
{

const String ComponentListTag = "Components";

void WriteFormatTag(PStream out, const String &tag, bool open = true)
{
    String full_tag = String::FromFormat(open ? "<%s>" : "</%s>", tag.GetCStr());
    out->Write(full_tag.GetCStr(), full_tag.GetLength());
}

bool ReadFormatTag(PStream in, String &tag, bool open = true)
{
    if (in->ReadByte() != '<')
        return false;
    if (!open && in->ReadByte() != '/')
        return false;
    tag.Empty();
    while (!in->EOS())
    {
        char c = in->ReadByte();
        if (c == '>')
            return true;
        tag.AppendChar(c);
    }
    return false; // reached EOS before closing symbol
}

bool AssertFormatTag(PStream in, const String &tag, bool open = true)
{
    String read_tag;
    if (!ReadFormatTag(in, read_tag, open))
        return false;
    return read_tag.Compare(tag) == 0;
}

bool AssertFormatTagStrict(HSaveError &err, PStream in, const String &tag, bool open = true)
{
    
    String read_tag;
    if (!ReadFormatTag(in, read_tag, open) || read_tag.Compare(tag) != 0)
    {
        err = new SavegameError(kSvgErr_InconsistentFormat,
            String::FromFormat("Mismatching tag: %s.", tag.GetCStr()));
        return false;
    }
    return true;
}

inline bool AssertCompatLimit(HSaveError &err, int count, int max_count, const char *content_name)
{
    if (count > max_count)
    {
        err = new SavegameError(kSvgErr_IncompatibleEngine,
            String::FromFormat("Incompatible number of %s (count: %d, max: %d).",
            content_name, count, max_count));
        return false;
    }
    return true;
}

inline bool AssertCompatRange(HSaveError &err, int value, int min_value, int max_value, const char *content_name)
{
    if (value < min_value || value > max_value)
    {
        err = new SavegameError(kSvgErr_IncompatibleEngine,
            String::FromFormat("Restore game error: incompatible %s (id: %d, range: %d - %d).",
            content_name, value, min_value, max_value));
        return false;
    }
    return true;
}

inline bool AssertGameContent(HSaveError &err, int new_val, int original_val, const char *content_name)
{
    if (new_val != original_val)
    {
        err = new SavegameError(kSvgErr_GameContentAssertion,
            String::FromFormat("Mismatching number of %s (game: %d, save: %d).",
            content_name, original_val, new_val));
        return false;
    }
    return true;
}

inline bool AssertGameObjectContent(HSaveError &err, int new_val, int original_val, const char *content_name,
                                    const char *obj_type, int obj_id)
{
    if (new_val != original_val)
    {
        err = new SavegameError(kSvgErr_GameContentAssertion,
            String::FromFormat("Mismatching number of %s, %s #%d (game: %d, save: %d).",
            content_name, obj_type, obj_id, original_val, new_val));
        return false;
    }
    return true;
}

inline bool AssertGameObjectContent2(HSaveError &err, int new_val, int original_val, const char *content_name,
                                    const char *obj1_type, int obj1_id, const char *obj2_type, int obj2_id)
{
    if (new_val != original_val)
    {
        err = new SavegameError(kSvgErr_GameContentAssertion,
            String::FromFormat("Mismatching number of %s, %s #%d, %s #%d (game: %d, save: %d).",
            content_name, obj1_type, obj1_id, obj2_type, obj2_id, original_val, new_val));
        return false;
    }
    return true;
}


enum GameViewCamFlags
{
    kSvgGameAutoRoomView = 0x01
};

enum CameraSaveFlags
{
    kSvgCamPosLocked = 0x01
};

enum ViewportSaveFlags
{
    kSvgViewportVisible = 0x01
};

void WriteCameraState(const Camera &cam, Stream *out)
{
    int flags = 0;
    if (cam.IsLocked()) flags |= kSvgCamPosLocked;
    out->WriteInt32(flags);
    const Rect &rc = cam.GetRect();
    out->WriteInt32(rc.Left);
    out->WriteInt32(rc.Top);
    out->WriteInt32(rc.GetWidth());
    out->WriteInt32(rc.GetHeight());
}

void WriteViewportState(const Viewport &view, Stream *out)
{
    int flags = 0;
    if (view.IsVisible()) flags |= kSvgViewportVisible;
    out->WriteInt32(flags);
    const Rect &rc = view.GetRect();
    out->WriteInt32(rc.Left);
    out->WriteInt32(rc.Top);
    out->WriteInt32(rc.GetWidth());
    out->WriteInt32(rc.GetHeight());
    out->WriteInt32(view.GetZOrder());
    auto cam = view.GetCamera();
    if (cam)
        out->WriteInt32(cam->GetID());
    else
        out->WriteInt32(-1);
}

HSaveError WriteGameState(PStream out)
{
    // Game base
    game.WriteForSavegame(out);
    // Game palette
    // TODO: probably no need to save this for hi/true-res game
    out->WriteArray(palette, sizeof(color), 256);

    if (loaded_game_file_version <= kGameVersion_272)
    {
        // Global variables
        out->WriteInt32(numGlobalVars);
        for (int i = 0; i < numGlobalVars; ++i)
            globalvars[i].Write(out.get());
    }

    // Game state
    play.WriteForSavegame(out.get());
    // Other dynamic values
    out->WriteInt32(frames_per_second);
    out->WriteInt32(loopcounter);
    out->WriteInt32(ifacepopped);
    out->WriteInt32(game_paused);
    // Mouse cursor
    out->WriteInt32(cur_mode);
    out->WriteInt32(cur_cursor);
    out->WriteInt32(mouse_on_iface);

    // Viewports and cameras
    int viewcam_flags = 0;
    if (play.IsAutoRoomViewport())
        viewcam_flags |= kSvgGameAutoRoomView;
    out->WriteInt32(viewcam_flags);
    out->WriteInt32(play.GetRoomCameraCount());
    for (int i = 0; i < play.GetRoomCameraCount(); ++i)
        WriteCameraState(*play.GetRoomCamera(i), out.get());
    out->WriteInt32(play.GetRoomViewportCount());
    for (int i = 0; i < play.GetRoomViewportCount(); ++i)
        WriteViewportState(*play.GetRoomViewportObj(i), out.get());

    return HSaveError::None();
}

void ReadCameraState(/*Camera &cam,*/ RestoredData &r_data, Stream *in)
{
    Camera cam;
    cam.SetID(r_data.Cameras.size());
    int flags = in->ReadInt32();
    if ((flags & kSvgCamPosLocked) != 0)
        cam.Lock();
    else
        cam.Release();
    int left = in->ReadInt32();
    int top = in->ReadInt32();
    int width = in->ReadInt32();
    int height = in->ReadInt32();
    cam.SetAt(left, top);
    cam.SetSize(Size(width, height));
    r_data.Cameras.push_back(cam);
}

void ReadViewportState(/*Viewport &view,*/ RestoredData &r_data, Stream *in)
{
    Viewport view;
    view.SetID(r_data.Viewports.size());
    int flags = in->ReadInt32();
    view.SetVisible((flags & kSvgViewportVisible) != 0);
    const Rect &rc = view.GetRect();
    int left = in->ReadInt32();
    int top = in->ReadInt32();
    int width = in->ReadInt32();
    int height = in->ReadInt32();
    view.SetRect(RectWH(left, top, width, height));
    view.SetZOrder(in->ReadInt32());
    int cam_index = in->ReadInt32();
    r_data.Viewports.push_back(view);
    r_data.ViewCamLinks.push_back(cam_index);
}

HSaveError ReadGameState(PStream in, int32_t cmp_ver, const PreservedParams &pp, RestoredData &r_data)
{
    HSaveError err;
    GameStateSvgVersion svg_ver = (GameStateSvgVersion)cmp_ver;
    // Game base
    game.ReadFromSavegame(in);
    // Game palette
    in->ReadArray(palette, sizeof(color), 256);

    if (loaded_game_file_version <= kGameVersion_272)
    {
        // Legacy interaction global variables
        if (!AssertGameContent(err, in->ReadInt32(), numGlobalVars, "Global Variables"))
            return err;
        for (int i = 0; i < numGlobalVars; ++i)
            globalvars[i].Read(in.get());
    }

    // Game state
    play.ReadFromSavegame(in.get(), svg_ver);

    // Other dynamic values
    r_data.FPS = in->ReadInt32();
    set_loop_counter(in->ReadInt32());
    ifacepopped = in->ReadInt32();
    game_paused = in->ReadInt32();
    // Mouse cursor state
    r_data.CursorMode = in->ReadInt32();
    r_data.CursorID = in->ReadInt32();
    mouse_on_iface = in->ReadInt32();

    // Viewports and cameras
    if (svg_ver < kGSSvgVersion_3510)
    {
        int camx = in->ReadInt32();
        int camy = in->ReadInt32();
        play.GetRoomCamera(0)->SetAt(camx, camy);
    }
    else
    {
        int viewcam_flags = in->ReadInt32();
        play.SetAutoRoomViewport((viewcam_flags & kSvgGameAutoRoomView) != 0);
        // TODO: we create viewport and camera objects here because they are
        // required for the managed pool deserialization, but read actual
        // data into temp structs because of issue with load_new_room().
        // See comments to RestoredData struct for further details.
        int cam_count = in->ReadInt32();
        for (int i = 0; i < cam_count; ++i)
        {
            play.CreateRoomCamera();
            ReadCameraState(r_data, in.get());
        }
        int view_count = in->ReadInt32();
        for (int i = 0; i < view_count; ++i)
        {
            play.CreateRoomViewport();
            ReadViewportState(r_data, in.get());
        }
    }
    return err;
}

HSaveError WriteAudio(PStream out)
{
    AudioChannelsLock lock;

    // Game content assertion
    out->WriteInt32(game.audioClipTypeCount);
    out->WriteInt32(game.audioClipCount);
    // Audio types
    for (int i = 0; i < game.audioClipTypeCount; ++i)
    {
        game.audioClipTypes[i].WriteToSavegame(out.get());
        out->WriteInt32(play.default_audio_type_volumes[i]);
    }

    // Audio clips and crossfade
    for (int i = 0; i <= MAX_SOUND_CHANNELS; i++)
    {
        auto* ch = lock.GetChannelIfPlaying(i);
        if ((ch != nullptr) && (ch->sourceClip != nullptr))
        {
            out->WriteInt32(((ScriptAudioClip*)ch->sourceClip)->id);
            out->WriteInt32(ch->get_pos());
            out->WriteInt32(ch->priority);
            out->WriteInt32(ch->repeat ? 1 : 0);
            out->WriteInt32(ch->vol);
            out->WriteInt32(ch->panning);
            out->WriteInt32(ch->volAsPercentage);
            out->WriteInt32(ch->panningAsPercentage);
            out->WriteInt32(ch->get_speed());
        }
        else
        {
            out->WriteInt32(-1);
        }
    }
    out->WriteInt32(crossFading);
    out->WriteInt32(crossFadeVolumePerStep);
    out->WriteInt32(crossFadeStep);
    out->WriteInt32(crossFadeVolumeAtStart);
    // CHECKME: why this needs to be saved?
    out->WriteInt32(current_music_type);

    // Ambient sound
    for (int i = 0; i < MAX_SOUND_CHANNELS; ++i)
        ambient[i].WriteToFile(out.get());
    return HSaveError::None();
}

HSaveError ReadAudio(PStream in, int32_t cmp_ver, const PreservedParams &pp, RestoredData &r_data)
{
    HSaveError err;
    // Game content assertion
    if (!AssertGameContent(err, in->ReadInt32(), game.audioClipTypeCount, "Audio Clip Types"))
        return err;
    if (!AssertGameContent(err, in->ReadInt32(), game.audioClipCount, "Audio Clips"))
        return err;

    // Audio types
    for (int i = 0; i < game.audioClipTypeCount; ++i)
    {
        game.audioClipTypes[i].ReadFromSavegame(in.get());
        play.default_audio_type_volumes[i] = in->ReadInt32();
    }

    // Audio clips and crossfade
    for (int i = 0; i <= MAX_SOUND_CHANNELS; ++i)
    {
        RestoredData::ChannelInfo &chan_info = r_data.AudioChans[i];
        chan_info.Pos = 0;
        chan_info.ClipID = in->ReadInt32();
        if (chan_info.ClipID >= 0)
        {
            chan_info.Pos = in->ReadInt32();
            if (chan_info.Pos < 0)
                chan_info.Pos = 0;
            chan_info.Priority = in->ReadInt32();
            chan_info.Repeat = in->ReadInt32();
            chan_info.Vol = in->ReadInt32();
            chan_info.Pan = in->ReadInt32();
            chan_info.VolAsPercent = in->ReadInt32();
            chan_info.PanAsPercent = in->ReadInt32();
            chan_info.Speed = 1000;
            chan_info.Speed = in->ReadInt32();
        }
    }
    crossFading = in->ReadInt32();
    crossFadeVolumePerStep = in->ReadInt32();
    crossFadeStep = in->ReadInt32();
    crossFadeVolumeAtStart = in->ReadInt32();
    // preserve legacy music type setting
    current_music_type = in->ReadInt32();
    
    // Ambient sound
    for (int i = 0; i < MAX_SOUND_CHANNELS; ++i)
        ambient[i].ReadFromFile(in.get());
    for (int i = 1; i < MAX_SOUND_CHANNELS; ++i)
    {
        if (ambient[i].channel == 0)
        {
            r_data.DoAmbient[i] = 0;
        }
        else
        {
            r_data.DoAmbient[i] = ambient[i].num;
            ambient[i].channel = 0;
        }
    }
    return err;
}

HSaveError WriteCharacters(PStream out)
{
    out->WriteInt32(game.numcharacters);
    for (int i = 0; i < game.numcharacters; ++i)
    {
        game.chars[i].WriteToFile(out.get());
        charextra[i].WriteToFile(out.get());
        Properties::WriteValues(play.charProps[i], out.get());
        if (loaded_game_file_version <= kGameVersion_272)
            game.intrChar[i]->WriteTimesRunToSavedgame(out.get());
        // character movement path cache
        mls[CHMLSOFFS + i].WriteToFile(out.get());
    }
    return HSaveError::None();
}

HSaveError ReadCharacters(PStream in, int32_t cmp_ver, const PreservedParams &pp, RestoredData &r_data)
{
    HSaveError err;
    if (!AssertGameContent(err, in->ReadInt32(), game.numcharacters, "Characters"))
        return err;
    for (int i = 0; i < game.numcharacters; ++i)
    {
        game.chars[i].ReadFromFile(in.get());
        charextra[i].ReadFromFile(in.get());
        Properties::ReadValues(play.charProps[i], in.get());
        if (loaded_game_file_version <= kGameVersion_272)
            game.intrChar[i]->ReadTimesRunFromSavedgame(in.get());
        // character movement path cache
        err = mls[CHMLSOFFS + i].ReadFromFile(in.get(), cmp_ver > 0 ? 1 : 0);
        if (!err)
            return err;
    }
    return err;
}

HSaveError WriteDialogs(PStream out)
{
    out->WriteInt32(game.numdialog);
    for (int i = 0; i < game.numdialog; ++i)
    {
        dialog[i].WriteToSavegame(out.get());
    }
    return HSaveError::None();
}

HSaveError ReadDialogs(PStream in, int32_t cmp_ver, const PreservedParams &pp, RestoredData &r_data)
{
    HSaveError err;
    if (!AssertGameContent(err, in->ReadInt32(), game.numdialog, "Dialogs"))
        return err;
    for (int i = 0; i < game.numdialog; ++i)
    {
        dialog[i].ReadFromSavegame(in.get());
    }
    return err;
}

HSaveError WriteGUI(PStream out)
{
    // GUI state
    WriteFormatTag(out, "GUIs");
    out->WriteInt32(game.numgui);
    for (int i = 0; i < game.numgui; ++i)
        guis[i].WriteToSavegame(out.get());

    WriteFormatTag(out, "GUIButtons");
    out->WriteInt32(numguibuts);
    for (int i = 0; i < numguibuts; ++i)
        guibuts[i].WriteToSavegame(out.get());

    WriteFormatTag(out, "GUILabels");
    out->WriteInt32(numguilabels);
    for (int i = 0; i < numguilabels; ++i)
        guilabels[i].WriteToSavegame(out.get());

    WriteFormatTag(out, "GUIInvWindows");
    out->WriteInt32(numguiinv);
    for (int i = 0; i < numguiinv; ++i)
        guiinv[i].WriteToSavegame(out.get());

    WriteFormatTag(out, "GUISliders");
    out->WriteInt32(numguislider);
    for (int i = 0; i < numguislider; ++i)
        guislider[i].WriteToSavegame(out.get());

    WriteFormatTag(out, "GUITextBoxes");
    out->WriteInt32(numguitext);
    for (int i = 0; i < numguitext; ++i)
        guitext[i].WriteToSavegame(out.get());

    WriteFormatTag(out, "GUIListBoxes");
    out->WriteInt32(numguilist);
    for (int i = 0; i < numguilist; ++i)
        guilist[i].WriteToSavegame(out.get());

    // Animated buttons
    WriteFormatTag(out, "AnimatedButtons");
    out->WriteInt32(numAnimButs);
    for (int i = 0; i < numAnimButs; ++i)
        animbuts[i].WriteToFile(out.get());
    return HSaveError::None();
}

HSaveError ReadGUI(PStream in, int32_t cmp_ver, const PreservedParams &pp, RestoredData &r_data)
{
    HSaveError err;
    const GuiSvgVersion svg_ver = (GuiSvgVersion)cmp_ver;
    // GUI state
    if (!AssertFormatTagStrict(err, in, "GUIs"))
        return err;
    if (!AssertGameContent(err, in->ReadInt32(), game.numgui, "GUIs"))
        return err;
    for (int i = 0; i < game.numgui; ++i)
        guis[i].ReadFromSavegame(in.get(), svg_ver);

    if (!AssertFormatTagStrict(err, in, "GUIButtons"))
        return err;
    if (!AssertGameContent(err, in->ReadInt32(), numguibuts, "GUI Buttons"))
        return err;
    for (int i = 0; i < numguibuts; ++i)
        guibuts[i].ReadFromSavegame(in.get(), svg_ver);

    if (!AssertFormatTagStrict(err, in, "GUILabels"))
        return err;
    if (!AssertGameContent(err, in->ReadInt32(), numguilabels, "GUI Labels"))
        return err;
    for (int i = 0; i < numguilabels; ++i)
        guilabels[i].ReadFromSavegame(in.get(), svg_ver);

    if (!AssertFormatTagStrict(err, in, "GUIInvWindows"))
        return err;
    if (!AssertGameContent(err, in->ReadInt32(), numguiinv, "GUI InvWindows"))
        return err;
    for (int i = 0; i < numguiinv; ++i)
        guiinv[i].ReadFromSavegame(in.get(), svg_ver);

    if (!AssertFormatTagStrict(err, in, "GUISliders"))
        return err;
    if (!AssertGameContent(err, in->ReadInt32(), numguislider, "GUI Sliders"))
        return err;
    for (int i = 0; i < numguislider; ++i)
        guislider[i].ReadFromSavegame(in.get(), svg_ver);

    if (!AssertFormatTagStrict(err, in, "GUITextBoxes"))
        return err;
    if (!AssertGameContent(err, in->ReadInt32(), numguitext, "GUI TextBoxes"))
        return err;
    for (int i = 0; i < numguitext; ++i)
        guitext[i].ReadFromSavegame(in.get(), svg_ver);

    if (!AssertFormatTagStrict(err, in, "GUIListBoxes"))
        return err;
    if (!AssertGameContent(err, in->ReadInt32(), numguilist, "GUI ListBoxes"))
        return err;
    for (int i = 0; i < numguilist; ++i)
        guilist[i].ReadFromSavegame(in.get(), svg_ver);

    // Animated buttons
    if (!AssertFormatTagStrict(err, in, "AnimatedButtons"))
        return err;
    int anim_count = in->ReadInt32();
    if (!AssertCompatLimit(err, anim_count, MAX_ANIMATING_BUTTONS, "animated buttons"))
        return err;
    numAnimButs = anim_count;
    for (int i = 0; i < numAnimButs; ++i)
        animbuts[i].ReadFromFile(in.get());
    return err;
}

HSaveError WriteInventory(PStream out)
{
    out->WriteInt32(game.numinvitems);
    for (int i = 0; i < game.numinvitems; ++i)
    {
        game.invinfo[i].WriteToSavegame(out.get());
        Properties::WriteValues(play.invProps[i], out.get());
        if (loaded_game_file_version <= kGameVersion_272)
            game.intrInv[i]->WriteTimesRunToSavedgame(out.get());
    }
    return HSaveError::None();
}

HSaveError ReadInventory(PStream in, int32_t cmp_ver, const PreservedParams &pp, RestoredData &r_data)
{
    HSaveError err;
    if (!AssertGameContent(err, in->ReadInt32(), game.numinvitems, "Inventory Items"))
        return err;
    for (int i = 0; i < game.numinvitems; ++i)
    {
        game.invinfo[i].ReadFromSavegame(in.get());
        Properties::ReadValues(play.invProps[i], in.get());
        if (loaded_game_file_version <= kGameVersion_272)
            game.intrInv[i]->ReadTimesRunFromSavedgame(in.get());
    }
    return err;
}

HSaveError WriteMouseCursors(PStream out)
{
    out->WriteInt32(game.numcursors);
    for (int i = 0; i < game.numcursors; ++i)
    {
        game.mcurs[i].WriteToSavegame(out.get());
    }
    return HSaveError::None();
}

HSaveError ReadMouseCursors(PStream in, int32_t cmp_ver, const PreservedParams &pp, RestoredData &r_data)
{
    HSaveError err;
    if (!AssertGameContent(err, in->ReadInt32(), game.numcursors, "Mouse Cursors"))
        return err;
    for (int i = 0; i < game.numcursors; ++i)
    {
        game.mcurs[i].ReadFromSavegame(in.get());
    }
    return err;
}

HSaveError WriteViews(PStream out)
{
    out->WriteInt32(game.numviews);
    for (int view = 0; view < game.numviews; ++view)
    {
        out->WriteInt32(views[view].numLoops);
        for (int loop = 0; loop < views[view].numLoops; ++loop)
        {
            out->WriteInt32(views[view].loops[loop].numFrames);
            for (int frame = 0; frame < views[view].loops[loop].numFrames; ++frame)
            {
                out->WriteInt32(views[view].loops[loop].frames[frame].sound);
                out->WriteInt32(views[view].loops[loop].frames[frame].pic);
            }
        }
    }
    return HSaveError::None();
}

HSaveError ReadViews(PStream in, int32_t cmp_ver, const PreservedParams &pp, RestoredData &r_data)
{
    HSaveError err;
    if (!AssertGameContent(err, in->ReadInt32(), game.numviews, "Views"))
        return err;
    for (int view = 0; view < game.numviews; ++view)
    {
        if (!AssertGameObjectContent(err, in->ReadInt32(), views[view].numLoops,
            "Loops", "View", view))
            return err;
        for (int loop = 0; loop < views[view].numLoops; ++loop)
        {
            if (!AssertGameObjectContent2(err, in->ReadInt32(), views[view].loops[loop].numFrames,
                "Frame", "View", view, "Loop", loop))
                return err;
            for (int frame = 0; frame < views[view].loops[loop].numFrames; ++frame)
            {
                views[view].loops[loop].frames[frame].sound = in->ReadInt32();
                views[view].loops[loop].frames[frame].pic = in->ReadInt32();
            }
        }
    }
    return err;
}

HSaveError WriteDynamicSprites(PStream out)
{
    const soff_t ref_pos = out->GetPosition();
    out->WriteInt32(0); // number of dynamic sprites
    out->WriteInt32(0); // top index
    int count = 0;
    int top_index = 1;
    for (int i = 1; i < spriteset.GetSpriteSlotCount(); ++i)
    {
        if (game.SpriteInfos[i].Flags & SPF_DYNAMICALLOC)
        {
            count++;
            top_index = i;
            out->WriteInt32(i);
            out->WriteInt32(game.SpriteInfos[i].Flags);
            serialize_bitmap(spriteset[i], out.get());
        }
    }
    const soff_t end_pos = out->GetPosition();
    out->Seek(ref_pos, kSeekBegin);
    out->WriteInt32(count);
    out->WriteInt32(top_index);
    out->Seek(end_pos, kSeekBegin);
    return HSaveError::None();
}

HSaveError ReadDynamicSprites(PStream in, int32_t cmp_ver, const PreservedParams &pp, RestoredData &r_data)
{
    HSaveError err;
    const int spr_count = in->ReadInt32();
    // ensure the sprite set is at least large enough
    // to accomodate top dynamic sprite index
    const int top_index = in->ReadInt32();
    spriteset.EnlargeTo(top_index + 1);
    for (int i = 0; i < spr_count; ++i)
    {
        int id = in->ReadInt32();
        int flags = in->ReadInt32();
        add_dynamic_sprite(id, read_serialized_bitmap(in.get()));
        game.SpriteInfos[id].Flags = flags;
    }
    return err;
}

HSaveError WriteOverlays(PStream out)
{
    out->WriteInt32(numscreenover);
    for (int i = 0; i < numscreenover; ++i)
    {
        screenover[i].WriteToFile(out.get());
        serialize_bitmap(screenover[i].pic, out.get());
    }
    return HSaveError::None();
}

HSaveError ReadOverlays(PStream in, int32_t cmp_ver, const PreservedParams &pp, RestoredData &r_data)
{
    HSaveError err;
    int over_count = in->ReadInt32();
    if (!AssertCompatLimit(err, over_count, MAX_SCREEN_OVERLAYS, "overlays"))
        return err;
    numscreenover = over_count;
    for (int i = 0; i < numscreenover; ++i)
    {
        screenover[i].ReadFromFile(in.get());
        if (screenover[i].hasSerializedBitmap)
            screenover[i].pic = read_serialized_bitmap(in.get());
    }
    return err;
}

HSaveError WriteDynamicSurfaces(PStream out)
{
    out->WriteInt32(MAX_DYNAMIC_SURFACES);
    for (int i = 0; i < MAX_DYNAMIC_SURFACES; ++i)
    {
        if (dynamicallyCreatedSurfaces[i] == nullptr)
        {
            out->WriteInt8(0);
        }
        else
        {
            out->WriteInt8(1);
            serialize_bitmap(dynamicallyCreatedSurfaces[i], out.get());
        }
    }
    return HSaveError::None();
}

HSaveError ReadDynamicSurfaces(PStream in, int32_t cmp_ver, const PreservedParams &pp, RestoredData &r_data)
{
    HSaveError err;
    if (!AssertCompatLimit(err, in->ReadInt32(), MAX_DYNAMIC_SURFACES, "Dynamic Surfaces"))
        return err;
    // Load the surfaces into a temporary array since ccUnserialiseObjects will destroy them otherwise
    r_data.DynamicSurfaces.resize(MAX_DYNAMIC_SURFACES);
    for (int i = 0; i < MAX_DYNAMIC_SURFACES; ++i)
    {
        if (in->ReadInt8() == 0)
            r_data.DynamicSurfaces[i] = nullptr;
        else
            r_data.DynamicSurfaces[i] = read_serialized_bitmap(in.get());
    }
    return err;
}

HSaveError WriteScriptModules(PStream out)
{
    // write the data segment of the global script
    int data_len = gameinst->globaldatasize;
    out->WriteInt32(data_len);
    if (data_len > 0)
        out->Write(gameinst->globaldata, data_len);
    // write the script modules data segments
    out->WriteInt32(numScriptModules);
    for (int i = 0; i < numScriptModules; ++i)
    {
        data_len = moduleInst[i]->globaldatasize;
        out->WriteInt32(data_len);
        if (data_len > 0)
            out->Write(moduleInst[i]->globaldata, data_len);
    }
    return HSaveError::None();
}

HSaveError ReadScriptModules(PStream in, int32_t cmp_ver, const PreservedParams &pp, RestoredData &r_data)
{
    HSaveError err;
    // read the global script data segment
    int data_len = in->ReadInt32();
    if (!AssertGameContent(err, data_len, pp.GlScDataSize, "global script data"))
        return err;
    r_data.GlobalScript.Len = data_len;
    r_data.GlobalScript.Data.reset(new char[data_len]);
    in->Read(r_data.GlobalScript.Data.get(), data_len);

    if (!AssertGameContent(err, in->ReadInt32(), numScriptModules, "Script Modules"))
        return err;
    r_data.ScriptModules.resize(numScriptModules);
    for (int i = 0; i < numScriptModules; ++i)
    {
        data_len = in->ReadInt32();
        if (!AssertGameObjectContent(err, data_len, pp.ScMdDataSize[i], "script module data", "module", i))
            return err;
        r_data.ScriptModules[i].Len = data_len;
        r_data.ScriptModules[i].Data.reset(new char[data_len]);
        in->Read(r_data.ScriptModules[i].Data.get(), data_len);
    }
    return err;
}

HSaveError WriteRoomStates(PStream out)
{
    // write the room state for all the rooms the player has been in
    out->WriteInt32(MAX_ROOMS);
    for (int i = 0; i < MAX_ROOMS; ++i)
    {
        if (isRoomStatusValid(i))
        {
            RoomStatus *roomstat = getRoomStatus(i);
            if (roomstat->beenhere)
            {
                out->WriteInt32(i);
                WriteFormatTag(out, "RoomState", true);
                roomstat->WriteToSavegame(out.get());
                WriteFormatTag(out, "RoomState", false);
            }
            else
                out->WriteInt32(-1);
        }
        else
            out->WriteInt32(-1);
    }
    return HSaveError::None();
}

HSaveError ReadRoomStates(PStream in, int32_t cmp_ver, const PreservedParams &pp, RestoredData &r_data)
{
    HSaveError err;
    int roomstat_count = in->ReadInt32();
    for (; roomstat_count > 0; --roomstat_count)
    {
        int id = in->ReadInt32();
        // If id == -1, then the player has not been there yet (or room state was reset)
        if (id != -1)
        {
            if (!AssertCompatRange(err, id, 0, MAX_ROOMS - 1, "room index"))
                return err;
            if (!AssertFormatTagStrict(err, in, "RoomState", true))
                return err;
            RoomStatus *roomstat = getRoomStatus(id);
            roomstat->ReadFromSavegame(in.get());
            if (!AssertFormatTagStrict(err, in, "RoomState", false))
                return err;
        }
    }
    return HSaveError::None();
}

HSaveError WriteThisRoom(PStream out)
{
    out->WriteInt32(displayed_room);
    if (displayed_room < 0)
        return HSaveError::None();

    // modified room backgrounds
    for (int i = 0; i < MAX_ROOM_BGFRAMES; ++i)
    {
        out->WriteBool(play.raw_modified[i] != 0);
        if (play.raw_modified[i])
            serialize_bitmap(thisroom.BgFrames[i].Graphic.get(), out.get());
    }
    out->WriteBool(raw_saved_screen != nullptr);
    if (raw_saved_screen)
        serialize_bitmap(raw_saved_screen, out.get());

    // room region state
    for (int i = 0; i < MAX_ROOM_REGIONS; ++i)
    {
        out->WriteInt32(thisroom.Regions[i].Light);
        out->WriteInt32(thisroom.Regions[i].Tint);
    }
    for (int i = 0; i < MAX_WALK_AREAS + 1; ++i)
    {
        out->WriteInt32(thisroom.WalkAreas[i].ScalingFar);
        out->WriteInt32(thisroom.WalkAreas[i].ScalingNear);
    }

    // room object movement paths cache
    out->WriteInt32(thisroom.ObjectCount + 1);
    for (size_t i = 0; i < thisroom.ObjectCount + 1; ++i)
    {
        mls[i].WriteToFile(out.get());
    }

    // room music volume
    out->WriteInt32(thisroom.Options.MusicVolume);

    // persistent room's indicator
    const bool persist = displayed_room < MAX_ROOMS;
    out->WriteBool(persist);
    // write the current troom state, in case they save in temporary room
    if (!persist)
        troom.WriteToSavegame(out.get());
    return HSaveError::None();
}

HSaveError ReadThisRoom(PStream in, int32_t cmp_ver, const PreservedParams &pp, RestoredData &r_data)
{
    HSaveError err;
    displayed_room = in->ReadInt32();
    if (displayed_room < 0)
        return err;

    // modified room backgrounds
    for (int i = 0; i < MAX_ROOM_BGFRAMES; ++i)
    {
        play.raw_modified[i] = in->ReadBool();
        if (play.raw_modified[i])
            r_data.RoomBkgScene[i].reset(read_serialized_bitmap(in.get()));
        else
            r_data.RoomBkgScene[i] = nullptr;
    }
    if (in->ReadBool())
        raw_saved_screen = read_serialized_bitmap(in.get());

    // room region state
    for (int i = 0; i < MAX_ROOM_REGIONS; ++i)
    {
        r_data.RoomLightLevels[i] = in->ReadInt32();
        r_data.RoomTintLevels[i] = in->ReadInt32();
    }
    for (int i = 0; i < MAX_WALK_AREAS + 1; ++i)
    {
        r_data.RoomZoomLevels1[i] = in->ReadInt32();
        r_data.RoomZoomLevels2[i] = in->ReadInt32();
    }

    // room object movement paths cache
    int objmls_count = in->ReadInt32();
    if (!AssertCompatLimit(err, objmls_count, CHMLSOFFS, "room object move lists"))
        return err;
    for (int i = 0; i < objmls_count; ++i)
    {
        err = mls[i].ReadFromFile(in.get(), cmp_ver > 0 ? 1 : 0);
        if (!err)
            return err;
    }

    // save the new room music vol for later use
    r_data.RoomVolume = (RoomVolumeMod)in->ReadInt32();

    // read the current troom state, in case they saved in temporary room
    if (!in->ReadBool())
        troom.ReadFromSavegame(in.get());

    return HSaveError::None();
}

HSaveError WriteManagedPool(PStream out)
{
    ccSerializeAllObjects(out.get());
    return HSaveError::None();
}

HSaveError ReadManagedPool(PStream in, int32_t cmp_ver, const PreservedParams &pp, RestoredData &r_data)
{
    if (ccUnserializeAllObjects(in.get(), &ccUnserializer))
    {
        return new SavegameError(kSvgErr_GameObjectInitFailed,
            String::FromFormat("Managed pool deserialization failed: %s", ccErrorString.GetCStr()));
    }
    return HSaveError::None();
}

HSaveError WritePluginData(PStream out)
{
    auto pluginFileHandle = AGSE_SAVEGAME;
    pl_set_file_handle(pluginFileHandle, out.get());
    pl_run_plugin_hooks(AGSE_SAVEGAME, pluginFileHandle);
    pl_clear_file_handle();
    return HSaveError::None();
}

HSaveError ReadPluginData(PStream in, int32_t cmp_ver, const PreservedParams &pp, RestoredData &r_data)
{
    auto pluginFileHandle = AGSE_RESTOREGAME;
    pl_set_file_handle(pluginFileHandle, in.get());
    pl_run_plugin_hooks(AGSE_RESTOREGAME, pluginFileHandle);
    pl_clear_file_handle();
    return HSaveError::None();
}


// Description of a supported game state serialization component
struct ComponentHandler
{
    String             Name;    // internal component's ID
    int32_t            Version; // current version to write and the highest supported version
    int32_t            LowestVersion; // lowest supported version that the engine can read
    HSaveError       (*Serialize)  (PStream);
    HSaveError       (*Unserialize)(PStream, int32_t cmp_ver, const PreservedParams&, RestoredData&);
};

// Array of supported components
ComponentHandler ComponentHandlers[] =
{
    {
        "Game State",
        kGSSvgVersion_3510,
        kGSSvgVersion_Initial,
        WriteGameState,
        ReadGameState
    },
    {
        "Audio",
        0,
        0,
        WriteAudio,
        ReadAudio
    },
    {
        "Characters",
        1,
        0,
        WriteCharacters,
        ReadCharacters
    },
    {
        "Dialogs",
        0,
        0,
        WriteDialogs,
        ReadDialogs
    },
    {
        "GUI",
        kGuiSvgVersion_350,
        kGuiSvgVersion_Initial,
        WriteGUI,
        ReadGUI
    },
    {
        "Inventory Items",
        0,
        0,
        WriteInventory,
        ReadInventory
    },
    {
        "Mouse Cursors",
        0,
        0,
        WriteMouseCursors,
        ReadMouseCursors
    },
    {
        "Views",
        0,
        0,
        WriteViews,
        ReadViews
    },
    {
        "Dynamic Sprites",
        0,
        0,
        WriteDynamicSprites,
        ReadDynamicSprites
    },
    {
        "Overlays",
        0,
        0,
        WriteOverlays,
        ReadOverlays
    },
    {
        "Dynamic Surfaces",
        0,
        0,
        WriteDynamicSurfaces,
        ReadDynamicSurfaces
    },
    {
        "Script Modules",
        0,
        0,
        WriteScriptModules,
        ReadScriptModules
    },
    {
        "Room States",
        0,
        0,
        WriteRoomStates,
        ReadRoomStates
    },
    {
        "Loaded Room State",
        1,
        0,
        WriteThisRoom,
        ReadThisRoom
    },
    {
        "Managed Pool",
        0,
        0,
        WriteManagedPool,
        ReadManagedPool
    },
    {
        "Plugin Data",
        0,
        0,
        WritePluginData,
        ReadPluginData
    },
    { nullptr, 0, 0, nullptr, nullptr } // end of array
};


typedef std::map<String, ComponentHandler> HandlersMap;
void GenerateHandlersMap(HandlersMap &map)
{
    map.clear();
    for (int i = 0; !ComponentHandlers[i].Name.IsEmpty(); ++i)
        map[ComponentHandlers[i].Name] = ComponentHandlers[i];
}

// A helper struct to pass to (de)serialization handlers
struct SvgCmpReadHelper
{
    SavegameVersion       Version;  // general savegame version
    const PreservedParams &PP;      // previous game state kept for reference
    RestoredData          &RData;   // temporary storage for loaded data, that
                                    // will be applied after loading is done
    // The map of serialization handlers, one per supported component type ID
    HandlersMap            Handlers;

    SvgCmpReadHelper(SavegameVersion svg_version, const PreservedParams &pp, RestoredData &r_data)
        : Version(svg_version)
        , PP(pp)
        , RData(r_data)
    {
    }
};

// The basic information about deserialized component, used for debugging purposes
struct ComponentInfo
{
    String  Name;       // internal component's ID
    int32_t Version;    // data format version
    soff_t  Offset;     // offset at which an opening tag is located
    soff_t  DataOffset; // offset at which component data begins
    soff_t  DataSize;   // expected size of component data

    ComponentInfo() : Version(-1), Offset(0), DataOffset(0), DataSize(0) {}
};

HSaveError ReadComponent(PStream in, SvgCmpReadHelper &hlp, ComponentInfo &info)
{
    info = ComponentInfo(); // reset in case of early error
    info.Offset = in->GetPosition();
    if (!ReadFormatTag(in, info.Name, true))
        return new SavegameError(kSvgErr_ComponentOpeningTagFormat);
    info.Version = in->ReadInt32();
    info.DataSize = hlp.Version >= kSvgVersion_Cmp_64bit ? in->ReadInt64() : in->ReadInt32();
    info.DataOffset = in->GetPosition();

    const ComponentHandler *handler = nullptr;
    std::map<String, ComponentHandler>::const_iterator it_hdr = hlp.Handlers.find(info.Name);
    if (it_hdr != hlp.Handlers.end())
        handler = &it_hdr->second;

    if (!handler || !handler->Unserialize)
        return new SavegameError(kSvgErr_UnsupportedComponent);
    if (info.Version > handler->Version || info.Version < handler->LowestVersion)
        return new SavegameError(kSvgErr_UnsupportedComponentVersion, String::FromFormat("Saved version: %d, supported: %d - %d", info.Version, handler->LowestVersion, handler->Version));
    HSaveError err = handler->Unserialize(in, info.Version, hlp.PP, hlp.RData);
    if (!err)
        return err;
    if (in->GetPosition() - info.DataOffset != info.DataSize)
        return new SavegameError(kSvgErr_ComponentSizeMismatch, String::FromFormat("Expected: %lld, actual: %lld", info.DataSize, in->GetPosition() - info.DataOffset));
    if (!AssertFormatTag(in, info.Name, false))
        return new SavegameError(kSvgErr_ComponentClosingTagFormat);
    return HSaveError::None();
}

HSaveError ReadAll(PStream in, SavegameVersion svg_version, const PreservedParams &pp, RestoredData &r_data)
{
    // Prepare a helper struct we will be passing to the block reading proc
    SvgCmpReadHelper hlp(svg_version, pp, r_data);
    GenerateHandlersMap(hlp.Handlers);

    size_t idx = 0;
    if (!AssertFormatTag(in, ComponentListTag, true))
        return new SavegameError(kSvgErr_ComponentListOpeningTagFormat);
    do
    {
        // Look out for the end of the component list:
        // this is the only way how this function ends with success
        soff_t off = in->GetPosition();
        if (AssertFormatTag(in, ComponentListTag, false))
            return HSaveError::None();
        // If the list's end was not detected, then seek back and continue reading
        in->Seek(off, kSeekBegin);

        ComponentInfo info;
        HSaveError err = ReadComponent(in, hlp, info);
        if (!err)
        {
            return new SavegameError(kSvgErr_ComponentUnserialization,
                String::FromFormat("(#%d) %s, version %i, at offset %u.",
                idx, info.Name.IsEmpty() ? "unknown" : info.Name.GetCStr(), info.Version, info.Offset),
                err);
        }

        idx++;
    }
    while (!in->EOS());
    return new SavegameError(kSvgErr_ComponentListClosingTagMissing);
}

HSaveError WriteComponent(PStream out, ComponentHandler &hdlr)
{
    WriteFormatTag(out, hdlr.Name, true);
    out->WriteInt32(hdlr.Version);
    soff_t ref_pos = out->GetPosition();
    out->WriteInt64(0); // placeholder for the component size
    HSaveError err = hdlr.Serialize(out);
    soff_t end_pos = out->GetPosition();
    out->Seek(ref_pos, kSeekBegin);
    out->WriteInt64(end_pos - ref_pos - sizeof(int64_t)); // size of serialized component data
    out->Seek(end_pos, kSeekBegin);
    if (err)
        WriteFormatTag(out, hdlr.Name, false);
    return err;
}

HSaveError WriteAllCommon(PStream out)
{
    WriteFormatTag(out, ComponentListTag, true);
    for (int type = 0; !ComponentHandlers[type].Name.IsEmpty(); ++type)
    {
        HSaveError err = WriteComponent(out, ComponentHandlers[type]);
        if (!err)
        {
            return new SavegameError(kSvgErr_ComponentSerialization,
                String::FromFormat("Component: (#%d) %s", type, ComponentHandlers[type].Name.GetCStr()),
                err);
        }

    }
    WriteFormatTag(out, ComponentListTag, false);
    return HSaveError::None();
}

} // namespace SavegameBlocks
} // namespace Engine
} // namespace AGS
