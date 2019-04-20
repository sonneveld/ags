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

#ifndef SDL_nintendoswitchopengles_h_
#define SDL_nintendoswitchopengles_h_

#if SDL_VIDEO_DRIVER_NINTENDO_SWITCH && SDL_VIDEO_OPENGL_EGL

#include "../SDL_sysvideo.h"
#include "../SDL_egl_c.h"

/* OpenGLES functions */
#define NINTENDOSWITCH_GLES_GetAttribute SDL_EGL_GetAttribute
#define NINTENDOSWITCH_GLES_GetProcAddress SDL_EGL_GetProcAddress
#define NINTENDOSWITCH_GLES_UnloadLibrary SDL_EGL_UnloadLibrary
#define NINTENDOSWITCH_GLES_SetSwapInterval SDL_EGL_SetSwapInterval
#define NINTENDOSWITCH_GLES_GetSwapInterval SDL_EGL_GetSwapInterval
#define NINTENDOSWITCH_GLES_DeleteContext SDL_EGL_DeleteContext

extern int NINTENDOSWITCH_GLES_LoadLibrary(_THIS, const char *path);
extern SDL_GLContext NINTENDOSWITCH_GLES_CreateContext(_THIS, SDL_Window * window);
extern int NINTENDOSWITCH_GLES_SwapWindow(_THIS, SDL_Window * window);
extern int NINTENDOSWITCH_GLES_MakeCurrent(_THIS, SDL_Window * window, SDL_GLContext context);
extern void NINTENDOSWITCH_GLES_DefaultProfileConfig(_THIS, int *mask, int *major, int *minor);

#endif /* SDL_VIDEO_DRIVER_NINTENDO_SWITCH && SDL_VIDEO_OPENGL_EGL */

#endif /* SDL_nintendoswitchopengles_h_ */

/* vi: set ts=4 sw=4 expandtab: */
