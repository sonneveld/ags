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

#include "core/platform.h"
#include "ac/common.h"
#include "ac/display.h"
#include "ac/draw.h"
#include "ac/game_version.h"
#include "ac/gamesetup.h"
#include "ac/gamesetupstruct.h"
#include "ac/gamestate.h"
#include "ac/runtime_defines.h"
#include "ac/walkbehind.h"
#include "ac/dynobj/scriptsystem.h"
#include "debug/out.h"
#include "device/mousew32.h"
#include "font/fonts.h"
#include "gfx/ali3dexception.h"
#include "gfx/graphicsdriver.h"
#include "gui/guimain.h"
#include "gui/guiinv.h"
#include "main/graphics_mode.h"
#include "main/engine_setup.h"
#include "media/video/video.h"
#include "platform/base/agsplatformdriver.h"

using namespace AGS::Common;
using namespace AGS::Engine;

extern GameSetupStruct game;
extern ScriptSystem scsystem;
extern int _places_r, _places_g, _places_b;
extern IGraphicsDriver *gfxDriver;

int convert_16bit_bgr = 0;

// Convert guis position and size to proper game resolution.
// Necessary for pre 3.1.0 games only to sync with modern engine.
void convert_gui_to_game_resolution(GameDataVersion filever)
{
    if (filever > kGameVersion_310)
        return;

    const int mul = game.GetDataUpscaleMult();
    for (int i = 0; i < game.numcursors; ++i)
    {
        game.mcurs[i].hotx *= mul;
        game.mcurs[i].hoty *= mul;
    }

    for (int i = 0; i < game.numinvitems; ++i)
    {
        game.invinfo[i].hotx *= mul;
        game.invinfo[i].hoty *= mul;
    }

    for (int i = 0; i < game.numgui; ++i)
    {
        GUIMain*cgp = &guis[i];
        cgp->X *= mul;
        cgp->Y *= mul;
        if (cgp->Width < 1)
            cgp->Width = 1;
        if (cgp->Height < 1)
            cgp->Height = 1;
        // This is probably a way to fix GUIs meant to be covering whole screen
        if (cgp->Width == game.GetDataRes().Width - 1)
            cgp->Width = game.GetDataRes().Width;

        cgp->Width *= mul;
        cgp->Height *= mul;

        cgp->PopupAtMouseY *= mul;

        for (int j = 0; j < cgp->GetControlCount(); ++j)
        {
            GUIObject *guio = cgp->GetControl(j);
            guio->X *= mul;
            guio->Y *= mul;
            guio->Width *= mul;
            guio->Height *= mul;
            guio->IsActivated = false;
        }
    }
}

// Convert certain coordinates to data resolution (only if it's different from game resolution).
// Necessary for 3.1.0 and above games with legacy "low-res coordinates" setting.
void convert_objects_to_data_resolution(GameDataVersion filever)
{
    if (filever < kGameVersion_310 || game.GetDataUpscaleMult() == 1)
        return;

    const int mul = game.GetDataUpscaleMult();
    for (int i = 0; i < game.numcharacters; ++i) 
    {
        game.chars[i].x /= mul;
        game.chars[i].y /= mul;
    }

    for (int i = 0; i < numguiinv; ++i)
    {
        guiinv[i].ItemWidth /= mul;
        guiinv[i].ItemHeight /= mul;
    }
}

static void engine_setup_system_gamesize()
{
    scsystem.width = game.GetGameRes().Width;
    scsystem.height = game.GetGameRes().Height;
    scsystem.viewport_width = game_to_data_coord(play.GetMainViewport().GetWidth());
    scsystem.viewport_height = game_to_data_coord(play.GetMainViewport().GetHeight());
}

void engine_init_resolution_settings(const Size game_size)
{
    Debug::Printf("Initializing resolution settings");

    // Initialize default viewports and room camera
    Rect viewport = RectWH(game_size);
    play.SetMainViewport(viewport);
    play.SetUIViewport(viewport);
    play.SetRoomViewport(0, viewport);
    play.GetRoomCamera(0)->SetSize(viewport.GetSize());

    usetup.textheight = getfontheight_outlined(0) + 1;

    Debug::Printf(kDbgMsg_Init, "Game native resolution: %d x %d (%d bit)%s", game_size.Width, game_size.Height, game.color_depth * 8,
        game.IsLegacyLetterbox() ? " letterbox-by-design" : "");

    convert_gui_to_game_resolution(loaded_game_file_version);
    convert_objects_to_data_resolution(loaded_game_file_version);

    engine_setup_system_gamesize();
}

// Setup gfx driver callbacks and options
static void engine_post_gfxmode_driver_setup()
{
    gfxDriver->SetCallbackForPolling(nullptr);
    gfxDriver->SetCallbackToDrawScreen(draw_screen_callback);
    gfxDriver->SetCallbackForNullSprite(GfxDriverNullSpriteCallback);
}

// Reset gfx driver callbacks
static void engine_pre_gfxmode_driver_cleanup()
{
    gfxDriver->SetCallbackForPolling(nullptr);
    gfxDriver->SetCallbackToDrawScreen(nullptr);
    gfxDriver->SetCallbackForNullSprite(nullptr);
    gfxDriver->SetMemoryBackBuffer(nullptr);
}

// Setup virtual screen
static void engine_post_gfxmode_screen_setup(const DisplayMode &dm, bool recreate_bitmaps)
{
    if (recreate_bitmaps)
    {
        // TODO: find out if 
        // - we need to support this case at all;
        // - if yes then which bitmaps need to be recreated (probably only video bitmaps and textures?)
    }
}

static void engine_pre_gfxmode_screen_cleanup()
{
}

// Release virtual screen
static void engine_pre_gfxsystem_screen_destroy()
{
    delete sub_vscreen;
    sub_vscreen = nullptr;
}

// Setup color conversion parameters
static void engine_setup_color_conversions(int coldepth)
{
    set_color_conversion(COLORCONV_MOST | COLORCONV_EXPAND_256);
}

// Setup drawing modes and color conversions;
// they depend primarily on gfx driver capabilities and new color depth
static void engine_post_gfxmode_draw_setup(const DisplayMode &dm)
{
    engine_setup_color_conversions(dm.ColorDepth);
    init_draw_method();
}

// Cleanup auxiliary drawing objects
static void engine_pre_gfxmode_draw_cleanup()
{
    dispose_draw_method();
}

// Setup mouse control mode and graphic area
static void engine_post_gfxmode_mouse_setup(const DisplayMode &dm, const Size &init_desktop)
{
#ifdef AGS_DELETE_FOR_3_6

    // Assign mouse control parameters.
    //
    // Whether mouse movement should be controlled by the engine - this is
    // determined based on related config option.
    const bool should_control_mouse = usetup.mouse_control == kMouseCtrl_Always ||
        (usetup.mouse_control == kMouseCtrl_Fullscreen && !dm.Windowed);
    // Whether mouse movement control is supported by the engine - this is
    // determined on per platform basis. Some builds may not have such
    // capability, e.g. because of how backend library implements mouse utils.
    const bool can_control_mouse = platform->IsMouseControlSupported(dm.Windowed);
    // The resulting choice is made based on two aforementioned factors.
    const bool control_sens = should_control_mouse && can_control_mouse;
    if (control_sens)
    {
        Mouse::EnableControl(!dm.Windowed);
        Mouse::SetSpeedUnit(1.f);
        if (usetup.mouse_speed_def == kMouseSpeed_CurrentDisplay)
        {
            Size cur_desktop;
            if (get_desktop_resolution(&cur_desktop.Width, &cur_desktop.Height) == 0)
                Mouse::SetSpeedUnit(Math::Max((float)cur_desktop.Width / (float)init_desktop.Width,
                                              (float)cur_desktop.Height / (float)init_desktop.Height));
        }
        Mouse::SetSpeed(usetup.mouse_speed);
    }
    Debug::Printf(kDbgMsg_Init, "Mouse control: %s, base: %f, speed: %f", Mouse::IsControlEnabled() ? "on" : "off",
        Mouse::GetSpeedUnit(), Mouse::GetSpeed());

#endif

    // Since we're always use desktop resolution, we don't need to adjust mouse acceleration
    Mouse::DisableControl();

    on_coordinates_scaling_changed();

    // If auto lock option is set, lock mouse to the game window
    if (usetup.mouse_auto_lock && scsystem.windowed != 0)
        Mouse::TryLockToWindow();
}

// Reset mouse controls before changing gfx mode
static void engine_pre_gfxmode_mouse_cleanup()
{
    // Always disable mouse control and unlock mouse when releasing down gfx mode
    Mouse::DisableControl();
    Mouse::UnlockFromWindow();
}

// Fill in scsystem struct with display mode parameters
static void engine_setup_scsystem_screen(const DisplayMode &dm)
{
    scsystem.coldepth = dm.ColorDepth;
    scsystem.windowed = dm.Windowed;
    scsystem.vsync = dm.Vsync;
}

void engine_post_gfxmode_setup(const Size &init_desktop)
{
    DisplayMode dm = gfxDriver->GetDisplayMode();
    // If color depth has changed (or graphics mode was inited for the
    // very first time), we also need to recreate bitmaps
    bool has_driver_changed = scsystem.coldepth != dm.ColorDepth;

    engine_setup_scsystem_screen(dm);
    engine_post_gfxmode_driver_setup();
    engine_post_gfxmode_screen_setup(dm, has_driver_changed);
    if (has_driver_changed)
        engine_post_gfxmode_draw_setup(dm);
    engine_post_gfxmode_mouse_setup(dm, init_desktop);
    
    // TODO: the only reason this call was put here is that it requires
    // "windowed" flag to be specified. Find out whether this function
    // has anything to do with graphics mode at all. It is quite possible
    // that we may split it into two functions, or remove parameter.
    platform->PostAllegroInit(scsystem.windowed != 0);

    video_on_gfxmode_changed();
}

void engine_pre_gfxmode_release()
{
    engine_pre_gfxmode_mouse_cleanup();
    engine_pre_gfxmode_driver_cleanup();
    engine_pre_gfxmode_screen_cleanup();
}

void engine_pre_gfxsystem_shutdown()
{
    engine_pre_gfxmode_release();
    engine_pre_gfxmode_draw_cleanup();
    engine_pre_gfxsystem_screen_destroy();
}

void on_coordinates_scaling_changed()
{
    // Reset mouse graphic area and bounds
    Mouse::SetGraphicArea();
    // If mouse bounds do not have valid values yet, then limit cursor to viewport
    if (play.mboundx1 == 0 && play.mboundy1 == 0 && play.mboundx2 == 0 && play.mboundy2 == 0)
        Mouse::SetMoveLimit(play.GetMainViewport());
    else
        Mouse::SetMoveLimit(Rect(play.mboundx1, play.mboundy1, play.mboundx2, play.mboundy2));
}
