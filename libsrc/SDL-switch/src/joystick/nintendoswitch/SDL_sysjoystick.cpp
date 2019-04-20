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

#ifdef SDL_JOYSTICK_NINTENDO_SWITCH

/* This is the Nintendo Switch implementation of the SDL joystick API */
extern "C" {
#include "SDL_joystick.h"
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"
#include "../../events/SDL_events_c.h"
#include "SDL_assert.h"
#include "SDL_haptic.h"
}

#include <nn/os.h>
#include <nn/hid.h>
#include <nn/hid/hid_Npad.h>
#include <nn/hid/hid_NpadJoy.h>
#include <nn/hid/hid_NpadJoy.h>

#include "SDL_sysjoystick_cpp.h"

static NpadDevice npad_devices[9];  /* 8 plus handheld. */
static NpadDevice *npad_device_index[9];
static int num_connected_npad_devices = 0;  /* total connected at the moment. */
static SDL_JoystickID instance_counter = 0;
static SDL_bool initialized = SDL_FALSE;

static NpadStyle
NpadStyleMaskToEnum(const nn::hid::NpadStyleSet &style)
{
    if (style.IsAllOff()) return NPAD_STYLE_DISCONNECTED;
    else if (style == nn::hid::NpadStyleHandheld::Mask) return NPAD_STYLE_HANDHELD;
    else if (style == nn::hid::NpadStyleJoyDual::Mask) return NPAD_STYLE_JOY_DUAL;
    else if (style == nn::hid::NpadStyleJoyRight::Mask) return NPAD_STYLE_JOY_RIGHT;
    else if (style == nn::hid::NpadStyleJoyLeft::Mask) return NPAD_STYLE_JOY_LEFT;
    else if (style == nn::hid::NpadStyleFullKey::Mask) return NPAD_STYLE_FULL_KEY;

    SDL_assert(!"unknown npad style?!");
    return NPAD_STYLE_DISCONNECTED;
}

static void
NINTENDOSWITCH_JoystickDetect(void)
{
    if (!initialized) {
        return;
    }

    for (int i = 0; i < SDL_arraysize(npad_devices); i++) {
        NpadDevice &dev = npad_devices[i];
        const NpadStyle style = NpadStyleMaskToEnum(nn::hid::GetNpadStyleSet(dev.npad_id));
        const bool previously = (dev.device_instance != -1);
        bool now = (style != NPAD_STYLE_DISCONNECTED);

        // If we're connected but the style changed, mark the device as disconnected.
        if ((previously && now) && (style != dev.previous_style)) {
            now = false;
        }

        if (previously != now) {
            if (now) {
                const int device_index = num_connected_npad_devices++;
                SDL_assert(device_index < SDL_arraysize(npad_device_index));
                SDL_assert(npad_device_index[device_index] == NULL);
                npad_device_index[device_index] = &dev;
                dev.previous_style = style;
                dev.device_instance = instance_counter++;
                SDL_PrivateJoystickAdded(dev.device_instance);
            } else {
                for (i = 0; i < num_connected_npad_devices; i++) {
                    if (npad_device_index[i] == &dev) {
                        const int mvitems = (num_connected_npad_devices - i) - 1;
                        SDL_assert(mvitems >= 0);
                        if (mvitems > 0) {
                            SDL_memmove(&npad_device_index[i], &npad_device_index[i+1], sizeof (npad_device_index[0]) * mvitems);
                        }
                        npad_device_index[i+mvitems] = NULL;
                        break;
                    }
                }
                num_connected_npad_devices--;
                const int device_instance = dev.device_instance;
                dev.device_instance = -1;
                SDL_PrivateJoystickRemoved(device_instance);
            }
        }
    }
}

/* Function to scan the system for joysticks.
 * It should return 0, or -1 on an unrecoverable fatal error.
 */
static int
NINTENDOSWITCH_JoystickInit(void)
{
    nn::hid::InitializeNpad();

    nn::hid::SetSupportedNpadStyleSet(
        nn::hid::NpadStyleJoyLeft::Mask | 
        nn::hid::NpadStyleJoyRight::Mask |
        nn::hid::NpadStyleJoyDual::Mask |
        nn::hid::NpadStyleFullKey::Mask |
        nn::hid::NpadStyleHandheld::Mask
    );

    nn::hid::SetNpadJoyHoldType(nn::hid::NpadJoyHoldType_Horizontal);

    static const nn::hid::NpadIdType supported_npad_ids[] = {
        nn::hid::NpadId::Handheld,
        nn::hid::NpadId::No1,
        nn::hid::NpadId::No2,
        nn::hid::NpadId::No3,
        nn::hid::NpadId::No4,
        nn::hid::NpadId::No5,
        nn::hid::NpadId::No6,
        nn::hid::NpadId::No7,
        nn::hid::NpadId::No8
    };
    nn::hid::SetSupportedNpadIdType(supported_npad_ids, SDL_arraysize(supported_npad_ids));

    SDL_zero(npad_devices);
    SDL_zero(npad_device_index);
    SDL_assert(SDL_arraysize(supported_npad_ids) == SDL_arraysize(npad_devices));
    for (int i = 0; i < SDL_arraysize(npad_devices); i++) {
        npad_devices[i].npad_id = supported_npad_ids[i];
        npad_devices[i].device_instance = -1;
    }
    num_connected_npad_devices = 0;
    instance_counter = 0;
    initialized = SDL_TRUE;

    #if 0  // for testing purposes.
    const nn::hid::NpadIdType supported_controllers[] = { nn::hid::NpadId::Handheld, nn::hid::NpadId::No1, nn::hid::NpadId::No2 }; 
    nn::hid::SetSupportedNpadIdType(supported_controllers, sizeof (supported_controllers) / sizeof (supported_controllers[0]));
    nn::hid::ControllerSupportArg arg;
    arg.SetDefault();
    arg.playerCountMax = 2;
    arg.enableLeftJustify = false;
    nn::hid::ShowControllerSupport(arg);
    //nn::hid::SetNpadJoyHoldType(nn::hid::NpadJoyHoldType_Vertical);
    #endif

    NINTENDOSWITCH_JoystickDetect();

    return 0;
}

static int
NINTENDOSWITCH_JoystickGetCount(void)
{
    return num_connected_npad_devices;
}

static int
GetNpadIdInfo(const nn::hid::NpadIdType &npad_id, SDL_JoystickGUID *guid, const char **name)
{
    SDL_JoystickGUID dummyguid; if (!guid) guid = &dummyguid;
    const char *dummyname; if (!name) name = &dummyname;

    switch (NpadStyleMaskToEnum(nn::hid::GetNpadStyleSet(npad_id))) {
        #define NPADINFO(typ, str, guidstr) \
            case NPAD_STYLE_##typ: \
                SDL_assert(SDL_strlen(guidstr) == 14); \
                *name = str; \
                SDL_memcpy(guid->data, guidstr, 14); \
                guid->data[14] = 'x'; /* 'x' followed by a number is how SDL reports xinput controller types */ \
                guid->data[15] = 0x01; /* tell the higher level it's a SDL_JOYSTICK_TYPE_GAMECONTROLLER */ \
                break
        NPADINFO(HANDHELD, "Joy-Cons connected to console", "npad_handheld0");
        NPADINFO(JOY_DUAL, "Joy-Con (Dual)", "npad_joydual00");
        NPADINFO(JOY_RIGHT, "Joy-Con (Right)", "npad_joyright0");
        NPADINFO(JOY_LEFT, "Joy-Con (Left)", "npad_joyleft00");
        NPADINFO(FULL_KEY, "Switch Pro Controller compatible", "npad_fullkey00");
        #undef NPADINFO

        case NPAD_STYLE_DISCONNECTED:
            SDL_zerop(guid);
            *name = NULL;
            return SDL_SetError("No such Npad ID connected");
    }

    return 0;
}

/* Function to get the device-dependent name of a joystick */
static const char *
NINTENDOSWITCH_JoystickGetDeviceName(int device_index)
{
    const char *name;
    GetNpadIdInfo(npad_device_index[device_index]->npad_id, NULL, &name);
    return name;
}

/* Function to perform the mapping from device index to the instance id for this index */
static SDL_JoystickID
NINTENDOSWITCH_JoystickGetDeviceInstanceID(int device_index)
{
    return npad_device_index[device_index]->device_instance;
}

static int
NINTENDOSWITCH_JoystickGetDevicePlayerIndex(int device_index)
{
    // we treat joycons attached to the console rails as player index zero, then other connected controllers start at 1.
    const nn::hid::NpadIdType npadid = npad_device_index[device_index]->npad_id;
    if (npadid == nn::hid::NpadId::Handheld) {
        return 0;
    }

    SDL_assert(((int) nn::hid::NpadId::No1) == 0);
    return ((int) npadid) + 1;
}

/* Function to open a joystick for use.
   The joystick to open is specified by the device index.
   This should fill the nbuttons and naxes fields of the joystick structure.
   It returns 0, or -1 if there is an error.
 */
static int
NINTENDOSWITCH_JoystickOpen(SDL_Joystick * joystick, int device_index)
{
    const NpadDevice *dev = npad_device_index[device_index];
    SDL_assert(dev != NULL);
    const NpadStyle style = NpadStyleMaskToEnum(nn::hid::GetNpadStyleSet(dev->npad_id));

    if (style == NPAD_STYLE_DISCONNECTED) {
        return SDL_SetError("device is disconnected"); // huh?
    }

    joystick->hwdata = (struct joystick_hwdata *)
        SDL_calloc(1, sizeof(*joystick->hwdata));
    if (joystick->hwdata == NULL) {
        return SDL_OutOfMemory();
    }

    joystick->hwdata->npad_device = dev;
    joystick->hwdata->haptic_effect = -1;
    joystick->instance_id = dev->device_instance;
    joystick->nhats = 0;

    switch (style) {
        #define NPADCFG(typ, buttons, axes) case NPAD_STYLE_##typ: joystick->nbuttons = buttons; joystick->naxes = axes; break
        NPADCFG(HANDHELD, 16, 4);
        NPADCFG(JOY_DUAL, 20, 4);
        NPADCFG(JOY_RIGHT, 10, 2);
        NPADCFG(JOY_LEFT, 10, 2);
        NPADCFG(FULL_KEY, 16, 4);
        NPADCFG(DISCONNECTED, 0, 0);
        #undef NPADCFG
    }

    return 0;
}

static void
UpdateNpadButtonState(SDL_Joystick *joystick, const nn::hid::NpadButtonSet &available, const nn::hid::NpadButtonSet &newstate)
{
    const nn::hid::NpadButtonSet &oldstate = joystick->hwdata->previous_buttons;
    if (newstate != oldstate) {
        Uint8 sdlbutton = 0;
        #define TEST_JBUTTON(nnsdk) { \
            if (available.Test<nn::hid::NpadJoyButton::nnsdk>()) { \
                const bool newpress = newstate.Test<nn::hid::NpadJoyButton::nnsdk>(); \
                const bool oldpress = oldstate.Test<nn::hid::NpadJoyButton::nnsdk>(); \
                if (newpress != oldpress) { \
                    SDL_PrivateJoystickButton(joystick, sdlbutton, newpress ? SDL_PRESSED : SDL_RELEASED); \
                } \
                sdlbutton++; \
            } \
        }

        TEST_JBUTTON(Up);
        TEST_JBUTTON(Down);
        TEST_JBUTTON(Left);
        TEST_JBUTTON(Right);
        TEST_JBUTTON(A);
        TEST_JBUTTON(B);
        TEST_JBUTTON(X);
        TEST_JBUTTON(Y);
        TEST_JBUTTON(L);
        TEST_JBUTTON(R);
        TEST_JBUTTON(ZL);
        TEST_JBUTTON(ZR);
        TEST_JBUTTON(StickL);
        TEST_JBUTTON(StickR);
        TEST_JBUTTON(Plus);
        TEST_JBUTTON(Minus);
        TEST_JBUTTON(LeftSL);
        TEST_JBUTTON(LeftSR);
        TEST_JBUTTON(RightSL);
        TEST_JBUTTON(RightSR);
        #undef TEST_JBUTTON

        joystick->hwdata->previous_buttons = newstate;
    }
}

static inline void 
UpdateNpadStickState(SDL_Joystick *joystick, const int baseaxis, const nn::hid::AnalogStickState &newstate)
{
    SDL_assert(newstate.x >= -32768);
    SDL_assert(newstate.x <= 32767);
    SDL_assert(newstate.y >= -32768);
    SDL_assert(newstate.y <= 32767);
    SDL_PrivateJoystickAxis(joystick, baseaxis, newstate.x);
    SDL_PrivateJoystickAxis(joystick, baseaxis + 1, -newstate.y);  // this axis is backwards from what we want. Flip it!
}

#define FULLKEY_BUTTON_MASK \
    nn::hid::NpadJoyButton::A::Mask | nn::hid::NpadJoyButton::B::Mask | \
    nn::hid::NpadJoyButton::X::Mask | nn::hid::NpadJoyButton::Y::Mask | \
    nn::hid::NpadJoyButton::L::Mask | nn::hid::NpadJoyButton::R::Mask | \
    nn::hid::NpadJoyButton::ZL::Mask | nn::hid::NpadJoyButton::ZR::Mask | \
    nn::hid::NpadJoyButton::StickL::Mask | nn::hid::NpadJoyButton::StickR::Mask | \
    nn::hid::NpadJoyButton::Plus::Mask | nn::hid::NpadJoyButton::Minus::Mask | \
    nn::hid::NpadJoyButton::Up::Mask | nn::hid::NpadJoyButton::Down::Mask | \
    nn::hid::NpadJoyButton::Left::Mask | nn::hid::NpadJoyButton::Right::Mask

static void 
UpdateNpadState(SDL_Joystick *joystick, const nn::hid::NpadHandheldState &state)
{
    static const nn::hid::NpadButtonSet available_buttons(FULLKEY_BUTTON_MASK);
    UpdateNpadButtonState(joystick, available_buttons, state.buttons);
    UpdateNpadStickState(joystick, 0, state.analogStickL);
    UpdateNpadStickState(joystick, 2, state.analogStickR);
}

static void
UpdateNpadState(SDL_Joystick *joystick, const nn::hid::NpadFullKeyState &state)
{
    static const nn::hid::NpadButtonSet available_buttons(FULLKEY_BUTTON_MASK);
    UpdateNpadButtonState(joystick, available_buttons, state.buttons);
    UpdateNpadStickState(joystick, 0, state.analogStickL);
    UpdateNpadStickState(joystick, 2, state.analogStickR);
}

static void
UpdateNpadState(SDL_Joystick *joystick, const nn::hid::NpadJoyLeftState &state)
{
    static const nn::hid::NpadButtonSet available_buttons(
        nn::hid::NpadJoyButton::Up::Mask | nn::hid::NpadJoyButton::Down::Mask |
        nn::hid::NpadJoyButton::Left::Mask | nn::hid::NpadJoyButton::Right::Mask |
        nn::hid::NpadJoyButton::L::Mask | nn::hid::NpadJoyButton::ZL::Mask |
        nn::hid::NpadJoyButton::StickL::Mask | nn::hid::NpadJoyButton::Minus::Mask |
        nn::hid::NpadJoyButton::LeftSL::Mask | nn::hid::NpadJoyButton::LeftSR::Mask
    );

    // if horizontal, change button positions to deal with rotation.    
    if (nn::hid::GetNpadJoyHoldType() == nn::hid::NpadJoyHoldType_Horizontal) {
        const nn::hid::NpadButtonSet &buttons = state.buttons;
        nn::hid::NpadButtonSet rotatedButtons = buttons;
        #define ROTATEBUTTON(from, to) rotatedButtons.Set<nn::hid::NpadJoyButton::to>(buttons.Test<nn::hid::NpadJoyButton::from>())
        ROTATEBUTTON(Up, Left);
        ROTATEBUTTON(Right, Up);
        ROTATEBUTTON(Down, Right);
        ROTATEBUTTON(Left, Down);
        #undef ROTATEBUTTON
        const nn::hid::AnalogStickState rotatedStickL = { -state.analogStickL.y, state.analogStickL.x };
        UpdateNpadButtonState(joystick, available_buttons, rotatedButtons);
        UpdateNpadStickState(joystick, 0, rotatedStickL);
    } else {
        UpdateNpadButtonState(joystick, available_buttons, state.buttons);
        UpdateNpadStickState(joystick, 0, state.analogStickL);
    }
}

static void
UpdateNpadState(SDL_Joystick *joystick, const nn::hid::NpadJoyRightState &state)
{
    static const nn::hid::NpadButtonSet available_buttons(
        nn::hid::NpadJoyButton::A::Mask | nn::hid::NpadJoyButton::B::Mask |
        nn::hid::NpadJoyButton::X::Mask | nn::hid::NpadJoyButton::Y::Mask |
        nn::hid::NpadJoyButton::R::Mask | nn::hid::NpadJoyButton::ZR::Mask |
        nn::hid::NpadJoyButton::StickR::Mask | nn::hid::NpadJoyButton::Plus::Mask |
        nn::hid::NpadJoyButton::RightSL::Mask | nn::hid::NpadJoyButton::RightSR::Mask
    );

    // if horizontal, change button positions to deal with rotation.    
    if (nn::hid::GetNpadJoyHoldType() == nn::hid::NpadJoyHoldType_Horizontal) {
        const nn::hid::NpadButtonSet &buttons = state.buttons;
        nn::hid::NpadButtonSet rotatedButtons = buttons;
        #define ROTATEBUTTON(from, to) rotatedButtons.Set<nn::hid::NpadJoyButton::to>(buttons.Test<nn::hid::NpadJoyButton::from>())
        ROTATEBUTTON(X, A);
        ROTATEBUTTON(A, B);
        ROTATEBUTTON(B, Y);
        ROTATEBUTTON(Y, X);
        #undef ROTATEBUTTON
        const nn::hid::AnalogStickState rotatedStickR = { state.analogStickR.y, -state.analogStickR.x };
        UpdateNpadButtonState(joystick, available_buttons, rotatedButtons);
        UpdateNpadStickState(joystick, 0, rotatedStickR);
    } else {
        UpdateNpadButtonState(joystick, available_buttons, state.buttons);
        UpdateNpadStickState(joystick, 0, state.analogStickR);
    }
}

static void
UpdateNpadState(SDL_Joystick *joystick, const nn::hid::NpadJoyDualState &state)
{
    static const nn::hid::NpadButtonSet available_buttons(
        FULLKEY_BUTTON_MASK |
        nn::hid::NpadJoyButton::LeftSL::Mask | nn::hid::NpadJoyButton::LeftSR::Mask |
        nn::hid::NpadJoyButton::RightSL::Mask | nn::hid::NpadJoyButton::RightSR::Mask
    );
    UpdateNpadButtonState(joystick, available_buttons, state.buttons);
    UpdateNpadStickState(joystick, 0, state.analogStickL);
    UpdateNpadStickState(joystick, 2, state.analogStickR);
}

template <class npadStateT>
static void
UpdateNpadStyle(SDL_Joystick *joystick)
{
    joystick_hwdata *hw = joystick->hwdata;
    const NpadDevice &dev = *hw->npad_device;
    const int64_t last_sample = hw->last_npad_sample;
    npadStateT states[nn::hid::NpadStateCountMax];
    int idx;

    if (dev.device_instance == -1) {
        return;  /* disconnected. */
    }

    const int numstates = nn::hid::GetNpadStates(states, SDL_arraysize(states), dev.npad_id);
    for (idx = numstates - 1; idx >= 0; idx--) {
        if (states[idx].samplingNumber > last_sample) {
            break;  /* new data. */
        }
    }

    /* walk through all unseen states, oldest to newest */
    for (; idx >= 0; idx--) {
        const npadStateT &state = states[idx];
        UpdateNpadState(joystick, state);
        hw->last_npad_sample = state.samplingNumber;
    }
}


/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
static void
NINTENDOSWITCH_JoystickUpdate(SDL_Joystick *joystick)
{
    const NpadDevice &dev = *joystick->hwdata->npad_device;
    const NpadStyle style = NpadStyleMaskToEnum(nn::hid::GetNpadStyleSet(dev.npad_id));

    if (style != dev.previous_style) {
        return;  // style changed, should disconnect shortly. Ignore until then.
    }

    switch (style) {
        #define NPADUPDATE(sdltyp, nntyp) case NPAD_STYLE_##sdltyp: UpdateNpadStyle<nn::hid::Npad##nntyp##State>(joystick); break
        NPADUPDATE(HANDHELD, Handheld);
        NPADUPDATE(JOY_DUAL, JoyDual);
        NPADUPDATE(JOY_RIGHT, JoyRight);
        NPADUPDATE(JOY_LEFT, JoyLeft);
        NPADUPDATE(FULL_KEY, FullKey);
        #undef NPADUPDATE
        case NPAD_STYLE_DISCONNECTED: break;  // got disconnected?
    }
}

/* Function to close a joystick after use */
static void
NINTENDOSWITCH_JoystickClose(SDL_Joystick * joystick)
{
    if (joystick->hwdata->haptic != NULL) {
        SDL_HapticClose(joystick->hwdata->haptic);
    }
    SDL_free(joystick->hwdata);
}

/* Function to perform any system-specific joystick related cleanup */
static void
NINTENDOSWITCH_JoystickQuit(void)
{
    SDL_zero(npad_devices);
    SDL_zero(npad_device_index);
    num_connected_npad_devices = 0;
    instance_counter = 0;
    initialized = SDL_FALSE;
}

static SDL_JoystickGUID
NINTENDOSWITCH_JoystickGetDeviceGUID( int device_index )
{
    SDL_JoystickGUID guid;
    GetNpadIdInfo(npad_device_index[device_index]->npad_id, &guid, NULL);
    return guid;
}

static int
NINTENDOSWITCH_JoystickRumble(SDL_Joystick * joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration_ms)
{
    SDL_HapticEffect effect;
    effect.type = SDL_HAPTIC_LEFTRIGHT;
    effect.leftright.length = duration_ms;
    effect.leftright.large_magnitude = low_frequency_rumble;
    effect.leftright.small_magnitude = high_frequency_rumble;

    /* Generate the haptic device if needed */
    if (joystick->hwdata->haptic == NULL) {
        joystick->hwdata->haptic = SDL_HapticOpenFromJoystick(joystick);
        if (joystick->hwdata->haptic == NULL) {
            /* No device, bail. */
            return -1;
        }
    }

    /* Generate the haptic effect if needed */
    if (joystick->hwdata->haptic_effect < 0) {
        joystick->hwdata->haptic_effect = SDL_HapticNewEffect(joystick->hwdata->haptic, &effect);
        if (joystick->hwdata->haptic_effect < 0) {
            /* Effect wasn't made, bail */
            return joystick->hwdata->haptic_effect;
        }
    }

    /* Run the effect, finally. */
    if (low_frequency_rumble == 0 && high_frequency_rumble == 0) {
        return SDL_HapticStopAll(joystick->hwdata->haptic);
    }
    if (SDL_HapticUpdateEffect(joystick->hwdata->haptic, joystick->hwdata->haptic_effect, &effect) < 0) {
        return -1;
    }
    return SDL_HapticRunEffect(joystick->hwdata->haptic, joystick->hwdata->haptic_effect, 1);
}

int
SDL_NintendoSwitch_JoystickNpadId(SDL_Joystick *joystick, Sint32 *npadid)
{
    if (!joystick) {
        return SDL_InvalidParamError("joystick");
    } else if (!npadid) {
        return SDL_InvalidParamError("npadid");
    }

    *npadid = (Sint32) joystick->hwdata->npad_device->npad_id;
    return 0;
}

int
SDL_NintendoSwitch_JoystickNpadIdForIndex(const int device_index, Sint32 *npadid)
{
    if ((device_index < 0) || (device_index >= num_connected_npad_devices)) {
        return SDL_SetError("There are %d joysticks available", SDL_NumJoysticks());
    } else if (!npadid) {
        return SDL_InvalidParamError("npadid");
    }

    *npadid = (Sint32) npad_device_index[device_index]->npad_id;
    return 0;
}

SDL_JoystickDriver SDL_NINTENDOSWITCH_JoystickDriver =
{
    NINTENDOSWITCH_JoystickInit,
    NINTENDOSWITCH_JoystickGetCount,
    NINTENDOSWITCH_JoystickDetect,
    NINTENDOSWITCH_JoystickGetDeviceName,
    NINTENDOSWITCH_JoystickGetDevicePlayerIndex,
    NINTENDOSWITCH_JoystickGetDeviceGUID,
    NINTENDOSWITCH_JoystickGetDeviceInstanceID,
    NINTENDOSWITCH_JoystickOpen,
    NINTENDOSWITCH_JoystickRumble,
    NINTENDOSWITCH_JoystickUpdate,
    NINTENDOSWITCH_JoystickClose,
    NINTENDOSWITCH_JoystickQuit,
};

#endif /* SDL_JOYSTICK_NINTENDO_SWITCH */

/* vi: set ts=4 sw=4 expandtab: */
