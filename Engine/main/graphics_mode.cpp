// SDL-TODO: Perhaps save in an sdl2 specific version?

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

//
// Graphics initialization
//

#include <algorithm>
#include "core/platform.h"
#include "ac/draw.h"
#include "debug/debugger.h"
#include "debug/out.h"
#include "gfx/ali3dexception.h"
#include "gfx/bitmap.h"
#include "gfx/gfxdriverfactory.h"
#include "gfx/gfxfilter.h"
#include "gfx/graphicsdriver.h"
#include "main/config.h"
#include "main/engine_setup.h"
#include "main/graphics_mode.h"
#include "main/main_allegro.h"
#include "platform/base/agsplatformdriver.h"

using namespace AGS::Common;
using namespace AGS::Engine;

extern int proper_exit;
extern AGSPlatformDriver *platform;
extern IGraphicsDriver *gfxDriver;

void GfxDriverOnSurfaceUpdate();
bool graphics_mode_update_render_frame();

static IGfxDriverFactory *GfxFactory = nullptr;

// Last saved fullscreen and windowed configs; they are used when switching
// between between fullscreen and windowed modes at runtime.
// If particular mode is modified, e.g. by script command, related config should be overwritten.
static GameFrameSetup FullscreenFrame;
static GameFrameSetup WindowedFrame;
// The game-to-screen transformation
PlaneScaling       GameScaling;


GameFrameSetup::GameFrameSetup()
    : ScaleDef(kFrame_IntScale)
    , ScaleFactor(1)
{
}

GameFrameSetup::GameFrameSetup(FrameScaleDefinition def, int factor)
    : ScaleDef(def)
    , ScaleFactor(factor)
{
}

bool GameFrameSetup::IsValid() const
{
    return ScaleDef != kFrame_IntScale || ScaleFactor > 0;
}

ScreenSizeSetup::ScreenSizeSetup()
    : SizeDef(kScreenDef_MaxDisplay)
    , MatchDeviceRatio(true)
{
}

DisplayModeSetup::DisplayModeSetup()
    : RefreshRate(0)
    , VSync(false)
    , Windowed(false)
{
}


Size get_desktop_size()
{
    Size sz;
    get_desktop_resolution(&sz.Width, &sz.Height);
    return sz;
}

bool simple_create_gfx_driver_and_init_mode(const String &gfx_driver_id,
                                            const Size &game_size,
                                            const DisplayModeSetup &dm_setup,
                                            const ColorDepthOption &color_depth,
                                            const GameFrameSetup &win_frame_setup,
                                            const GameFrameSetup &fs_frame_setup,
                                            const GfxFilterSetup &filter_setup)
{
    if (game_size.IsNull())
        return false;
    if (!win_frame_setup.IsValid())
        return false;
    if (!fs_frame_setup.IsValid())
        return false;

    GfxFactory = GetGfxDriverFactory(gfx_driver_id);
    if (!GfxFactory) {
        Debug::Printf(kDbgMsg_Error, "Failed to initialize %s graphics factory. Error: %s", gfx_driver_id.GetCStr(), get_allegro_error());
        return false;
    }
    gfxDriver = GfxFactory->GetDriver();
    if (!gfxDriver) {
        Debug::Printf(kDbgMsg_Error, "Failed to create graphics driver. Error: %s", get_allegro_error());
        return false;
    }

    // both callbacks used to notify plugins of events
    gfxDriver->SetCallbackOnInit(GfxDriverOnInitCallback);
    gfxDriver->SetCallbackOnSurfaceUpdate(GfxDriverOnSurfaceUpdate);
    // must be called before SetDisplayMode because it determines shaders loaded
    gfxDriver->SetTintMethod(TintReColourise);

    // Tell Allegro new default bitmap color depth (must be done before set_gfx_mode)
    // TODO: this is also done inside ALSoftwareGraphicsDriver implementation; can remove one?
    set_color_depth(color_depth.Bits);


    auto dm = DisplayMode(GraphicResolution(game_size.Width, game_size.Height, color_depth.Bits),
                   dm_setup.Windowed, dm_setup.RefreshRate, dm_setup.VSync);

    if (!gfxDriver->SetDisplayMode(dm, nullptr)) {
        Debug::Printf(kDbgMsg_Error, "Failed to init gfx mode. Error: %s", get_allegro_error());
        return false;
    }


    //  set native size, the source
    if (!gfxDriver->SetNativeSize(game_size)) { return false; }


    // render frame, the dest
    WindowedFrame = win_frame_setup;
    FullscreenFrame = fs_frame_setup;
    graphics_mode_update_render_frame();

    String filter_error;
    if (!GfxFactory->SetFilter(filter_setup.ID, filter_error)) { 
        Debug::Printf(kDbgMsg_Error, "Unable to set graphics filter '%s'. Error: %s", filter_setup.ID.GetCStr(), filter_error.GetCStr());
        return false;
    }


    auto rdm = gfxDriver->GetDisplayMode();
    Debug::Printf("Graphics factory: %s", gfx_driver_id.GetCStr());
    Debug::Printf("Graphics driver: %s", gfxDriver->GetDriverName());
    Debug::Printf("Using gfx mode %d x %d (%d-bit) %s",
        rdm.Width, rdm.Height, rdm.ColorDepth, rdm.Windowed ? "windowed" : "fullscreen");
    Debug::Printf("Using gfx filter: '%s'", gfxDriver->GetGraphicsFilter()->GetInfo().Id.GetCStr());

    return true;
}


void display_gfx_mode_error(const Size &game_size, const ScreenSetup &setup, const int color_depth)
{
    proper_exit=1;
    platform->FinishedUsingGraphicsMode();

    String main_error;
    ScreenSizeSetup scsz = setup.DisplayMode.ScreenSize;
    PGfxFilter filter = gfxDriver ? gfxDriver->GetGraphicsFilter() : PGfxFilter();
    Size wanted_screen;
    if (scsz.SizeDef == kScreenDef_Explicit)
        main_error.Format("There was a problem initializing graphics mode %d x %d (%d-bit), or finding nearest compatible mode, with game size %d x %d and filter '%s'.",
            scsz.Size.Width, scsz.Size.Height, color_depth, game_size.Width, game_size.Height, filter ? filter->GetInfo().Id.GetCStr() : "Undefined");
    else
        main_error.Format("There was a problem finding and/or creating valid graphics mode for game size %d x %d (%d-bit) and requested filter '%s'.",
            game_size.Width, game_size.Height, color_depth, setup.Filter.UserRequest.IsEmpty() ? "Undefined" : setup.Filter.UserRequest.GetCStr());

    platform->DisplayAlert("%s\n"
            "(Problem: '%s')\n"
            "Try to correct the problem, or seek help from the AGS homepage."
            "%s",
            main_error.GetCStr(), get_allegro_error(), platform->GetGraphicsTroubleshootingText());
}

bool graphics_mode_init_any(const Size game_size, const ScreenSetup &setup, const ColorDepthOption &color_depth)
{
    {   
    // debug printing
    const char *screen_sz_def_options[kNumScreenDef] = { "explicit", "scaling", "max" };
    ScreenSizeSetup scsz = setup.DisplayMode.ScreenSize;
    const bool ignore_device_ratio = setup.DisplayMode.Windowed || scsz.SizeDef == kScreenDef_Explicit;
    GameFrameSetup gameframe = setup.DisplayMode.Windowed ? setup.WinGameFrame : setup.FsGameFrame;
    const String scale_option = make_scaling_option(gameframe);
    Debug::Printf(kDbgMsg_Init, "Graphic settings: driver: %s, windowed: %s, screen def: %s, screen size: %d x %d, match device ratio: %s, game scale: %s",
        setup.DriverID.GetCStr(),
        setup.DisplayMode.Windowed ? "yes" : "no", screen_sz_def_options[scsz.SizeDef],
        scsz.Size.Width, scsz.Size.Height,
        ignore_device_ratio ? "ignore" : (scsz.MatchDeviceRatio ? "yes" : "no"), scale_option.GetCStr());
    }

    auto result = simple_create_gfx_driver_and_init_mode (setup.DriverID, game_size, setup.DisplayMode, color_depth, setup.WinGameFrame, setup.FsGameFrame, setup.Filter);
    if (!result)
    {
        display_gfx_mode_error(game_size, setup, color_depth.Bits);
        return false;
    }

    return true;
}

void GfxDriverOnSurfaceUpdate()
{
    // Resize render frame using current scaling settings
    graphics_mode_update_render_frame();
    on_coordinates_scaling_changed();
}

uint32_t convert_scaling_to_fp(int scale_factor)
{
    if (scale_factor >= 0)
        return scale_factor <<= kShift;
    else
        return kUnit / abs(scale_factor);
}

Size set_game_frame_after_screen_size(const Size &game_size, const Size screen_size, const GameFrameSetup &setup)
{
    // Set game frame as native game resolution scaled by particular method
    Size frame_size;
    if (setup.ScaleDef == kFrame_MaxStretch)
    {
        frame_size = screen_size;
    }
    else if (setup.ScaleDef == kFrame_MaxProportional)
    {
        frame_size = ProportionalStretch(screen_size, game_size);
    }
    else
    {
        int scale;
        if (setup.ScaleDef == kFrame_MaxRound)
            scale = Math::Min((screen_size.Width / game_size.Width) << kShift,
                              (screen_size.Height / game_size.Height) << kShift);
        else
            scale = convert_scaling_to_fp(setup.ScaleFactor);

        // Ensure scaling factors are sane
        if (scale <= 0)
            scale = kUnit;

        frame_size = Size((game_size.Width * scale) >> kShift, (game_size.Height * scale) >> kShift);
        // If the scaled game size appear larger than the screen,
        // use "proportional stretch" method instead
        if (frame_size.ExceedsByAny(screen_size))
            frame_size = ProportionalStretch(screen_size, game_size);
    }
    return frame_size;
}

bool graphics_mode_update_render_frame()
{
    if (!gfxDriver) { return false; }
    if (!gfxDriver->IsModeSet()) { return false; }
    if (!gfxDriver->IsNativeSizeValid()) { return false; }

    DisplayMode dm = gfxDriver->GetDisplayMode();
    Size screen_size = Size(dm.Width, dm.Height);
    Size native_size = gfxDriver->GetNativeSize();

    GameFrameSetup CurFrameSetup = dm.Windowed ? WindowedFrame : FullscreenFrame;

    Size frame_size = set_game_frame_after_screen_size(native_size, screen_size, CurFrameSetup);
    Rect render_frame = CenterInRect(RectWH(screen_size), RectWH(frame_size));

    if (!gfxDriver->SetRenderFrame(render_frame))
    {
        Debug::Printf(kDbgMsg_Error, "Failed to set render frame (%d, %d, %d, %d : %d x %d). Error: %s", 
            render_frame.Left, render_frame.Top, render_frame.Right, render_frame.Bottom,
            render_frame.GetWidth(), render_frame.GetHeight(), get_allegro_error());
        return false;
    }

    Rect dst_rect = gfxDriver->GetRenderDestination();
    Debug::Printf("Render frame set, render dest (%d, %d, %d, %d : %d x %d)",
        dst_rect.Left, dst_rect.Top, dst_rect.Right, dst_rect.Bottom, dst_rect.GetWidth(), dst_rect.GetHeight());
    // init game scaling transformation
    GameScaling.Init(native_size, gfxDriver->GetRenderDestination());
    return true;
}

void graphics_mode_toggle_full_screen()
{
    gfxDriver->ToggleFullscreen();
    graphics_mode_update_render_frame();
}

void graphics_mode_on_window_size_changed()
{
    Size ignored;
    gfxDriver->UpdateDeviceScreen(ignored);
    graphics_mode_update_render_frame();
}

void graphics_mode_shutdown()
{
    if (GfxFactory)
        GfxFactory->Shutdown();
    GfxFactory = nullptr;
    gfxDriver = nullptr;

    // Tell Allegro that we are no longer in graphics mode
    set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
}
