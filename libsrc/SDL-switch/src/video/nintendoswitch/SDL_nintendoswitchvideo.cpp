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

#if SDL_VIDEO_DRIVER_NINTENDO_SWITCH

#include <nn/vi.h>

/* SDL internals */
extern "C" {
#include "../SDL_sysvideo.h"
#include "SDL_version.h"
#include "SDL_syswm.h"
#include "SDL_loadso.h"
#include "SDL_events.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "SDL_hints.h"
}

#include "SDL_nintendoswitchvideo.h"

extern "C" {
#include "SDL_nintendoswitchevents_c.h"
#include "SDL_nintendoswitchopengles.h"
#include "SDL_nintendoswitchvulkan.h"
}

#include <nn/vi.h>
#include <nv/nv_MemoryManagement.h>

/* just wedging this in here because I need to call C++ from C, elsewhere. */
#include "SDL_assert.h"
#include <nn/err.h>
#include <nn/err/err_ApplicationErrorArg.h>
#include <nn/err/err_ShowApplicationErrorApi.h>
#include <nn/settings.h>
extern "C" { SDL_assert_state SDL_NintendoSwitch_PromptAssertion(const char *message); }
SDL_assert_state
SDL_NintendoSwitch_PromptAssertion(const char *message)
{
    // ApplicationErrorArg is like 4k of data, so allocate it on the heap, not the stack.
#ifndef NN_SDK_BUILD_RELEASE
    nn::err::ApplicationErrorArg *arg = new nn::err::ApplicationErrorArg(42, message, message,  nn::settings::LanguageCode::Make(nn::settings::Language_AmericanEnglish));
    nn::err::ShowApplicationError(*arg);
    delete arg;
#endif
    return SDL_ASSERTION_ABORT;
}

static void *
nvMalloc(size_t size, size_t alignment, void *data)
{
    void *retval = NULL;
    return (posix_memalign(&retval, alignment, size) == 0) ? retval : NULL;
}

static void
nvFree(void *ptr, void *data)
{
    free(ptr);
}

static void *
nvRealloc(void *ptr, size_t size, void *data)
{
    return realloc(ptr, size);
}


/*****************************************************************************/
/* SDL Video and Display initialization/handling functions                   */
/*****************************************************************************/
static int
NINTENDOSWITCH_VideoInit(_THIS)
{
    nv::SetGraphicsAllocator(nvMalloc, nvFree, nvRealloc, NULL);
    nv::SetGraphicsDevtoolsAllocator(nvMalloc, nvFree, nvRealloc, NULL);

    /* !!! FIXME: SDK docs don't specify how much you need or if it needs to be aligned,
       !!! FIXME:  but the example program does 8 megabytes with operator new...so I guess that's okay.
       This memory is "unmapped from the process" according to the docs, so I guess don't free it ever.
       (the example program doesn't, either.) */
    const size_t nvHeapLen = 8 * 1024 * 1024;
    void *nvHeap = SDL_malloc(nvHeapLen);
    if (!nvHeap) {
        return SDL_OutOfMemory();
    }

    nv::InitializeGraphics(nvHeap, nvHeapLen);
    nn::vi::Initialize();
    nn::oe::Initialize();
    nn::oe::SetOperationModeChangedNotificationEnabled(true);
    nn::oe::SetPerformanceModeChangedNotificationEnabled(true);
    nn::oe::SetResumeNotificationEnabled(true);
    nn::oe::SetFocusHandlingMode(nn::oe::FocusHandlingMode_SuspendAndNotify);

    // nn::vi::ListDisplays might be more correct for a generic NintendoSDK target, but for the Switch, we hardcode it.
    nn::vi::Display *vi_display = NULL;
    const nn::Result rc = nn::vi::OpenDefaultDisplay(&vi_display);
    if (rc.IsFailure()) {
        nn::vi::Finalize();
        nv::FinalizeGraphics();
        return SDL_SetError("Couldn't open default display! err=%d", rc.GetDescription());
    }

    SDL_DisplayMode current_mode;
    SDL_zero(current_mode);
    if (nn::oe::GetOperationMode() == nn::oe::OperationMode_Handheld) {
        current_mode.w = 1280;  /* 720p */
        current_mode.h = 720;
    } else {
        /* 480p, 720p, 1080p */
        current_mode.w = 1920;
        current_mode.h = 1080;
        // FIXME: nn::oe::GetDefaultDisplayResolution(&current_mode.w, &current_mode.h);
    }
    current_mode.refresh_rate = 60;
    current_mode.format = SDL_PIXELFORMAT_ABGR8888;
    current_mode.driverdata = NULL;

    SDL_VideoDisplay display;
    SDL_zero(display);
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;
    display.driverdata = vi_display;
    SDL_AddVideoDisplay(&display);

    NINTENDOSWITCH_InitEvents(_this);

    return 1;
}

static void
NINTENDOSWITCH_VideoQuit(_THIS)
{
    NINTENDOSWITCH_QuitEvents(_this);

    for (int i = 0; i < _this->num_displays; ++i) {
        SDL_VideoDisplay *display = &_this->displays[i];
        nn::vi::CloseDisplay((nn::vi::Display *) display->driverdata);
        display->driverdata = NULL;  /* so we don't SDL_free() an nn::vi::Display* later. */
    }

    ::eglReleaseThread();
    nn::vi::Finalize();
    nv::FinalizeGraphics();
}

static void
NINTENDOSWITCH_GetDisplayModes(_THIS, SDL_VideoDisplay * display)
{
    /* add a 1080p and 720p mode, for the built-in screen and an HDMI connection. */
    SDL_DisplayMode mode;
    SDL_zero(mode);
    mode.refresh_rate = 60;
    mode.format = SDL_PIXELFORMAT_ABGR8888;
    mode.driverdata = NULL;
    mode.w = 1920;  /* 1080p */
    mode.h = 1080;
    SDL_AddDisplayMode(display, &mode);
    mode.w = 1280;  /* 720p */
    mode.h = 720;
    SDL_AddDisplayMode(display, &mode);
}

static int
NINTENDOSWITCH_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    return 0;
}

static int
NINTENDOSWITCH_CreateWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *wdata;
    SDL_VideoDisplay *display;

    /* Allocate window internal data */
    wdata = (SDL_WindowData *) SDL_calloc(1, sizeof(SDL_WindowData));
    if (wdata == NULL) {
        return SDL_OutOfMemory();
    }
    display = SDL_GetDisplayForWindow(window);
    nn::vi::Display *vi_display = (nn::vi::Display *) display->driverdata;

    nn::Result rc;
    rc = nn::vi::CreateLayer(&wdata->vi_layer, vi_display, window->w, window->h);
    if (rc.IsFailure()) {
        SDL_free(wdata);
        return SDL_SetError("Failed to create video layer! err=%d", rc.GetDescription());
    }

    rc = nn::vi::SetLayerScalingMode(wdata->vi_layer, nn::vi::ScalingMode_PreserveAspectRatio);
    if (rc.IsFailure()) {
        nn::vi::DestroyLayer(wdata->vi_layer);
        SDL_free(wdata);
        return SDL_SetError("Failed to set video layer's scaling mode! err=%d", rc.GetDescription());
    }

    rc = nn::vi::GetNativeWindow(&wdata->vi_window, wdata->vi_layer);
    if (rc.IsFailure()) {
        nn::vi::DestroyLayer(wdata->vi_layer);
        SDL_free(wdata);
        return SDL_SetError("Failed to get video layer's native window! err=%d", rc.GetDescription());
    }

    if (window->flags & SDL_WINDOW_OPENGL) {
        wdata->egl_surface = SDL_EGL_CreateSurface(_this, (NativeWindowType) wdata->vi_window);
        if (wdata->egl_surface == EGL_NO_SURFACE) {
            nn::vi::DestroyLayer(wdata->vi_layer);
            SDL_free(wdata);
            return SDL_SetError("Could not create GLES window surface");
        }
    }

    /* Setup driver data for this window */
    window->driverdata = wdata;

    /* One window, it always has focus */
    SDL_SetMouseFocus(window);
    SDL_SetKeyboardFocus(window);

    /* Window has been successfully created */
    return 0;
}

static void
NINTENDOSWITCH_DestroyWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;

    if(data) {
        if (data->egl_surface != EGL_NO_SURFACE) {
            SDL_EGL_DestroySurface(_this, data->egl_surface);
        }
        if (data->vi_layer) {
            nn::vi::DestroyLayer(data->vi_layer);
        }
        SDL_free(data);
        window->driverdata = NULL;
    }
}

static int
NINTENDOSWITCH_CreateWindowFrom(_THIS, SDL_Window * window, const void *data)
{
    return SDL_Unsupported();
}

static void
NINTENDOSWITCH_SetWindowTitle(_THIS, SDL_Window * window)
{
}

static void
NINTENDOSWITCH_SetWindowIcon(_THIS, SDL_Window * window, SDL_Surface * icon)
{
}

static void
NINTENDOSWITCH_SetWindowPosition(_THIS, SDL_Window * window)
{
}

void
NINTENDOSWITCH_SetWindowSize(_THIS, SDL_Window * window)
{
    SDL_VideoDisplay *display;
    SDL_GLContext gl_ctx;
    SDL_WindowData *wdata = (SDL_WindowData *) window->driverdata;

    display = SDL_GetDisplayForWindow(window);
    nn::vi::Display *vi_display = (nn::vi::Display *) display->driverdata;

    if (window->flags & SDL_WINDOW_OPENGL)
    {
        SDL_assert(SDL_GL_GetCurrentWindow() == window);
        gl_ctx = SDL_GL_GetCurrentContext();
        SDL_EGL_MakeCurrent(_this, NULL, NULL);
        SDL_EGL_DestroySurface(_this, wdata->egl_surface);
    }

    nn::vi::DestroyLayer(wdata->vi_layer);

    nn::Result rc;
    rc = nn::vi::CreateLayer(&wdata->vi_layer, vi_display, window->w, window->h);
    if (rc.IsFailure()) {
        SDL_SetError("Failed to create video layer! err=%d", rc.GetDescription());
        return;
    }

    rc = nn::vi::SetLayerScalingMode(wdata->vi_layer, nn::vi::ScalingMode_PreserveAspectRatio);
    if (rc.IsFailure()) {
        nn::vi::DestroyLayer(wdata->vi_layer);
        SDL_SetError("Failed to set video layer's scaling mode! err=%d", rc.GetDescription());
        return;
    }

    rc = nn::vi::GetNativeWindow(&wdata->vi_window, wdata->vi_layer);
    if (rc.IsFailure()) {
        nn::vi::DestroyLayer(wdata->vi_layer);
        SDL_SetError("Failed to get video layer's native window! err=%d", rc.GetDescription());
        return;
    }

    if (window->flags & SDL_WINDOW_OPENGL) {
        wdata->egl_surface = SDL_EGL_CreateSurface(_this, (NativeWindowType)wdata->vi_window);
        if (wdata->egl_surface == EGL_NO_SURFACE) {
            nn::vi::DestroyLayer(wdata->vi_layer);
            SDL_SetError("Could not create GLES window surface");
            return;
        }
        if (gl_ctx != NULL) {
            SDL_EGL_MakeCurrent(_this, wdata->egl_surface, gl_ctx);
        }
    }
}

static void
NINTENDOSWITCH_ShowWindow(_THIS, SDL_Window * window)
{
}

static void
NINTENDOSWITCH_HideWindow(_THIS, SDL_Window * window)
{
}

static void
NINTENDOSWITCH_RaiseWindow(_THIS, SDL_Window * window)
{
}

static void
NINTENDOSWITCH_MaximizeWindow(_THIS, SDL_Window * window)
{
}

static void
NINTENDOSWITCH_MinimizeWindow(_THIS, SDL_Window * window)
{
}

static void
NINTENDOSWITCH_RestoreWindow(_THIS, SDL_Window * window)
{
}

static void
NINTENDOSWITCH_SetWindowResizable(_THIS, SDL_Window * window, SDL_bool resizable)
{
}

static void
NINTENDOSWITCH_SetWindowGrab(_THIS, SDL_Window * window, SDL_bool grabbed)
{
}

static SDL_bool
NINTENDOSWITCH_GetWindowWMInfo(_THIS, SDL_Window * window, struct SDL_SysWMinfo *info)
{
    if (info->version.major > SDL_MAJOR_VERSION) {
        SDL_SetError("application not compiled with SDL %d.%d",
                     SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
        return SDL_FALSE;
    }

    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    SDL_zerop(info);
    SDL_VERSION(&info->version);
    info->subsystem = SDL_SYSWM_NINTENDOSWITCH;
    info->info.nintendoswitch.egl_surface = data->egl_surface;
    info->info.nintendoswitch.vi_window = data->vi_window;
    info->info.nintendoswitch.vi_layer = data->vi_layer;

    return SDL_TRUE;
}

static int
NINTENDOSWITCH_Available(void)
{
    return 1;
}

static void
NINTENDOSWITCH_Destroy(SDL_VideoDevice * device)
{
    SDL_free(device->driverdata);
    SDL_free(device);
}

static SDL_VideoDevice *
NINTENDOSWITCH_Create(int devindex)
{
    SDL_VideoDevice *device;
    SDL_VideoData *phdata;

    /* Initialize SDL_VideoDevice structure */
    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (device == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    /* Initialize internal data */
    phdata = (SDL_VideoData *)SDL_calloc(1, sizeof(SDL_VideoData));
    if (phdata == NULL) {
        SDL_OutOfMemory();
        SDL_free(device);
        return NULL;
    }

    device->driverdata = phdata;

    /* Setup amount of available displays */
    device->num_displays = 0;

    /* Set device free function */
    device->free = NINTENDOSWITCH_Destroy;

    /* Setup all functions which we can handle */
    device->VideoInit = NINTENDOSWITCH_VideoInit;
    device->VideoQuit = NINTENDOSWITCH_VideoQuit;
    device->GetDisplayModes = NINTENDOSWITCH_GetDisplayModes;
    device->SetDisplayMode = NINTENDOSWITCH_SetDisplayMode;
    device->CreateSDLWindow = NINTENDOSWITCH_CreateWindow;
    device->CreateSDLWindowFrom = NINTENDOSWITCH_CreateWindowFrom;
    device->SetWindowTitle = NINTENDOSWITCH_SetWindowTitle;
    device->SetWindowIcon = NINTENDOSWITCH_SetWindowIcon;
    device->SetWindowPosition = NINTENDOSWITCH_SetWindowPosition;
    device->SetWindowSize = NINTENDOSWITCH_SetWindowSize;
    device->ShowWindow = NINTENDOSWITCH_ShowWindow;
    device->HideWindow = NINTENDOSWITCH_HideWindow;
    device->RaiseWindow = NINTENDOSWITCH_RaiseWindow;
    device->MaximizeWindow = NINTENDOSWITCH_MaximizeWindow;
    device->MinimizeWindow = NINTENDOSWITCH_MinimizeWindow;
    device->RestoreWindow = NINTENDOSWITCH_RestoreWindow;
    device->SetWindowResizable = NINTENDOSWITCH_SetWindowResizable;
    device->SetWindowGrab = NINTENDOSWITCH_SetWindowGrab;
    device->DestroyWindow = NINTENDOSWITCH_DestroyWindow;
    device->GetWindowWMInfo = NINTENDOSWITCH_GetWindowWMInfo;
    device->PumpEvents = NINTENDOSWITCH_PumpEvents;
    device->GL_LoadLibrary = NINTENDOSWITCH_GLES_LoadLibrary;
    device->GL_GetProcAddress = NINTENDOSWITCH_GLES_GetProcAddress;
    device->GL_UnloadLibrary = NINTENDOSWITCH_GLES_UnloadLibrary;
    device->GL_CreateContext = NINTENDOSWITCH_GLES_CreateContext;
    device->GL_MakeCurrent = NINTENDOSWITCH_GLES_MakeCurrent;
    device->GL_SetSwapInterval = NINTENDOSWITCH_GLES_SetSwapInterval;
    device->GL_GetSwapInterval = NINTENDOSWITCH_GLES_GetSwapInterval;
    device->GL_SwapWindow = NINTENDOSWITCH_GLES_SwapWindow;
    device->GL_DeleteContext = NINTENDOSWITCH_GLES_DeleteContext;
    device->GL_DefaultProfileConfig = NINTENDOSWITCH_GLES_DefaultProfileConfig;

#if SDL_VIDEO_VULKAN
    device->Vulkan_LoadLibrary = NINTENDOSWITCH_Vulkan_LoadLibrary;
    device->Vulkan_UnloadLibrary = NINTENDOSWITCH_Vulkan_UnloadLibrary;
    device->Vulkan_GetInstanceExtensions = NINTENDOSWITCH_Vulkan_GetInstanceExtensions;
    device->Vulkan_CreateSurface = NINTENDOSWITCH_Vulkan_CreateSurface;
#endif

    return device;
}

VideoBootStrap NINTENDOSWITCH_bootstrap = {
    "nintendoswitch",
    "Nintendo Switch Video Driver",
    NINTENDOSWITCH_Available,
    NINTENDOSWITCH_Create
};

#endif /* SDL_VIDEO_DRIVER_NINTENDO_SWITCH */

/* vi: set ts=4 sw=4 expandtab: */
