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

#include "SDL_nintendoswitchvideo.h"

extern "C" {
#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "SDL_nintendoswitchevents_c.h"
#include "SDL_assert.h"
}

#include <nn/hid.h>
#include <nn/hid/hid_Keyboard.h>
#include <nn/hid/hid_Mouse.h>
#include <nn/swkbd/swkbd_Api.h>
#include <nn/swkbd/swkbd_Result.h>

#define SWITCH_TOUCHID ((SDL_TouchID) 1)

typedef nn::hid::TouchScreenState<nn::hid::TouchStateCountMax> Touches;


void
NINTENDOSWITCH_InitEvents(_THIS)
{
    SDL_VideoData &data = *((SDL_VideoData *) _this->driverdata);    
    data.first_finger_down = -1;
    data.last_fingers_count = 0;
    nn::hid::InitializeKeyboard();
    nn::hid::InitializeMouse();
    nn::hid::InitializeTouchScreen();
    SDL_AddTouch(SWITCH_TOUCHID, SDL_TOUCH_DEVICE_DIRECT, "LCD screen");
}

void
NINTENDOSWITCH_QuitEvents(_THIS)
{
    // there are not Finalize events for keyboard, etc, at the moment.
}

static void
UpdateKeyboard(_THIS, const nn::hid::KeyboardKeySet &oldset, const nn::hid::KeyboardKeySet &newset)
{
    // !!! FIXME:
    // nn::hid API was clearly built around USB keyboard scancode values
    //  but the docs read like these are meant to be virtual keys. Alternately:
    //  they literally assume you have a US-QWERTY keyboard?

    // the good news is these all map directly to SDL scancodes in any case, so we
    //  don't have to parse each one; just blast through the bits.
    const int total = oldset.GetCount();
    for (int i = 0; i < total; i++) {
        const bool newval = newset[i];
        if (oldset[i] != newval) {
            SDL_SendKeyboardKey(newval ? SDL_PRESSED : SDL_RELEASED, (SDL_Scancode)i);
        }
    }
}

static void
PumpKeyboard(_THIS)
{
    SDL_VideoData &data = *((SDL_VideoData *) _this->driverdata);    
    nn::hid::KeyboardState states[nn::hid::KeyboardStateCountMax];
    const int numstates = nn::hid::GetKeyboardStates(states, SDL_arraysize(states));
    int idx;

    for (idx = numstates - 1; idx >= 0; idx--) {
        if (states[idx].samplingNumber > data.last_keyboard_sample) {
            break;  /* new data. */
        }
    }

    /* walk through all unseen states, oldest to newest */
    for (; idx >= 0; idx--) {
        const nn::hid::KeyboardState &state = states[idx];
        const SDL_bool thisconnected = state.attributes.Test<nn::hid::KeyboardAttribute::IsConnected>() ? SDL_TRUE : SDL_FALSE;
        if (!thisconnected && !data.keyboard_connected) {
            /* don't bother processing keys, no difference from previous state. */
        } else if (data.currentkeys == state.keys) {
            /* don't bother processing keys, no bits are different from previous state. */
        } else {
            UpdateKeyboard(_this, data.currentkeys, state.keys);
            data.currentkeys = state.keys;  /* make them match for next iteration. */
        }
        data.keyboard_connected = thisconnected;
        data.last_keyboard_sample = state.samplingNumber;
    }
}

static void
UpdateMouse(_THIS, const nn::hid::MouseState &oldstate, const nn::hid::MouseState &newstate)
{
    SDL_Window *window = _this->windows;

    /* all connected mice look like one device to NintendoSDK, so we just use mouseid 0. */
    if (newstate.deltaX || newstate.deltaY) {
        SDL_SendMouseMotion(window, 0, 1, (int) newstate.deltaX, (int) newstate.deltaY);
    }

    if (oldstate.buttons != newstate.buttons) {
        #define TEST_MBUTTON(nnsdk, sdl) { \
            const bool newpress = newstate.buttons.Test<nn::hid::MouseButton::nnsdk>(); \
            const bool oldpress = oldstate.buttons.Test<nn::hid::MouseButton::nnsdk>(); \
            if (newpress != oldpress) { \
                SDL_SendMouseButton(window, 0, newpress ? SDL_PRESSED : SDL_RELEASED, SDL_BUTTON_##sdl); \
            } \
        }

        TEST_MBUTTON(Left, LEFT);
        TEST_MBUTTON(Right, RIGHT);
        TEST_MBUTTON(Middle, MIDDLE);
        TEST_MBUTTON(Back, X1);
        TEST_MBUTTON(Forward, X2);

        #undef TEST_MBUTTON
    }

    /* !!! FIXME: no idea what resolution wheelDelta runs at. */
    if (newstate.wheelDelta) {
        SDL_SendMouseWheel(window, 0, (float) newstate.wheelDelta, 0.0f, SDL_MOUSEWHEEL_NORMAL);
    }
}

static void
PumpMouse(_THIS)
{
    SDL_VideoData &data = *((SDL_VideoData *) _this->driverdata);    
    nn::hid::MouseState states[nn::hid::MouseStateCountMax];
    const int numstates = nn::hid::GetMouseStates(states, SDL_arraysize(states));
    int idx;

    for (idx = numstates - 1; idx >= 0; idx--) {
        if (states[idx].samplingNumber > data.last_mouse_sample) {
            break;  /* new data. */
        }
    }

    /* walk through all unseen states, oldest to newest */
    for (; idx >= 0; idx--) {
        const nn::hid::MouseState &state = states[idx];
        const SDL_bool thisconnected = state.attributes.Test<nn::hid::MouseAttribute::IsConnected>() ? SDL_TRUE : SDL_FALSE;
        if (!thisconnected && !data.mouse_connected) {
            /* don't bother processing, no difference from previous state. */
        } else {
            UpdateMouse(_this, data.currentmousestate, state);
            data.currentmousestate = state;  /* make them match for next iteration. */
        }
        data.mouse_connected = thisconnected;
        data.last_mouse_sample = state.samplingNumber;
    }
}

static inline void
ScaleTouchToWindowAndNormalize(const SDL_Window *window, const int32_t x, const int32_t y, int *xscaled, int *yscaled, float *xnormalized, float *ynormalized)
{
    SDL_assert(window != NULL);
    // the Switch LCD is always 1280x720 ("720p")
    *xscaled = (int) (((float) x) * (((float) window->w) / 1280.0f));
    *yscaled = (int) (((float) y) * (((float) window->h) / 720.0f));
    *xnormalized = (float) *xscaled / (float) window->w;
    *ynormalized = (float) *yscaled / (float) window->h;
}

static void
UpdateTouch(_THIS, const Touches &state)
{
    SDL_VideoData &data = *((SDL_VideoData *) _this->driverdata);    
    SDL_Window *window = _this->windows;

    /* look for fingers the SDK reports are touching right now. */
    for (int32_t i = 0; i < state.count; i++) {
        const nn::hid::TouchState &touch = state.touches[i];
        const int32_t finger = touch.fingerId;
        SDL_LastFinger *already_touched = NULL;
        int xscaled, yscaled;
        float xnormalized, ynormalized;

        ScaleTouchToWindowAndNormalize(
            window,
            touch.x, touch.y,
            &xscaled, &yscaled,
            &xnormalized, &ynormalized
        );

        SDL_assert(finger != -1);

        for (int32_t j = 0; j < data.last_fingers_count; j++) {
            if (finger == data.last_fingers[j].finger) {
                already_touched = &data.last_fingers[j];
                break;
            }
        }

        /* if no finger was previously touching, treat the first new touch we see as a mouse input. */
        if ((data.first_finger_down == -1) && (!already_touched)) {
            data.first_finger_down = finger;
            SDL_SendMouseMotion(window, SDL_TOUCH_MOUSEID, 0, xscaled, yscaled);
            SDL_SendMouseButton(window, SDL_TOUCH_MOUSEID, SDL_PRESSED, SDL_BUTTON_LEFT);        
        }

        if (!already_touched) {  /* a new finger touching? Send an initial touch-down event. */
            SDL_SendTouch(SWITCH_TOUCHID, (SDL_FingerID)((size_t)finger), SDL_TRUE, xnormalized, ynormalized, 1.0f);
        } else {  /* if this finger was already touching (and has moved), send a move event. */
            if ((already_touched->x != touch.x) || (already_touched->y != touch.y)) {
                if (finger == data.first_finger_down) {  /* is this the finger we're treating as a mouse? */
                    SDL_SendMouseMotion(window, SDL_TOUCH_MOUSEID, 0, xscaled, yscaled);
                }
                SDL_SendTouchMotion(SWITCH_TOUCHID, (SDL_FingerID)((size_t)finger), xnormalized, ynormalized, 1.0f);
            }
        }
    }

    /* Look for fingers that were touching last time that aren't touching now. */
    for (int32_t i = 0; i < data.last_fingers_count; i++) {
        const SDL_LastFinger &last_finger = data.last_fingers[i];
        const int32_t finger = last_finger.finger;
        SDL_bool still_touched = SDL_FALSE;
        for (int32_t j = 0; j < state.count; j++) {
            if (state.touches[j].fingerId == finger) {
                still_touched = SDL_TRUE;
                break;
            }
        }

        /* not touching anymore? Send a touch-up event. */
        if (!still_touched) {
            if (finger == data.first_finger_down) {  /* were we treating this finger as the mouse? */
                data.first_finger_down = -1;
                SDL_SendMouseButton(window, SDL_TOUCH_MOUSEID, SDL_RELEASED, SDL_BUTTON_LEFT);
            }
            int xscaled, yscaled;
            float xnormalized, ynormalized;
            ScaleTouchToWindowAndNormalize(window, last_finger.x, last_finger.y, &xscaled, &yscaled, &xnormalized, &ynormalized);
            SDL_SendTouch(SWITCH_TOUCHID, (SDL_FingerID)((size_t)finger), SDL_FALSE, xnormalized, ynormalized, 1.0f);
        }
    }

    /* set up state to compare against next time. */
    for (int32_t i = 0; i < state.count; i++) {
        const nn::hid::TouchState &touch = state.touches[i];
        SDL_LastFinger &last_finger = data.last_fingers[i];
        last_finger.finger = touch.fingerId;
        last_finger.x = touch.x;
        last_finger.y = touch.y;
    }
    data.last_fingers_count = state.count;
}

static void
PumpTouchscreen(_THIS)
{
    SDL_VideoData &data = *((SDL_VideoData *) _this->driverdata);    
    Touches states[nn::hid::TouchScreenStateCountMax];
    const int numstates = nn::hid::GetTouchScreenStates(states, SDL_arraysize(states));
    int idx;

    for (idx = numstates - 1; idx >= 0; idx--) {
        if (states[idx].samplingNumber > data.last_touch_sample) {
            break;  /* new data. */
        }
    }

    /* walk through all unseen states, oldest to newest */
    for (; idx >= 0; idx--) {
        const Touches &state = states[idx];
        UpdateTouch(_this, state);
        data.last_touch_sample = state.samplingNumber;
    }
}

static void
PumpOperatingEnvironment(_THIS)
{
    nn::oe::Message msg;
    Uint8 evt;
    while (nn::oe::TryPopNotificationMessage(&msg))
    {
        switch (msg) {
            case nn::oe::MessageFocusStateChanged:
                if (nn::oe::GetCurrentFocusState() == nn::oe::FocusState_InFocus) {
                    evt = SDL_WINDOWEVENT_FOCUS_GAINED;
                } else { // nn::oe::FocusState_OutOfFocus, nn::oe::FocusState_Background
                    evt = SDL_WINDOWEVENT_FOCUS_LOST;
                }
                // FIXME: GetWindowFromID...?
                SDL_SendWindowEvent(_this->windows, evt, 0, 0);
                break;
            case nn::oe::MessageResume:
                // FIXME: GetWindowFromID...?
                SDL_SendWindowEvent(_this->windows, SDL_WINDOWEVENT_FOCUS_GAINED, 0, 0);
                break;
            case nn::oe::MessageOperationModeChanged: {
                int w, h;
                if (nn::oe::GetOperationMode() == nn::oe::OperationMode_Handheld) {
                    w = 1280;
                    h = 720;
                } else { // nn::oe::OperationMode_Console
                    w = 1920;
                    h = 1080;
                    // FIXME: nn::oe::GetDefaultDisplayResolution(&w, &h);
                }
                _this->displays->current_mode.w = _this->displays->desktop_mode.w = w;
                _this->displays->current_mode.h = _this->displays->desktop_mode.h = h;
                if ((_this->windows->flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP) {
                    _this->windows->w = w;
                    _this->windows->h = h;
                    NINTENDOSWITCH_SetWindowSize(_this, _this->windows);
                    _this->windows->w = _this->windows->h = 1;  // so the resized event isn't dropped.
                    SDL_SendWindowEvent(_this->windows, SDL_WINDOWEVENT_RESIZED, w, h);
                }
                break;
            }
            case nn::oe::MessagePerformanceModeChanged:
                // TODO: SDL_SendSysWMEvent -flibit
                break;
            case nn::oe::MessageExitRequest:
                SDL_SendAppEvent(SDL_APP_TERMINATING);
                break;
            default:
                SDL_assert(0 && "Unknown nn::oe::Message!");
                break;
        }
    }
}

void
NINTENDOSWITCH_PumpEvents(_THIS)
{
    if (!_this->windows) {
        return;
    }

    PumpKeyboard(_this);
    PumpMouse(_this);
    PumpTouchscreen(_this);
    PumpOperatingEnvironment(_this);
}


char *
SDL_NintendoSwitch_RunSoftwareKeyboard(const SDL_NintendoSwitch_SoftwareKeyboardMode mode, const char *guide, const int maxchars)
{
    char *retval = NULL;
    nn::swkbd::ShowKeyboardArg arg;
    nn::swkbd::MakePreset(&arg.keyboardConfig, nn::swkbd::Preset_Default);

    switch (mode) {
        case SDL_NINTENDOSWITCH_KEYBOARD_SINGLELINE:
            arg.keyboardConfig.keyboardMode = nn::swkbd::KeyboardMode_Full;
            arg.keyboardConfig.inputFormMode = nn::swkbd::InputFormMode_OneLine;
            arg.keyboardConfig.passwordMode = nn::swkbd::PasswordMode_Show;
            arg.keyboardConfig.isUseNewLine = false;
            break;
        case SDL_NINTENDOSWITCH_KEYBOARD_MULTILINE:
            arg.keyboardConfig.keyboardMode = nn::swkbd::KeyboardMode_Full;
            arg.keyboardConfig.inputFormMode = nn::swkbd::InputFormMode_MultiLine;
            arg.keyboardConfig.passwordMode = nn::swkbd::PasswordMode_Show;
            arg.keyboardConfig.isUseNewLine = true;
            break;
        case SDL_NINTENDOSWITCH_KEYBOARD_NUMERIC:
            arg.keyboardConfig.keyboardMode = nn::swkbd::KeyboardMode_Numeric;
            arg.keyboardConfig.inputFormMode = nn::swkbd::InputFormMode_OneLine;
            arg.keyboardConfig.passwordMode = nn::swkbd::PasswordMode_Show;
            arg.keyboardConfig.isUseNewLine = false;
            break;
        case SDL_NINTENDOSWITCH_KEYBOARD_PASSWORD:
            arg.keyboardConfig.keyboardMode = nn::swkbd::KeyboardMode_Full;
            arg.keyboardConfig.inputFormMode = nn::swkbd::InputFormMode_OneLine;
            arg.keyboardConfig.passwordMode = nn::swkbd::PasswordMode_Hide;
            arg.keyboardConfig.isUseNewLine = false;
            break;
        default:
            SDL_InvalidParamError("mode");
            return NULL;
    }

    arg.keyboardConfig.isUseUtf8 = true;

    if (maxchars > 0) {
        arg.keyboardConfig.textMaxLength = (int32_t) maxchars;
    }

    if (guide) {
        nn::swkbd::SetGuideTextUtf8(&arg.keyboardConfig, guide);
    }

    const size_t workbuflen = nn::swkbd::GetRequiredWorkBufferSize(false);
    void *workbuf = NULL;
    if (posix_memalign(&workbuf, nn::os::MemoryPageSize, workbuflen) != 0) {
        SDL_OutOfMemory();
        return NULL;  /* oh well. */
    }
    const size_t outbuflen = nn::swkbd::GetRequiredStringBufferSize();
    void *outbuf = NULL;
    if (posix_memalign(&outbuf, nn::os::MemoryPageSize, outbuflen) != 0) {
        free(workbuf);
        SDL_OutOfMemory();
        return NULL;  /* oh well. */
    }

    arg.workBuf = workbuf;
    arg.workBufSize = workbuflen;
    nn::swkbd::String str;
    str.ptr = outbuf;
    str.bufSize = outbuflen;
    const nn::Result rc = nn::swkbd::ShowKeyboard(&str, arg);
    if (nn::swkbd::ResultCanceled::Includes(rc)) {
        retval = SDL_strdup("");
        if (!retval) {
            SDL_OutOfMemory();
        }
    } else if (rc.IsSuccess()) {
        retval = SDL_strdup((const char *) str.ptr);
        if (!retval) {
            SDL_OutOfMemory();
        }
    } else {
        SDL_SetError("nn::swkbd::ShowKeyboard failed, err=%d", rc.GetDescription());
    }

    free(outbuf);
    free(workbuf);

    return retval;
}

#endif /* SDL_VIDEO_DRIVER_NINTENDO_SWITCH */

/* vi: set ts=4 sw=4 expandtab: */
