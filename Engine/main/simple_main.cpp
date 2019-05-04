/* simple main

xperment to see wats necssary 

*/

#include "core/platform.h"
#define AGS_PLATFORM_DEFINES_PSP_VARS (AGS_PLATFORM_OS_IOS || AGS_PLATFORM_OS_ANDROID)


#include "ac/common.h"
#include "ac/gamesetup.h"
#include "ac/gamestate.h"
#include "core/def_version.h"
#include "debug/agseditordebugger.h"
#include "debug/debug_log.h"
#include "debug/out.h"
#include "main/config.h"
#include "main/engine.h"
#include "main/mainheader.h"
#include "main/main.h"
#include "platform/base/agsplatformdriver.h"
#include "ac/route_finder.h"
#include "core/assetmanager.h"
#include "util/directory.h"
#include "util/path.h"

#include <unistd.h>

char **global_argv = nullptr;
int    global_argc = 0;

String appDirectory; // Needed for library/plugin loading

#if ! AGS_PLATFORM_DEFINES_PSP_VARS
int psp_video_framedrop = 1;
int psp_audio_enabled = 1;
int psp_midi_enabled = 1;
int psp_ignore_acsetup_cfg_file = 0;
int psp_clear_cache_on_room_change = 0;

int psp_midi_preload_patches = 0;
int psp_audio_cachesize = 10;
char psp_game_file_name[] = "";
char psp_translation[] = "default";

int psp_gfx_renderer = 0;
int psp_gfx_scaling = 1;
int psp_gfx_smoothing = 0;
int psp_gfx_super_sampling = 1;
int psp_gfx_smooth_sprites = 0;
#endif


// this needs to be updated if the "play" struct changes
#define SVG_VERSION_BWCOMPAT_MAJOR      3
#define SVG_VERSION_BWCOMPAT_MINOR      2
#define SVG_VERSION_BWCOMPAT_RELEASE    0
#define SVG_VERSION_BWCOMPAT_REVISION   1103
// CHECKME: we may lower this down, if we find that earlier versions may still
// load new savedgames
#define SVG_VERSION_FWCOMPAT_MAJOR      3
#define SVG_VERSION_FWCOMPAT_MINOR      2
#define SVG_VERSION_FWCOMPAT_RELEASE    1
#define SVG_VERSION_FWCOMPAT_REVISION   1111


// Current engine version
AGS::Common::Version EngineVersion;
// Lowest savedgame version, accepted by this engine
AGS::Common::Version SavedgameLowestBackwardCompatVersion;
// Lowest engine version, which would accept current savedgames
AGS::Common::Version SavedgameLowestForwardCompatVersion;

extern int display_fps;

int ags_entry_point(int argc, char *argv[])
{ 
    // display_fps = 2;
    
    global_argv = argv;
    global_argc = argc;

    EngineVersion = AGS::Common::Version(ACI_VERSION_STR " " SPECIAL_VERSION);
#if defined (BUILD_STR)
    EngineVersion.BuildInfo = BUILD_STR;
#endif

    SavedgameLowestBackwardCompatVersion = AGS::Common::Version(SVG_VERSION_BWCOMPAT_MAJOR, SVG_VERSION_BWCOMPAT_MINOR, SVG_VERSION_BWCOMPAT_RELEASE, SVG_VERSION_BWCOMPAT_REVISION);
    SavedgameLowestForwardCompatVersion = AGS::Common::Version(SVG_VERSION_FWCOMPAT_MAJOR, SVG_VERSION_FWCOMPAT_MINOR, SVG_VERSION_FWCOMPAT_RELEASE, SVG_VERSION_FWCOMPAT_REVISION);

    platform = AGSPlatformDriver::GetDriver();

    init_debug();

    Common::AssetManager::CreateInstance();
    Common::AssetManager::SetSearchPriority(Common::kAssetPriorityDir);

    ConfigTree startup_opts;

    // We need to know where to load plugins, should be bundled with game but maybe engine is separate
    appDirectory = AGS::Common::Path::GetDirectoryPath(AGS::Common::Path::MakeAbsolutePath("."));

    int exitcode = initialize_engine(startup_opts);

    allegro_exit();
    platform->PostAllegroExit();
    
    return exitcode;
}




const char *get_allegro_error()
{
    return allegro_error;
}

const char *set_allegro_error(const char *format, ...)
{
    va_list argptr;
    va_start(argptr, format);
    uvszprintf(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text(format), argptr);
    va_end(argptr);
    return allegro_error;
}