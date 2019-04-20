/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#ifdef SDL_FILESYSTEM_NINTENDO_SWITCH

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* System dependent filesystem routines                                */

#include <new>
#include <nn/fs.h>
#include <nn/account.h>

#include "SDL_error.h"
#include "SDL_stdinc.h"
#include "SDL_filesystem.h"

static char *SDL_NintendoSwitch_ROMCache = NULL;
static nn::account::UserHandle *SDL_NintendoSwitch_UserHandle = NULL;

char *
SDL_GetBasePath(void)
{
    if (!SDL_NintendoSwitch_ROMCache) {
        size_t cachelen = 0;
        nn::fs::QueryMountRomCacheSize(&cachelen);
        if (cachelen == 0) {
            SDL_SetError("No ROM filesystem to mount?");
            return NULL;
        }

        char *cachebuf = (char *) SDL_malloc(cachelen);
        if (cachebuf == NULL) {
            SDL_SetError("Failed to allocate %llu bytes for ROM filesystem cache!", (unsigned long long) cachelen);
            return NULL;
        }

        const nn::Result rc = nn::fs::MountRom("rom", cachebuf, cachelen);
        if (rc.IsFailure()) {
            SDL_free(cachebuf);
            SDL_SetError("Failed to mount ROM filesystem: err=%d", rc.GetDescription());
            return NULL;
        }

        SDL_NintendoSwitch_ROMCache = cachebuf;
    }

    char *retval = SDL_strdup("rom:/");
    if (!retval) {
        SDL_OutOfMemory();
    }
    return retval;
}

char *
SDL_GetPrefPath(const char *org, const char *app)
{
    if (!SDL_NintendoSwitch_UserHandle) {
        nn::account::Initialize();
        nn::account::UserHandle *handle = new nn::account::UserHandle;
        nn::Result rc;

        rc = nn::account::OpenPreselectedUser(handle);
        if (rc.IsFailure()) {
            SDL_SetError("Couldn't open preselected user account, err=%d", rc.GetDescription());
            delete handle;
            return NULL;
        }

        nn::account::Uid uid;
        rc = nn::account::GetUserId(&uid, *handle);
        if (rc.IsFailure()) {
            SDL_SetError("Couldn't get uid from opened user handle, err=%d", rc.GetDescription());
            delete handle;
            return NULL;
        }

        rc = nn::fs::EnsureSaveData(uid);
        if (rc.IsFailure()) {
            SDL_SetError("Couldn't ensure save data, err=%d", rc.GetDescription());
            delete handle;
            return NULL;
        }

        rc = nn::fs::MountSaveData("save", uid);
        if (rc.IsFailure()) {
            SDL_SetError("Couldn't mount save data, err=%d", rc.GetDescription());
            delete handle;
            return NULL;
        }

        SDL_NintendoSwitch_UserHandle = handle;
    }
    
    char *retval = SDL_strdup("save:/");
    if (!retval) {
        SDL_OutOfMemory();
    }
    return retval;
}

int
SDL_NintendoSwitch_CommitSaveData(const char *mountpoint)
{
    const nn::Result rc = nn::fs::CommitSaveData(mountpoint);
    if (rc.IsFailure()) {
        return SDL_SetError("Failed to commit save data, err=%d", rc.GetDescription());
    }
    return 0;
}

extern "C" {
    void SDL_NintendoSwitch_QuitFilesystem(void);
}

void SDL_NintendoSwitch_QuitFilesystem(void)
{
    if (SDL_NintendoSwitch_ROMCache != NULL) {
        nn::fs::Unmount("rom");
        SDL_free(SDL_NintendoSwitch_ROMCache);
        SDL_NintendoSwitch_ROMCache = NULL;
    }

    if (SDL_NintendoSwitch_UserHandle != NULL) {
        nn::fs::Unmount("save");
        nn::account::CloseUser(*SDL_NintendoSwitch_UserHandle);
        delete SDL_NintendoSwitch_UserHandle;
        SDL_NintendoSwitch_UserHandle = NULL;
    }
}

#endif /* SDL_FILESYSTEM_NINTENDOSWITCH */

/* vi: set ts=4 sw=4 expandtab: */
