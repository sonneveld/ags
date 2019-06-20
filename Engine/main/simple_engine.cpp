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

/*
!!TODO
- replace std::runtime_error with something else
- then ensure we quit with an error
- support opengl 3.0
- sprite sheets
- support the config objects (but ensure all config work and file searching is done outside)
- start extracting some global variables.. maybe pass by reference?
- renderer class (init, render scene graph)
- take screen shots! (set a variable, no delay, and save in buffer)
- copy the scene graph so we can re-use it.
- which might mean we need to use shared_ptrs for bitmaps so we have them around.
- actually.. once we have generated texture.. delete the reference to the bitmap
- support touch events
- support high-dpi
- toggle switch for touch and mouse (or both!)
- support joy sticks (joy sticks for moving cursor?)
- scripting commands: 
        - show keyboard,
        - write file atomically
        - preload_loop/sprite/sound?/ room?
- smooth scrolling
- go through mouse button events.. when should we clear the "button down" thingo? we don't always return after a single game loop.
*/


//
// Engine initialization
//

#include <stdexcept>
#include <errno.h>

#include <thread>
#include <condition_variable>
#include <atomic>

#include <list>

#include <iostream>

#include <nn/profiler.h>
#include <nn/os.h>
#include <nn/init.h>

#include "physfs.h"

#include "core/platform.h"
#include "main/mainheader.h"
#include "ac/common.h"
#include "ac/character.h"
#include "ac/draw.h"
#include "ac/game.h"
#include "ac/gamesetup.h"
#include "ac/gamesetupstruct.h"
#include "ac/global_character.h"
#include "ac/gui.h"
#include "ac/lipsync.h"
#include "ac/objectcache.h"
#include "ac/path_helper.h"
#include "ac/sys_events.h"
#include "ac/speech.h"
#include "ac/spritecache.h"
#include "ac/translation.h"
#include "ac/viewframe.h"
#include "ac/dynobj/scriptsystem.h"
#include "debug/debug_log.h"
#include "debug/debugger.h"
#include "font/fonts.h"
#include "main/config.h"
#include "main/game_start.h"
#include "main/engine.h"
#include "main/main.h"
#include "media/audio/audio_system.h"
#include "util/directory.h"
#include "main/game_run.h"
#include "ac/display.h"
#include "gui/guiinv.h"
#include "media/video/video.h"
#include "gfx/scene_graph_driver.h"
#include "gui/guitextbox.h"
#include "debug/debugmanager.h"
#include "util/path.h"
#include "util/asset_loader.h"
#include "ac/timer.h"
#include "ac/event.h"

#define AGS_DEBUG_FRAME_TIMER 0

namespace ags = AGS::Common;
namespace agsdbg = AGS::Common::Debug;
namespace agseng = AGS::Engine;

using namespace AGS::Common;
using namespace AGS::Engine;

namespace agsdbg = AGS::Common::Debug;

extern char check_dynamic_sprites_at_exit;
extern volatile char want_exit, abort_engine;

extern GameSetup usetup;
extern GameSetupStruct game;
extern SpriteCache spriteset;
extern ObjectCache objcache[MAX_ROOM_OBJECTS];
extern ScriptObject scrObj[MAX_ROOM_OBJECTS];
extern ViewStruct*views;
extern int displayed_room;
extern int eip_guinum;
extern int eip_guiobj;
extern SpeechLipSyncLine *splipsync;
extern int numLipLines, curLipLine, curLipLinePhoneme;
extern ScriptSystem scsystem;
extern IGraphicsDriver *gfxDriver;
extern Bitmap **actsps;
extern color palette[256];
extern CharacterExtras *charextra;
extern CharacterInfo*playerchar;
extern Bitmap **guibg;
extern IDriverDependantBitmap **guibgbmp;

extern bool framerate_maxed;

ResourcePaths ResPaths;


class GameContext 
{
public:

    enum ControllingThread { MainControlling, GameControlling, QuitControlling };

    GameContext() {}

    void Start()
    {
        controlling_ = MainControlling;
        gth_ = std::thread(&GameContext::WorkThread, &*this);
    }

    void Stop() 
    {
        GiveControlToQuitting();
        gth_.join();
    }

    ControllingThread GiveControlToGame()
    {
        std::unique_lock<std::mutex> lk(controlling_cv_m);

        if (controlling_ == QuitControlling) { return controlling_; }

        if (controlling_ != ControllingThread::GameControlling) {
            controlling_ = ControllingThread::GameControlling;
            controlling_cv_.notify_all();
        }

        controlling_cv_.wait(lk, [&] { return controlling_ == MainControlling || controlling_ == QuitControlling; });

        return controlling_;
    }

    ControllingThread GiveControlToMain()
    {
        std::unique_lock<std::mutex> lk(controlling_cv_m);

        if (controlling_ == QuitControlling) { return controlling_; }

        if (controlling_ != ControllingThread::MainControlling) {
            controlling_ = ControllingThread::MainControlling;
            controlling_cv_.notify_all();
        }

        controlling_cv_.wait(lk, [&] { return controlling_ == GameControlling || controlling_ == QuitControlling; });

        return controlling_;
    }

    ControllingThread GiveControlToQuitting()
    {
        std::unique_lock<std::mutex> lk(controlling_cv_m);
        controlling_ = ControllingThread::QuitControlling;
        controlling_cv_.notify_all();

        return controlling_;
    }


private:

    void WorkThread() 
    {
        // wait for main thread to be ready.
        GiveControlToMain();

        try
        {
            engine_init_game_settings();
            start_game();
            RunGameUntilAborted();
        }
        catch(const std::exception& e) {
            std::cout << "Caught exception \"" << e.what() << "\"\n";
        } catch (...) {
            std::cout << "uhoh";
        }

        GiveControlToQuitting();
        
        printf("done?\n");
    }

    std::thread gth_;

    ControllingThread controlling_ = ControllingThread::MainControlling;
    std::condition_variable controlling_cv_ {};
    std::mutex controlling_cv_m {};
};

static GameContext gameContext_;

// this is all that is exposed for the game.
void GameContextGiveControlToMain()
{
    auto controlling = gameContext_.GiveControlToMain();
    if (controlling == GameContext::QuitControlling) {
        throw std::runtime_error("quitting");
    }
}



static void load_speech_sync_data() {
    auto f = gameAssetLoader.LoadFileAsync("speech/syncdata.dat", true);
    Stream *speechsync = gameAssetLoader.ResolveAsStream(f);
    if (speechsync == nullptr) { return; }

    // this game has voice lip sync
    int lipsync_fmt = speechsync->ReadInt32();
    if (lipsync_fmt != 4) {return;}

    numLipLines = speechsync->ReadInt32();
    splipsync = (SpeechLipSyncLine*)malloc (sizeof(SpeechLipSyncLine) * numLipLines);
    for (int ee = 0; ee < numLipLines; ee++)
    {
        splipsync[ee].numPhonemes = speechsync->ReadInt16();
        speechsync->Read(splipsync[ee].filename, 14);
        splipsync[ee].endtimeoffs = (int*)malloc(splipsync[ee].numPhonemes * sizeof(int));
        speechsync->ReadArrayOfInt32(splipsync[ee].endtimeoffs, splipsync[ee].numPhonemes);
        splipsync[ee].frame = (short*)malloc(splipsync[ee].numPhonemes * sizeof(short));
        speechsync->ReadArrayOfInt16(splipsync[ee].frame, splipsync[ee].numPhonemes);
    }

    delete speechsync;
}

// Convert guis position and size to proper game resolution.
// Necessary for pre 3.1.0 games only to sync with modern engine.
static void convert_gui_to_game_resolution()
{
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
static void convert_objects_to_data_resolution()
{
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


// Setup engine after the graphics mode has changed
static void engine_post_gfxmode_setup(const Size &init_desktop)
{
    DisplayMode dm = gfxDriver->GetDisplayMode();
    // If color depth has changed (or graphics mode was inited for the
    // very first time), we also need to recreate bitmaps
    bool has_driver_changed = scsystem.coldepth != dm.ColorDepth;

    // Fill in scsystem struct with display mode parameters
    scsystem.coldepth = dm.ColorDepth;
    scsystem.windowed = dm.Windowed;
    scsystem.vsync = dm.Vsync;

    // Setup gfx driver callbacks and options
    //gfxDriver->SetCallbackForPolling(update_polled_stuff_if_runtime);
    gfxDriver->SetCallbackToDrawScreen(draw_screen_callback);
    gfxDriver->SetCallbackForNullSprite(GfxDriverNullSpriteCallback);

    if (has_driver_changed) {
        // Setup drawing modes and color conversions;
        // they depend primarily on gfx driver capabilities and new color depth 

        // Setup color conversion parameters
        set_color_conversion(COLORCONV_MOST | COLORCONV_EXPAND_256);

        init_draw_method();
    }

    // Setup mouse control mode and graphic area
    // Since we're always use desktop resolution, we don't need to adjust mouse acceleration
    Mouse::DisableControl();
    on_coordinates_scaling_changed();
    // If auto lock option is set, lock mouse to the game window
    if (usetup.mouse_auto_lock && scsystem.windowed != 0)
        Mouse::TryLockToWindow();
    
    video_on_gfxmode_changed();

}

bool text_box_is_active()
{
    for (int uu = 0; uu < game.numgui; uu++) {
        if (!guis[uu].IsDisplayed()) continue;
        for (int ww = 0; ww < guis[uu].GetControlCount(); ww++) {
            if (guis[uu].GetControlType(ww) != kGUITextBox) { continue; }
            auto guitex = dynamic_cast<GUITextBox*>(guis[uu].GetControl(ww));
            if (guitex == nullptr)      { continue;  }
            if (!guitex->IsEnabled())   { continue; }
            if (!guitex->IsVisible())   { continue; }

            return true;
        }
    }

    return false;
}




extern void onkeydown(SDL_Event *event);
extern void on_sdl_mouse_motion(SDL_MouseMotionEvent *event);
extern void on_sdl_mouse_button(SDL_MouseButtonEvent *event);
extern void on_sdl_mouse_wheel(SDL_MouseWheelEvent *event);
extern bool graphics_mode_update_render_frame();
extern void toggle_mouse_lock();
extern int sdl_button_to_allegro_bit(int button);
extern void set_mouse_b(int);

void send_keyboard_event_scancode(SDL_Scancode scancode)
{
    SDL_Event event = { SDL_KEYDOWN };
    event.key.keysym.scancode = scancode;
    event.key.keysym.mod = 0;
    onkeydown(&event);
}

void send_keyboard_event_ascii(char c)
{
    SDL_Event event = { SDL_TEXTINPUT };
    event.text.text[0] = c;
    event.text.text[1] = '\0';
    onkeydown(&event);
}

// allocate textures for bitmaps. allows you to clean up after a frame.
// pointers should be valid until CleanupForFrame.

#define AGS_DEBUG_TEXTURE_MANAGER 0

struct ManagedTexture {
    int64_t bitmapId;
    int64_t bitmapVersion;
    int width;
    int height;
    SDL_Texture *texture;
    bool inUse;
};

class TextureManager {
public:

    TextureManager(SDL_Renderer * renderer) : renderer_(renderer), textures_() {}

    void StartFrame() {
        // mark all as unused
#if AGS_DEBUG_TEXTURE_MANAGER
        agsdbg::Printf(kDbgMsg_Init, "---");
#endif
        for (auto &mt : textures_) {
            mt.inUse = false;
        }
    }

    SDL_Texture *TextureForBitmap(AGS::Common::Bitmap *bmp) {

        // do we have exact match?

        for (auto &mt : textures_) {
            if (mt.bitmapId != bmp->id_) { continue; }
            if (mt.bitmapVersion != bmp->version_) { continue; }
            if (mt.texture == nullptr) { continue; } // how did that get there?

            hits_++;
            mt.inUse = true;
#if AGS_DEBUG_TEXTURE_MANAGER
            agsdbg::Printf(kDbgMsg_Init, "hit: %d %d", (int)bmp->id_, (int)bmp->version_);
#endif
            return mt.texture;
        }

        misses_++;

        auto *s = SDL_CreateRGBSurfaceWithFormatFrom(
            (void*)bmp->GetData(),
            bmp->GetWidth(),
            bmp->GetHeight(),
            bmp->GetBPP(),
            bmp->GetLineLength(),
            SDL_PIXELFORMAT_ARGB8888);
        SDL_SetColorKey(s, SDL_TRUE, MASK_COLOR_32);
        auto *t = SDL_CreateTextureFromSurface(renderer_, s);
        SDL_FreeSurface(s);
        textures_.push_front({ bmp->id_, bmp->version_, bmp->GetWidth(), bmp->GetHeight(), t, true });

#if AGS_DEBUG_TEXTURE_MANAGER
        agsdbg::Printf(kDbgMsg_Init, "miss: %d %d", (int)bmp->id_, (int)bmp->version_);
#endif

        return t;
    }

    void Flush() {
#if AGS_DEBUG_TEXTURE_MANAGER
        agsdbg::Printf(kDbgMsg_Init, "hits: %2.1f%%  size: %d", 100.0f * hits_ / (hits_ + misses_), textures_.size());
#endif

        if (textures_.size() > (hits_ * 2)) {

#if AGS_DEBUG_TEXTURE_MANAGER
            agsdbg::Printf(kDbgMsg_Init, "flush");
#endif

            auto it = textures_.cbegin();
            while (it != textures_.end()) {
                if (!it->inUse) {
                    SDL_DestroyTexture(it->texture);
                    it = textures_.erase(it);
                }
                else {
                    it++;
                }
            }
        }

        hits_ = misses_ = 0;
    }

private:
    SDL_Renderer *renderer_;
    std::list<ManagedTexture> textures_;

    int hits_ = 0;
    int misses_ = 0;
};


int initialize_engine(const ConfigTree &startup_opts)
{
    const String exe_path = ".";


    // ScriptSystem::aci_version is only 10 chars long
    strncpy(scsystem.aci_version, EngineVersion.LongString.GetCStr(), 10);
    scsystem.os = platform->GetSystemOSID();

    play.randseed = time(nullptr);
    srand (play.randseed);

    set_uformat(U_ASCII);
    if (install_allegro(SYSTEM_AUTODETECT, &errno, atexit) != 0) { throw std::runtime_error("Unable to initialise Allegro."); }
    // NOTE: allegro should have called SDL_Init internally.
    platform->PostAllegroInit(false);



    agsdbg::Printf(ags::kDbgMsg_Init, "Controllers available:");
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            agsdbg::Printf(ags::kDbgMsg_Init, "  %d : C : %s", i, SDL_GameControllerNameForIndex(i));
        } else {
            agsdbg::Printf(ags::kDbgMsg_Init, "  %d : J : %s", i, SDL_JoystickNameForIndex(i));
        }
    }


    SDL_GameController *controller = nullptr;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (!SDL_IsGameController(i)) { continue; }
        controller = SDL_GameControllerOpen(i);
        if (controller) { break; }
    }

    if (controller) {
        agsdbg::Printf(ags::kDbgMsg_Init, "Got controller: %s", SDL_GameControllerName(controller));
    }


    init_font_renderer();

    usetup.mod_player = 1;

    config_defaults();
    usetup.data_files_dir = Directory::GetCurrentDirectory();
    usetup.main_data_filepath = Directory::GetCurrentDirectory();
    usetup.main_data_filename = "agsgame.dat";

    ConfigTree cfg;
#if 0
    String def_cfg_file = find_default_cfg_file(exe_path.GetCStr());
    IniUtil::Read(def_cfg_file, cfg);
    apply_config(cfg);
#endif
    post_config();

    psp_audio_multithreaded = 1; //consoles can always do this, i guess 


    ResPaths.DataDir = ".";
    ResPaths.GamePak.Name = "unavowed.ags"; // used to uniquely identify when switching paks
    ResPaths.GamePak.Path = "./unavowed.ags";
    ResPaths.SpeechPak.Name = "speech.vox";
    ResPaths.SpeechPak.Path = "./speech.vox";
    ResPaths.AudioPak.Name = "audio.vox";
    ResPaths.AudioPak.Path = "./audio.vox";

    extern String installDirectory;
    extern String installAudioDirectory;
    extern String installVoiceDirectory;

    installDirectory = ResPaths.DataDir;
    installAudioDirectory = ResPaths.DataDir;
    installVoiceDirectory = ResPaths.DataDir;


    PHYSFS_mount("rom:/game.zip", "/", 1);
    PHYSFS_mount("rom:/audio.zip", "/audio", 1);
    PHYSFS_mount("rom:/sprite.zip", "/sprite", 1);
    PHYSFS_mount("rom:/view.zip", "/view", 1);
    PHYSFS_mount("rom:/audio-extra.zip", "/audio", 1);
    PHYSFS_mount("rom:/audio-speech.zip", "/speech", 1);

    gameAssetLibrary = std::make_unique<AGS::Common::AssetManager>();

    gameAssetLoader.StartBackgroundThread();


#if 0
    if (AGS::Common::Path::IsDirectory(ResPaths.SpeechPak.Path.GetCStr())) {
        PHYSFS_mount(ResPaths.SpeechPak.Path.GetCStr(), "/speech", 1);
        play.want_speech = 1;
    }
    else {
        ResPaths.SpeechPak.Name = "";
        ResPaths.SpeechPak.Path = "";
        play.want_speech = -2;
    }
#endif
    play.want_speech = 1;

#if 0
    if (AGS::Common::Path::IsDirectory(ResPaths.AudioPak.Path.GetCStr())) {
        PHYSFS_mount(ResPaths.AudioPak.Path.GetCStr(), "/audio", 1);
        play.separate_music_lib = 1;
    }
    else {
        ResPaths.AudioPak.Name = "";
        ResPaths.AudioPak.Path = "";
        play.separate_music_lib = 0;
    }
#endif
    play.separate_music_lib = 1;


    if (play.want_speech >= 1) {
        load_speech_sync_data();  // uses open audio asset file
    }


    auto audioAssetLibrary = std::make_unique<AGS::Common::AssetManager>();


    audio_core_init(audioAssetLibrary);


    set_game_speed(40);

    // Pre-load game name and savegame folder names from data file
    // also sets loaded_game_file_version
    if (!preload_game_data()) { throw std::runtime_error("Error preloading game data."); }
    if (!load_game_file()) { throw std::runtime_error("Error loading game data."); }

    char newDirBuffer[MAX_PATH];
    sprintf(newDirBuffer, "%s/%s", UserSavedgamesRootToken.GetCStr(), game.saveGameFolderName);
    SetSaveGameDirectoryPath(newDirBuffer);


    // Initializing resolution settings
    // Sets up game viewport and object scaling parameters depending on game.
    // TODO: this is part of the game init, not engine init, move it later

    usetup.textheight = getfontheight_outlined(0) + 1;

    // Initialize default viewports and room camera
    Rect viewport = RectWH(game.GetGameRes());
    play.SetMainViewport(viewport);
    play.SetUIViewport(viewport);

    scsystem.width = game.GetGameRes().Width;
    scsystem.height = game.GetGameRes().Height;
    scsystem.viewport_width = game_to_data_coord(play.GetMainViewport().GetWidth());
    scsystem.viewport_height = game_to_data_coord(play.GetMainViewport().GetHeight());

    if (loaded_game_file_version <= kGameVersion_310) {
        convert_gui_to_game_resolution();
    }
    if (loaded_game_file_version >= kGameVersion_310) {
        convert_objects_to_data_resolution();
    }



    engine_shutdown_gfxmode();

    const Size init_desktop = get_desktop_size();
    if (!graphics_mode_init_any(game.GetGameRes(), usetup.Screen, ColorDepthOption(game.GetColorDepth()))) { throw std::runtime_error("Error initialising graphics."); }
    
    const Size ignored;
    engine_post_gfxmode_setup(ignored);
    
    SDL_ShowCursor(SDL_DISABLE);


    if (!spriteset.InitFile("acsprset.spr")) { throw std::runtime_error("Error loading sprite set."); }

    load_view("cursor");
    load_view("inventory");
    load_view("gui");


    auto sceneGraphDriver = dynamic_cast<SceneGraphDriver*>(gfxDriver);
    if (sceneGraphDriver == nullptr) { throw std::runtime_error("Internal error dynamic casting to SceneGraphDriver."); }

    // create sdl renderer
    
    const int DEFAULT_DISPLAY_INDEX = 0;


    auto dm = sceneGraphDriver->GetDisplayMode();

    Uint32 flags = SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE;
    if (!dm.Windowed) {
        //flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    auto window = SDL_CreateWindow(
        "AGS",                  // window title
        SDL_WINDOWPOS_CENTERED_DISPLAY(DEFAULT_DISPLAY_INDEX),           // initial x position
        SDL_WINDOWPOS_CENTERED_DISPLAY(DEFAULT_DISPLAY_INDEX),           // initial y position
        dm.Width,                               // width, in pixels
        dm.Height,                               // height, in pixels
        flags                 // flags 
    );

    //   if (SDL_GetWindowGammaRamp(window, default_gamma_red, default_gamma_green, default_gamma_blue) == 0) {
    // has_gamma = true;
    //   }

    Uint32 rendererFlags = SDL_RENDERER_ACCELERATED;
    rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
    //   if (mode.Vsync) {
    // rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
    //   }

    auto renderer = SDL_CreateRenderer(window, -1, rendererFlags);

    //   SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    // SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");  // make the scaled rendering look smoother.
    auto nativeSize = sceneGraphDriver->GetNativeSize();
    SDL_RenderSetLogicalSize(renderer, nativeSize.Width, nativeSize.Height);



    // tell allegro whats up
    set_color_depth(dm.ColorDepth);

    // TODO set palette

   _rgb_r_shift_15 = 10;
   _rgb_g_shift_15 = 5;
   _rgb_b_shift_15 = 0;

   _rgb_r_shift_16 = 11;
   _rgb_g_shift_16 = 5;
   _rgb_b_shift_16 = 0;

   _rgb_r_shift_24 = 16;
   _rgb_g_shift_24 = 8;
   _rgb_b_shift_24 = 0;

   _rgb_a_shift_32 = 24;
   _rgb_r_shift_32 = 16; 
   _rgb_g_shift_32 = 8; 
   _rgb_b_shift_32 = 0;



    gameContext_.Start();


    
    bool have_text_box_displayed = false;

    int mouse_button_state = 0;
    int mouse_button_state_for_game_loop = mouse_button_state;

    auto textureManager = TextureManager(renderer);

    int memcount = 0;

    for(;;) {

        nn::profiler::RecordHeartbeat(nn::profiler::Heartbeats_Main);
        
#ifdef AGS_REGULAR_MEMORY_INFO_DUMPS
        memcount++;
        if (memcount > 120) {
            auto allocator = nn::init::GetAllocator();
            auto free_sz = allocator->GetTotalFreeSize();
            AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init, "mem heap free: %ld", (long)free_sz );
            memcount = 0;
        }
#endif

        bool want_nx_keyboard = false;

        #if AGS_DEBUG_FRAME_TIMER
		auto t_game_1 = AGS_Clock::now();
        #endif
        
        // keep track of button presses within a frame, to account for mouse up/down in a single frame

        SDL_Event event;
        while (SDL_PollEvent(&event)) {

            switch (event.type) {

            case SDL_QUIT:
                want_exit = 1;
                abort_engine = 1;
                check_dynamic_sprites_at_exit = 0;
                break;

            case SDL_WINDOWEVENT: {

                switch (event.window.event) {
                    // case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    Size ignored;
                    gfxDriver->UpdateDeviceScreen(ignored);
                    graphics_mode_update_render_frame();
                    engine_post_gfxmode_setup(ignored);
                    break;
                }
                break;
            }

            case SDL_KEYDOWN:
            case SDL_TEXTINPUT: {
                bool processed = false;

                // Alt+X, abort (but only once game is loaded)
                if (displayed_room >= 0) {
                    int gott = agsKeyCodeFromEvent(event);
                    if (gott == play.abort_key) {
                        check_dynamic_sprites_at_exit = 0;
                        want_exit = 1;
                        processed = true;
                    }
                }

                if (event.type == SDL_KEYDOWN) {
                    switch (event.key.keysym.scancode) {
                    // alt + ctrl
                    case SDL_SCANCODE_LCTRL:
                    case SDL_SCANCODE_RCTRL:
                    case SDL_SCANCODE_LALT:
                    case SDL_SCANCODE_RALT:
                        {
                            SDL_Keymod mod_state = SDL_GetModState();
                            if ((mod_state & KMOD_CTRL) && (mod_state & KMOD_ALT)) {
                                toggle_mouse_lock();
                                processed = true;
                            }
                        }
                        break;

                    // alt + enter
                    case SDL_SCANCODE_RETURN:
                        if (event.key.keysym.mod & KMOD_ALT) {
                            engine_try_switch_windowed_gfxmode();
                            processed = true;
                        }
                        break;

                    default: 
                        // do nothing with the 239 other enum  values
                        break;
                    }
                }

                if (!processed) {
                    onkeydown(&event);
                }
                break;
            }
            case SDL_KEYUP:
                break;

            case SDL_MOUSEMOTION:
                on_sdl_mouse_motion(&event.motion);
                break;
            case SDL_MOUSEBUTTONDOWN: {
                //auto mbits = sdl_button_to_allegro_bit(event.button.button);
                //mouse_button_state |= mbits;
                //mouse_button_state_for_game_loop |= mbits;
                on_sdl_mouse_button(&event.button); // just updates position
                break;
            }
            case SDL_MOUSEBUTTONUP: {
                //mouse_button_state &= ~(sdl_button_to_allegro_bit(event.button.button));
                auto mbits = sdl_button_to_allegro_bit(event.button.button);
                mouse_button_state |= mbits;
                mouse_button_state_for_game_loop |= mbits;
                on_sdl_mouse_button(&event.button); // just updates position
                break;
            }
            case SDL_MOUSEWHEEL:
                on_sdl_mouse_wheel(&event.wheel);
                break;


            case SDL_CONTROLLERBUTTONDOWN: {
                switch(event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_A:
                    case SDL_CONTROLLER_BUTTON_B:
                    case SDL_CONTROLLER_BUTTON_X:
                    case SDL_CONTROLLER_BUTTON_Y:
                    send_keyboard_event_scancode(SDL_SCANCODE_RETURN);
                    break;

                    case SDL_CONTROLLER_BUTTON_START:
                    send_keyboard_event_scancode(SDL_SCANCODE_ESCAPE);
                    break;

                    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                    want_nx_keyboard = true;
                    break;
                }
                break;
            }
            }

        }

        if (want_exit) { break; }

        if (editor_debugging_initialized) {
            check_for_messages_from_editor();
        }

        // I guess this doesn't necessarily need to be in the main thread.
        // audio_core_update();



		// YIELD TO GAME THREAD
		// TODO look into this loop. It might be that we can't call yield_main multiple times without rendering. (yet!)
		// auto max_skips = 3;
		// while ( (play.fast_forward || framerate_maxed || !waitingForNextTick()) && max_skips--) {
		if (play.fast_forward || !waitingForNextTick()) {
            update_audio_system_on_game_loop();
            gfxDriver->ClearDrawLists();

            set_mouse_b(mouse_button_state_for_game_loop);

            auto controlling = gameContext_.GiveControlToGame();
            if (controlling != GameContext::MainControlling) { break; }

            // clear mouse button state
            // mouse_button_state_for_game_loop = mouse_button_state;
            mouse_button_state_for_game_loop = 0;
        }
		// YIELD TO GAME THREAD

#if AGS_DEBUG_FRAME_TIMER
		auto t_game_2 = AGS_Clock::now();
		printf("game=%-2lld ms", std::chrono::duration_cast<std::chrono::milliseconds>(t_game_2 - t_game_1).count());
#endif


#if AGS_DEBUG_FRAME_TIMER
		auto t_render_1 = AGS_Clock::now();
#endif


        // On Screen Keyboard
        // - TODO: script api to show/hide
        // - TODO: on screen button to display
        // Manage On Screen Keyboard if textbox has just appeared/disappeared
        auto is_text_box_displayed = text_box_is_active();

        if (have_text_box_displayed != is_text_box_displayed) {
            if (is_text_box_displayed) {
                // SDL_StartTextInput();
                want_nx_keyboard = true;
            } else {
                // SDL_StopTextInput();
            }
        }
        have_text_box_displayed = is_text_box_displayed;

        if (want_nx_keyboard) {
            auto *keyboardbuf = SDL_NintendoSwitch_RunSoftwareKeyboard(SDL_NINTENDOSWITCH_KEYBOARD_SINGLELINE, nullptr, 120);
            for (auto c = keyboardbuf; *c; c++) {
                send_keyboard_event_ascii(*c);
            }
            SDL_free(keyboardbuf);
            send_keyboard_event_scancode(SDL_SCANCODE_RETURN);
        }


        if (!play.fast_forward) {

            textureManager.StartFrame();

            // clear screen
            SDL_SetRenderDrawColor( renderer, 0, 0, 0, 255 );
            SDL_RenderClear( renderer );

            for (auto const &operation : sceneGraphDriver->GetOperations()){
                
                if (operation.bitmap_ == nullptr) { 
                    if (operation.callback_ != nullptr)
                        operation.callback_(operation.x_, operation.y_);
                    continue; 
                }

                auto *bmp = operation.bitmap_->bitmap_;
                if (bmp == nullptr) { continue; }

                auto *t = textureManager.TextureForBitmap(bmp);
                if (t == nullptr) { continue; }

                auto viewport = SDL_Rect {
                    /*.x=*/operation.spriteTransformViewPortX_, 
                    /*.y=*/operation.spriteTransformViewPortY_, 
                    /*.w=*/operation.spriteTransformViewPortWidth_, 
                    /*.h=*/operation.spriteTransformViewPortHeight_
                };
                SDL_RenderSetViewport(renderer, &viewport);

                auto x = operation.x_;
                auto y = operation.y_;
                auto w = bmp->GetWidth();
                auto h = bmp->GetHeight();
                
                auto rotateDegrees = 0.0f;
                
                if (operation.spriteTransformIsSet_) {
                    x += operation.spriteTransformTranslateX_;
                    y += operation.spriteTransformTranslateY_;
                    
                    w *= operation.spriteTransformScaleX_;
                    h *= operation.spriteTransformScaleY_;
                    
                    rotateDegrees = operation.spriteTransformRotateRadians_ * 180.0 / M_PI;
                }

                if (operation.bitmap_->lightLevel_ == 0) {
                    SDL_SetTextureColorMod(t, 255, 255, 255);
                } else {
                    SDL_SetTextureColorMod(t, operation.bitmap_->lightLevel_, operation.bitmap_->lightLevel_, operation.bitmap_->lightLevel_);
                }
                
                auto dst = SDL_Rect {x, y, w, h};
                SDL_RenderCopyEx(renderer, t, nullptr, &dst, rotateDegrees, nullptr, operation.bitmap_->isFlipped_ ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
            }
            
            SDL_RenderPresent(renderer);

            textureManager.Flush();
        }

#if AGS_DEBUG_FRAME_TIMER
		auto t_render_2 = AGS_Clock::now();
		printf(" | render=%-2lld ms", std::chrono::duration_cast<std::chrono::milliseconds>(t_render_2 - t_render_1).count());

		printf(" | total=%-4lld ms", std::chrono::duration_cast<std::chrono::milliseconds>(t_render_2 - t_game_1).count());
		printf(" | budget=16.7 ms");
		printf("\n");
#endif

        // TODO: interpolate camera view!
    }
    
    quit("|bye!");

    gameContext_.Stop();

    return 0;
}


// externally called

// Applies necessary changes after screen<->virtual coordinate transformation has changed
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


static Uint32 lastFullscreenToggle = 0;

bool engine_try_switch_windowed_gfxmode()
{
    if (!gfxDriver) { return false; }
    if (!gfxDriver->IsModeSet()) { return false; }

    auto timeSinceLastToggle = SDL_GetTicks() - lastFullscreenToggle;
    if (timeSinceLastToggle < 250) { return false; }
    lastFullscreenToggle = SDL_GetTicks();
    
    graphics_mode_toggle_full_screen();
    
    const Size ignored;
    engine_post_gfxmode_setup(ignored);

    ags_clear_input_buffer();

    return true;
}

void engine_shutdown_gfxmode()
{
    if (!gfxDriver){ return;}

    // Prepare engine to the graphics mode shutdown and gfx driver destruction

    // Reset mouse controls before changing gfx mode
    // Always disable mouse control and unlock mouse when releasing down gfx mode
    Mouse::DisableControl();
    Mouse::UnlockFromWindow();

    // Reset gfx driver callbacks
    gfxDriver->SetCallbackForPolling(nullptr);
    gfxDriver->SetCallbackToDrawScreen(nullptr);
    gfxDriver->SetCallbackForNullSprite(nullptr);
    gfxDriver->SetMemoryBackBuffer(nullptr);
    
    // Cleanup auxiliary drawing objects
    dispose_draw_method();

    delete sub_vscreen;
    sub_vscreen = nullptr;


    graphics_mode_shutdown();
}

const char *get_engine_version() {
    return EngineVersion.LongString.GetCStr();
}



// on init and running an ags game.
void engine_init_game_settings()
{
    Debug::Printf("Initialize game settings");

    int ee;

    for (ee = 0; ee < MAX_ROOM_OBJECTS + game.numcharacters; ee++)
        actsps[ee] = nullptr;

    for (ee=0;ee<256;ee++) {
        if (game.paluses[ee]!=PAL_BACKGROUND)
            palette[ee]=game.defpal[ee];
    }

    for (ee = 0; ee < game.numcursors; ee++) 
    {
        // The cursor graphics are assigned to mousecurs[] and so cannot
        // be removed from memory
        if (game.mcurs[ee].pic >= 0)
            spriteset.Precache(game.mcurs[ee].pic);

        // just in case they typed an invalid view number in the editor
        if (game.mcurs[ee].view >= game.numviews)
            game.mcurs[ee].view = -1;

        if (game.mcurs[ee].view >= 0)
            precache_view (game.mcurs[ee].view);
    }

    for (ee = 0; ee < MAX_ROOM_OBJECTS; ee++)
        objcache[ee].image = nullptr;

    /*  dummygui.guiId = -1;
    dummyguicontrol.guin = -1;
    dummyguicontrol.objn = -1;*/

    //  game.chars[0].talkview=4;
    //init_language_text(game.langcodes[0]);

    for (ee = 0; ee < MAX_ROOM_OBJECTS; ee++) {
        scrObj[ee].id = ee;
        // 64 bit: Using the id instead
        // scrObj[ee].obj = NULL;
    }

    for (ee=0;ee<game.numcharacters;ee++) {
        memset(&game.chars[ee].inv[0],0,MAX_INV*sizeof(short));
        game.chars[ee].activeinv=-1;
        game.chars[ee].following=-1;
        game.chars[ee].followinfo=97 | (10 << 8);
        game.chars[ee].idletime=20;  // can be overridden later with SetIdle or summink
        game.chars[ee].idleleft=game.chars[ee].idletime;
        game.chars[ee].transparency = 0;
        game.chars[ee].baseline = -1;
        game.chars[ee].walkwaitcounter = 0;
        game.chars[ee].z = 0;
        charextra[ee].xwas = INVALID_X;
        charextra[ee].zoom = 100;
        if (game.chars[ee].view >= 0) {
            // set initial loop to 0
            game.chars[ee].loop = 0;
            // or to 1 if they don't have up/down frames
            if (views[game.chars[ee].view].loops[0].numFrames < 1)
                game.chars[ee].loop = 1;
        }
        charextra[ee].process_idle_this_time = 0;
        charextra[ee].invorder_count = 0;
        charextra[ee].slow_move_counter = 0;
        charextra[ee].animwait = 0;
    }
    // multiply up gui positions
    guibg = (Bitmap **)malloc(sizeof(Bitmap *) * game.numgui);
    guibgbmp = (IDriverDependantBitmap**)malloc(sizeof(IDriverDependantBitmap*) * game.numgui);
    for (ee=0;ee<game.numgui;ee++) {
        guibg[ee] = nullptr;
        guibgbmp[ee] = nullptr;
    }

    for (ee=0;ee<game.numinvitems;ee++) {
        if (game.invinfo[ee].flags & IFLG_STARTWITH) playerchar->inv[ee]=1;
        else playerchar->inv[ee]=0;
    }
    play.score=0;
    play.sierra_inv_color=7;
    // copy the value set by the editor
    if (game.options[OPT_GLOBALTALKANIMSPD] >= 0)
    {
        play.talkanim_speed = game.options[OPT_GLOBALTALKANIMSPD];
        game.options[OPT_GLOBALTALKANIMSPD] = 1;
    }
    else
    {
        play.talkanim_speed = -game.options[OPT_GLOBALTALKANIMSPD] - 1;
        game.options[OPT_GLOBALTALKANIMSPD] = 0;
    }
    play.inv_item_wid = 40;
    play.inv_item_hit = 22;
    play.messagetime=-1;
    play.disabled_user_interface=0;
    play.gscript_timer=-1;
    play.debug_mode=game.options[OPT_DEBUGMODE];
    play.inv_top=0;
    play.inv_numdisp=0;
    play.obsolete_inv_numorder=0;
    play.text_speed=15;
    play.text_min_display_time_ms = 1000;
    play.ignore_user_input_after_text_timeout_ms = 500;
    play.ignore_user_input_until_time = AGS_Clock::now();
    play.lipsync_speed = 15;
    play.close_mouth_speech_time = 10;
    play.disable_antialiasing = 0;
    play.rtint_enabled = false;
    play.rtint_level = 0;
    play.rtint_light = 0;
    play.text_speed_modifier = 0;
    play.text_align = kHAlignLeft;
    // Make the default alignment to the right with right-to-left text
    if (game.options[OPT_RIGHTLEFTWRITE])
        play.text_align = kHAlignRight;

    play.speech_bubble_width = get_fixed_pixel_size(100);
    play.bg_frame=0;
    play.bg_frame_locked=0;
    play.bg_anim_delay=0;
    play.anim_background_speed = 0;
    play.silent_midi = 0;
    play.current_music_repeating = 0;
    play.skip_until_char_stops = -1;
    play.get_loc_name_last_time = -1;
    play.get_loc_name_save_cursor = -1;
    play.restore_cursor_mode_to = -1;
    play.restore_cursor_image_to = -1;
    play.ground_level_areas_disabled = 0;
    play.next_screen_transition = -1;
    play.temporarily_turned_off_character = -1;
    play.inv_backwards_compatibility = 0;
    play.gamma_adjustment = 100;
    play.do_once_tokens.resize(0);
    play.music_queue_size = 0;
    play.shakesc_length = 0;
    play.wait_counter=0;
    play.key_skip_wait = 0;
    play.cur_music_number=-1;
    play.music_repeat=1;
    play.music_master_volume=100 + LegacyMusicMasterVolumeAdjustment;
    play.digital_master_volume = 100;
    play.screen_flipped=0;
    play.cant_skip_speech = user_to_internal_skip_speech((SkipSpeechStyle)game.options[OPT_NOSKIPTEXT]);
    play.sound_volume = 255;
    play.speech_volume = 255;
    play.normal_font = 0;
    play.speech_font = 1;
    play.speech_text_shadow = 16;
    play.screen_tint = -1;
    play.bad_parsed_word[0] = 0;
    play.swap_portrait_side = 0;
    play.swap_portrait_lastchar = -1;
    play.swap_portrait_lastlastchar = -1;
    play.in_conversation = 0;
    play.skip_display = 3;
    play.no_multiloop_repeat = 0;
    play.in_cutscene = 0;
    play.fast_forward = 0;
    play.totalscore = game.totalscore;
    play.roomscript_finished = 0;
    play.no_textbg_when_voice = 0;
    play.max_dialogoption_width = get_fixed_pixel_size(180);
    play.no_hicolor_fadein = 0;
    play.bgspeech_game_speed = 0;
    play.bgspeech_stay_on_display = 0;
    play.unfactor_speech_from_textlength = 0;
    play.mp3_loop_before_end = 70;
    play.speech_music_drop = 60;
    play.room_changes = 0;
    play.check_interaction_only = 0;
    play.replay_hotkey_unused = -1;  // StartRecording: not supported.
    play.dialog_options_x = 0;
    play.dialog_options_y = 0;
    play.min_dialogoption_width = 0;
    play.disable_dialog_parser = 0;
    play.ambient_sounds_persist = 0;
    play.screen_is_faded_out = 0;
    play.player_on_region = 0;
    play.top_bar_backcolor = 8;
    play.top_bar_textcolor = 16;
    play.top_bar_bordercolor = 8;
    play.top_bar_borderwidth = 1;
    play.top_bar_ypos = 25;
    play.top_bar_font = -1;
    play.screenshot_width = 160;
    play.screenshot_height = 100;
    play.speech_text_align = kHAlignCenter;
    play.auto_use_walkto_points = 1;
    play.inventory_greys_out = 0;
    play.skip_speech_specific_key = 0;
    play.abort_key = 324;  // Alt+X
    play.fade_to_red = 0;
    play.fade_to_green = 0;
    play.fade_to_blue = 0;
    play.show_single_dialog_option = 0;
    play.keep_screen_during_instant_transition = 0;
    play.read_dialog_option_colour = -1;
    play.speech_portrait_placement = 0;
    play.speech_portrait_x = 0;
    play.speech_portrait_y = 0;
    play.speech_display_post_time_ms = 0;
    play.dialog_options_highlight_color = DIALOG_OPTIONS_HIGHLIGHT_COLOR_DEFAULT;
    play.speech_has_voice = false;
    play.speech_voice_blocking = false;
    play.speech_in_post_state = false;
    play.narrator_speech = game.playercharacter;
    play.crossfading_out_channel = 0;
    play.speech_textwindow_gui = game.options[OPT_TWCUSTOM];
    if (play.speech_textwindow_gui == 0)
        play.speech_textwindow_gui = -1;
    strcpy(play.game_name, game.gamename);
    play.lastParserEntry[0] = 0;
    play.follow_change_room_timer = 150;
    for (ee = 0; ee < MAX_ROOM_BGFRAMES; ee++) 
        play.raw_modified[ee] = 0;
    play.game_speed_modifier = 0;
    if (debug_flags & DBG_DEBUGMODE)
        play.debug_mode = 1;
    gui_disabled_style = convert_gui_disabled_style(game.options[OPT_DISABLEOFF]);

    memset(&play.walkable_areas_on[0],1,MAX_WALK_AREAS+1);
    memset(&play.script_timers[0],0,MAX_TIMERS * sizeof(int));
    memset(&play.default_audio_type_volumes[0], -1, MAX_AUDIO_TYPES * sizeof(int));

    // reset graphical script vars (they're still used by some games)
    for (ee = 0; ee < MAXGLOBALVARS; ee++) 
        play.globalvars[ee] = 0;

    for (ee = 0; ee < MAXGLOBALSTRINGS; ee++)
        play.globalstrings[ee][0] = 0;

    if (!usetup.translation.IsEmpty())
        init_translation (usetup.translation, "", true);

    update_invorder();
    displayed_room = -10;

    currentcursor=0;
    mousey=100;  // stop icon bar popping up

    // We use same variable to read config and be used at runtime for now,
    // so update it here with regards to game design option
    usetup.RenderAtScreenRes = 
        (game.options[OPT_RENDERATSCREENRES] == kRenderAtScreenRes_UserDefined && usetup.RenderAtScreenRes) ||
         game.options[OPT_RENDERATSCREENRES] == kRenderAtScreenRes_Enabled;
}
