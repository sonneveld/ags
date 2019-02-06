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

#if AGS_PLATFORM_OS_LINUX

// ********* LINUX PLACEHOLDER DRIVER *********

#include <stdio.h>
#include <unistd.h>
#include <allegro.h>
#include "ac/runtime_defines.h"
#include "gfx/gfxdefines.h"
#include "platform/base/agsplatformdriver.h"
#include "plugin/agsplugin.h"
#include "media/audio/audio.h"
#include "util/string.h"
#include <libcda.h>

#include <pwd.h>
#include <sys/stat.h>

#include "binreloc.h"
#include "main/config.h"

using AGS::Common::String;


// Replace the default Allegro icon. The original defintion is in the
// Allegro 4.4 source under "src/x/xwin.c".
extern "C" {
#include "icon.xpm"
}
void* allegro_icon = icon_xpm;

// PSP variables
int psp_video_framedrop = 1;
int psp_audio_enabled = 1;
int psp_midi_enabled = 1;
int psp_ignore_acsetup_cfg_file = 0;
int psp_clear_cache_on_room_change = 0;

int psp_midi_preload_patches = 0;
int psp_audio_cachesize = 10;
char psp_game_file_name[256];

int psp_gfx_renderer = 0;
int psp_gfx_scaling = 1;
int psp_gfx_smoothing = 0;
int psp_gfx_super_sampling = 1;
int psp_gfx_smooth_sprites = 1;
char psp_translation[100];

String LinuxOutputDirectory;

struct AGSLinux : AGSPlatformDriver {
  AGSLinux();

  int  CDPlayerCommand(int cmdd, int datt) override;
  void DisplayAlert(const char*, ...) override;
  const char *GetUserSavedgamesDirectory() override;
  const char *GetUserConfigDirectory() override;
  const char *GetUserGlobalConfigDirectory() override;
  const char *GetAppOutputDirectory() override;
  unsigned long GetDiskFreeSpaceMB() override;
  const char* GetNoMouseErrorString() override;
  bool IsMouseControlSupported(bool windowed) override;
  const char* GetAllegroFailUserHint() override;
  eScriptSystemOSID GetSystemOSID() override;
  int  InitializeCDPlayer() override;
  void PostAllegroExit() override;
  void SetGameWindowIcon() override;
  void ShutdownCDPlayer() override;
};

AGSLinux::AGSLinux() {
  // TODO: why is psp_game_file_name needed for linux builds?  
  // Setting it prevents proper discovery of AGS games.
  // Might be a left over from preparing AGS for Steam on Linux
  strcpy(psp_game_file_name, "agsgame.dat");
  strcpy(psp_translation, "default");

  BrInitError e = BR_INIT_ERROR_DISABLED;
  if (br_init(&e)) {
    char *exedir = br_find_exe_dir(NULL);
    if (exedir) {
      chdir(exedir);
      free(exedir);
    }
  }

  // force private modules
  setenv("ALLEGRO_MODULES", ".", 1);
}

int AGSLinux::CDPlayerCommand(int cmdd, int datt) {
  return cd_player_control(cmdd, datt);
}

void AGSLinux::DisplayAlert(const char *text, ...) {
  char displbuf[2000];
  va_list ap;
  va_start(ap, text);
  vsprintf(displbuf, text, ap);
  va_end(ap);
  printf("%s\n", displbuf);
}

size_t BuildXDGPath(char *destPath, size_t destSize)
{
  // Check to see if XDG_DATA_HOME is set in the enviroment
  const char* home_dir = getenv("XDG_DATA_HOME");
  size_t l = 0;
  if (home_dir)
  {
    l = snprintf(destPath, destSize, "%s", home_dir);
  }
  else
  {
    // No evironment variable, so we fall back to home dir in /etc/passwd
    struct passwd *p = getpwuid(getuid());
    l = snprintf(destPath, destSize, "%s/.local", p->pw_dir);
    if (mkdir(destPath, 0755) != 0 && errno != EEXIST)
      return 0;
    l += snprintf(destPath + l, destSize - l, "/share");
    if (mkdir(destPath, 0755) != 0 && errno != EEXIST)
      return 0;
  }
  l += snprintf(destPath + l, destSize - l, "/ags");
  if (mkdir(destPath, 0755) != 0 && errno != EEXIST)
    return 0;
  return l;
}

void DetermineAppOutputDirectory()
{
  if (!LinuxOutputDirectory.IsEmpty())
  {
    return;
  }

  char xdg_path[256];
  if (BuildXDGPath(xdg_path, sizeof(xdg_path)) > 0)
    LinuxOutputDirectory = xdg_path;
  else
    LinuxOutputDirectory = "/tmp";
}

const char *AGSLinux::GetUserSavedgamesDirectory()
{
  DetermineAppOutputDirectory();
  return LinuxOutputDirectory.GetCStr();
}

const char *AGSLinux::GetUserConfigDirectory()
{
  return GetUserSavedgamesDirectory();
}

const char *AGSLinux::GetUserGlobalConfigDirectory()
{
  return GetUserSavedgamesDirectory();
}

const char *AGSLinux::GetAppOutputDirectory()
{
  DetermineAppOutputDirectory();
  return LinuxOutputDirectory.GetCStr();
}

unsigned long AGSLinux::GetDiskFreeSpaceMB() {
  // placeholder
  return 100;
}

const char* AGSLinux::GetNoMouseErrorString() {
  return "This game requires a mouse. You need to configure and setup your mouse to play this game.\n";
}

bool AGSLinux::IsMouseControlSupported(bool windowed)
{
  return true; // supported for both fullscreen and windowed modes
}

const char* AGSLinux::GetAllegroFailUserHint()
{
  return "Make sure you have latest version of Allegro 4 libraries installed, and X server is running.";
}

eScriptSystemOSID AGSLinux::GetSystemOSID() {
  // override performed if `override.os` is set in config.
  return eOS_Linux;
}

int AGSLinux::InitializeCDPlayer() {
  return cd_player_init();
}

void AGSLinux::PostAllegroExit() {
  // do nothing
}

void AGSLinux::SetGameWindowIcon() {
  // do nothing
}

void AGSLinux::ShutdownCDPlayer() {
  cd_exit();
}

AGSPlatformDriver* AGSPlatformDriver::GetDriver() {
  if (instance == nullptr)
    instance = new AGSLinux();
  return instance;
}

#if 0
void AGSLinux::ReplaceSpecialPaths(const char *sourcePath, char *destPath) {
  // MYDOCS is what is used in acplwin.cpp
  if(strncasecmp(sourcePath, "$MYDOCS$", 8) == 0) {
    struct passwd *p = getpwuid(getuid());
    strcpy(destPath, p->pw_dir);
    strcat(destPath, "/.local");
    mkdir(destPath, 0755);
    strcat(destPath, "/share");
    mkdir(destPath, 0755);
    strcat(destPath, &sourcePath[8]);
    mkdir(destPath, 0755);
  // SAVEGAMEDIR is what is actually used in ac.cpp
  } else if(strncasecmp(sourcePath, "$SAVEGAMEDIR$", 13) == 0) {
    struct passwd *p = getpwuid(getuid());
    strcpy(destPath, p->pw_dir);
    strcat(destPath, "/.local");
    mkdir(destPath, 0755);
    strcat(destPath, "/share");
    mkdir(destPath, 0755);
    strcat(destPath, &sourcePath[8]);
    mkdir(destPath, 0755);
  } else if(strncasecmp(sourcePath, "$APPDATADIR$", 12) == 0) {
    struct passwd *p = getpwuid(getuid());
    strcpy(destPath, p->pw_dir);
    strcat(destPath, "/.local");
    mkdir(destPath, 0755);
    strcat(destPath, "/share");
    mkdir(destPath, 0755);
    strcat(destPath, &sourcePath[12]);
    mkdir(destPath, 0755);
  } else {
    strcpy(destPath, sourcePath);
  }
}

#endif

#endif
