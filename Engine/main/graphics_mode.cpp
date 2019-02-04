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

// Don't try to figure out the window size on the mac because the port resizes itself.
#if defined(MAC_VERSION) || defined(ALLEGRO_SDL2) || defined(IOS_VERSION) || defined(PSP_VERSION) || defined(ANDROID_VERSION)
#define USE_SIMPLE_GFX_INIT
#endif

using namespace AGS::Common;
using namespace AGS::Engine;

extern int proper_exit;
extern AGSPlatformDriver *platform;
extern IGraphicsDriver *gfxDriver;


IGfxDriverFactory *GfxFactory = NULL;

// Current frame scaling setup
GameFrameSetup     CurFrameSetup;
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
    if (!gfxDriver || !gfxDriver->IsModeSet() || !gfxDriver->IsNativeSizeValid())
        return false;

    DisplayMode dm = gfxDriver->GetDisplayMode();
    Size screen_size = Size(dm.Width, dm.Height);

    Size native_size = gfxDriver->GetNativeSize();

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

bool graphics_mode_set_filter(const String &filter_id)
{
    if (!GfxFactory)
        return false;

    String filter_error;
    PGfxFilter filter = GfxFactory->SetFilter(filter_id, filter_error);
    if (!filter)
    {
        Debug::Printf(kDbgMsg_Error, "Unable to set graphics filter '%s'. Error: %s", filter_id.GetCStr(), filter_error.GetCStr());
        return false;
    }
    Rect filter_rect  = filter->GetDestination();
    Debug::Printf("Graphics filter set: '%s', filter dest (%d, %d, %d, %d : %d x %d)", filter->GetInfo().Id.GetCStr(),
        filter_rect.Left, filter_rect.Top, filter_rect.Right, filter_rect.Bottom, filter_rect.GetWidth(), filter_rect.GetHeight());
    return true;
}

void GfxDriverOnSurfaceUpdate()
{
    // Resize render frame using current scaling settings
    graphics_mode_update_render_frame();
    on_coordinates_scaling_changed();  // mouse
}

bool simple_create_gfx_driver_and_init_mode(const String &gfx_driver_id,
                                            const Size &game_size,
                                            const DisplayModeSetup &dm_setup,
                                            const ColorDepthOption &color_depth,
                                            const GameFrameSetup &frame_setup,
                                            const GfxFilterSetup &filter_setup)
{

    if (game_size.IsNull())
        return false;
    if (!frame_setup.IsValid())
        return false;

    GfxFactory = GetGfxDriverFactory(gfx_driver_id);
    if (!GfxFactory) {
        Debug::Printf(kDbgMsg_Error, "Failed to initialize %s graphics factory. Error: %s", gfx_driver_id.GetCStr(), get_allegro_error());
        return false;
    }
    Debug::Printf("Graphics factory: %s", gfx_driver_id.GetCStr());
    gfxDriver = GfxFactory->GetDriver();
    if (!gfxDriver) {
        Debug::Printf(kDbgMsg_Error, "Failed to create graphics driver. Error: %s", get_allegro_error());
        return false;
    }
    Debug::Printf("Graphics driver: %s", gfxDriver->GetDriverName());

    // both callbacks used to notify plugins of events
    gfxDriver->SetCallbackOnInit(GfxDriverOnInitCallback);
    gfxDriver->SetCallbackOnSurfaceUpdate(GfxDriverOnSurfaceUpdate);
    
    DisplayMode dm(GraphicResolution(game_size.Width, game_size.Height, color_depth.Bits),
                   dm_setup.Windowed, dm_setup.RefreshRate, dm_setup.VSync);

    // Tell Allegro new default bitmap color depth (must be done before set_gfx_mode)
    // TODO: this is also done inside ALSoftwareGraphicsDriver implementation; can remove one?
    set_color_depth(dm.ColorDepth);

    // TODO: this is remains of the old code; find out if this is really
    // the best time and place to set the tint method
    gfxDriver->SetTintMethod(TintReColourise);  // must be called before SetDisplayMode because it determins shaders loaded

    if (!gfxDriver->SetDisplayMode(dm, nullptr)) {
        Debug::Printf(kDbgMsg_Error, "Failed to init gfx mode. Error: %s", get_allegro_error());
        return false;
    }

    {
        DisplayMode rdm = gfxDriver->GetDisplayMode();
        Debug::Printf("Using gfx mode %d x %d (%d-bit) %s",
            rdm.Width, rdm.Height, rdm.ColorDepth, rdm.Windowed ? "windowed" : "fullscreen");
    }


    //  set native size, the source
    const Size &native_size = game_size;

    if (!gfxDriver->SetNativeSize(native_size))
        return false;


    // render frame, the dest
    CurFrameSetup = frame_setup;

    graphics_mode_update_render_frame();


    // filters
    Debug::Printf("Requested gfx filter: %s", filter_setup.UserRequest.GetCStr());
    if (!graphics_mode_set_filter(filter_setup.ID))
    {
        String def_filter = GfxFactory->GetDefaultFilterID();
        if (def_filter.CompareNoCase(filter_setup.ID) == 0)
            return false;
        Debug::Printf(kDbgMsg_Error, "Failed to apply gfx filter: %s; will try to use factory default filter '%s' instead",
                filter_setup.UserRequest.GetCStr(), def_filter.GetCStr());
        if (!graphics_mode_set_filter(def_filter))
            return false;
    }
    Debug::Printf("Using gfx filter: %s", GfxFactory->GetDriver()->GetGraphicsFilter()->GetInfo().Id.GetCStr());

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
    GameFrameSetup gameframe = setup.DisplayMode.Windowed ? setup.WinGameFrame : setup.FsGameFrame;

    {   
        // debug printing
        const char *screen_sz_def_options[kNumScreenDef] = { "explicit", "scaling", "max" };
        ScreenSizeSetup scsz = setup.DisplayMode.ScreenSize;
        const bool ignore_device_ratio = setup.DisplayMode.Windowed || scsz.SizeDef == kScreenDef_Explicit;
        const String scale_option = make_scaling_option(gameframe);
        Debug::Printf(kDbgMsg_Init, "Game settings: windowed = %s, screen def: %s, screen size: %d x %d, match device ratio: %s, game scale: %s",
            setup.DisplayMode.Windowed ? "yes" : "no", screen_sz_def_options[scsz.SizeDef],
            scsz.Size.Width, scsz.Size.Height,
            ignore_device_ratio ? "ignore" : (scsz.MatchDeviceRatio ? "yes" : "no"), scale_option.GetCStr());
    }

    // Prepare the list of available gfx factories, having the one requested by user at first place
    // TODO: make factory & driver IDs case-insensitive!
    StringV ids;
    GetGfxDriverFactoryNames(ids);
    StringV::iterator it = std::find(ids.begin(), ids.end(), setup.DriverID);
    if (it != ids.end())
        std::rotate(ids.begin(), it, ids.end());
    else
        Debug::Printf(kDbgMsg_Error, "Requested graphics driver '%s' not found, will try existing drivers instead", setup.DriverID.GetCStr());

    // Try to create renderer and init gfx mode, choosing one factory at a time
    bool result = false;
    for (StringV::const_iterator it = ids.begin(); it != ids.end(); ++it)
    {
        result = simple_create_gfx_driver_and_init_mode (*it, game_size, setup.DisplayMode, color_depth, gameframe, setup.Filter);

        if (result)
            break;
        graphics_mode_shutdown();
    }
    // If all possibilities failed, display error message and quit
    if (!result)
    {
        display_gfx_mode_error(game_size, setup, color_depth.Bits);
        return false;
    }
    return true;
}


void graphics_mode_shutdown()
{
    if (GfxFactory)
        GfxFactory->Shutdown();
    GfxFactory = NULL;
    gfxDriver = NULL;

    // Tell Allegro that we are no longer in graphics mode
    set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
}
