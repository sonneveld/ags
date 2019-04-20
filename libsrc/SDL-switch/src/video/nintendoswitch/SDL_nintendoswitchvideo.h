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

#ifndef __SDL_NINTENDOSWITCHVIDEO_H__
#define __SDL_NINTENDOSWITCHVIDEO_H__

#include "../../SDL_internal.h"
#include "../SDL_sysvideo.h"

#include <GLES3/gl32.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <nn/hid.h>
#include <nn/hid/hid_Keyboard.h>
#include <nn/hid/hid_Mouse.h>
#include <nn/vi.h>
#include <nn/os.h>
#include <nn/oe.h>

typedef struct
{
    int32_t finger;
    int32_t x;
    int32_t y;
} SDL_LastFinger;

typedef struct SDL_VideoData
{
    SDL_LastFinger last_fingers[nn::hid::TouchStateCountMax];
    int64_t last_touch_sample;
    int32_t last_fingers_count;
    int32_t first_finger_down;
    int64_t last_keyboard_sample;
    SDL_bool keyboard_connected;
    nn::hid::KeyboardKeySet currentkeys;
    int64_t last_mouse_sample;
    SDL_bool mouse_connected;
    nn::hid::MouseState currentmousestate;
} SDL_VideoData;

typedef struct SDL_WindowData
{
    EGLSurface egl_surface;
    nn::vi::Layer *vi_layer;
    nn::vi::NativeWindowHandle vi_window;
} SDL_WindowData;

void NINTENDOSWITCH_SetWindowSize(_THIS, SDL_Window * window);

#endif /* __SDL_NINTENDOSWITCHVIDEO_H__ */

/* vi: set ts=4 sw=4 expandtab: */
