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

#ifdef SDL_HAPTIC_NINTENDO_SWITCH

extern "C" {
#include "SDL_timer.h"
#include "../../thread/SDL_systhread.h"
#include "SDL_haptic.h"
#include "../SDL_syshaptic.h"
#include "SDL_assert.h"
}

#include <nn/hid.h>
#include <nn/hid/hid_Npad.h>
#include <nn/hid/hid_NpadJoy.h>
#include <nn/hid/hid_Vibration.h>

struct haptic_hwdata
{
    nn::hid::NpadIdType npad_id;
    nn::hid::VibrationDeviceHandle vibration_devices[2];
    nn::hid::VibrationDevicePosition motor_position[2];
    int num_vibration_devices;
    SDL_Thread *thread;
    SDL_mutex *mutex;
    Uint32 stopTicks;
    SDL_atomic_t stopThread;
};

struct haptic_hweffect
{
    int unused;
};

/* I stole this thread idea from SDL's Xinput haptic code. */
/* !!! FIXME: do all the devices on one thread? */
static int SDLCALL
NpadHapticThread(void *arg)
{
    haptic_hwdata *hwdata = (haptic_hwdata *) arg;
    const nn::hid::VibrationValue stop_vibrating = nn::hid::VibrationValue::Make();

    while (!SDL_AtomicGet(&hwdata->stopThread)) {
        SDL_Delay(50);
        SDL_LockMutex(hwdata->mutex);
        /* If we're currently running and need to stop... */
        if (hwdata->stopTicks) {
            if ((hwdata->stopTicks != SDL_HAPTIC_INFINITY) && SDL_TICKS_PASSED(SDL_GetTicks(), hwdata->stopTicks)) {
                hwdata->stopTicks = 0;
                for (int i = 0; i < hwdata->num_vibration_devices; i++) {
                    nn::hid::SendVibrationValue(hwdata->vibration_devices[i], stop_vibrating);
                }
            }
        }
        SDL_UnlockMutex(hwdata->mutex);
    }

    return 0;
}

int
SDL_SYS_HapticInit(void)
{
    return 0;
}

int
SDL_SYS_NumHaptics(void)
{
    return nn::hid::IsVibrationPermitted() ? SDL_NumJoysticks() : 0;
}

const char *
SDL_SYS_HapticName(int index)
{
    return SDL_JoystickNameForIndex(index);
}


static int
NpadHapticOpen(SDL_Haptic * haptic, const nn::hid::NpadIdType &npad_id)
{
    haptic->effects = NULL;

    if (!nn::hid::IsVibrationPermitted()) {
        return SDL_SetError("Vibration not permitted by system settings");
    }

    /* Prepare effects memory. */
    haptic_effect *effects = (haptic_effect *) SDL_calloc(1, sizeof (haptic_effect));
    if (effects == NULL) {
        return SDL_OutOfMemory();
    }

    /* Allocate the hwdata */
    haptic_hwdata *hwdata = (haptic_hwdata *) SDL_calloc(1, sizeof (*haptic->hwdata));
    if (hwdata == NULL) {
        SDL_free(effects);
        return SDL_OutOfMemory();
    }

    haptic_hwdata &hw = *hwdata;
    const nn::hid::NpadStyleSet style = nn::hid::GetNpadStyleSet(npad_id);
    const int numdevs = nn::hid::GetVibrationDeviceHandles(hw.vibration_devices, SDL_arraysize(hw.vibration_devices), npad_id, style);
    if (numdevs <= 0) {
        SDL_free(hwdata);
        SDL_free(effects);
        return SDL_SetError("Couldn't get vibration handles for this device");
    }

    hw.mutex = SDL_CreateMutex();
    if (hw.mutex == NULL) {
        SDL_free(hwdata);
        SDL_free(effects);
        return SDL_SetError("Couldn't create Npad haptic mutex");
    }

    for (int i = 0; i < numdevs; i++) {
        nn::hid::VibrationDeviceInfo info;
        nn::hid::InitializeVibrationDevice(hw.vibration_devices[i]);
        nn::hid::GetVibrationDeviceInfo(&info, hw.vibration_devices[i]);
        hw.motor_position[i] = info.position;
    }

    hw.npad_id = npad_id;
    hw.num_vibration_devices = numdevs;
    SDL_AtomicSet(&hw.stopThread, 0);

    haptic->hwdata = hwdata;
    haptic->effects = effects;
    haptic->supported = SDL_HAPTIC_LEFTRIGHT;
    haptic->neffects = 1;
    haptic->nplaying = 1;

    char threadName[32];
    SDL_snprintf(threadName, sizeof(threadName), "SDLNpadHapticDev_0x%X", (unsigned int) npad_id);
    hw.thread = SDL_CreateThreadInternal(NpadHapticThread, threadName, 4 * 4096, haptic->hwdata);
    if (hw.thread == NULL) {
        SDL_DestroyMutex(hw.mutex); 
        SDL_free(haptic->effects);
        SDL_free(haptic->hwdata);
        haptic->effects = NULL;
        haptic->hwdata = NULL;
        return SDL_SetError("Couldn't create Npad haptic thread");
    }

    return 0;
}

int
SDL_SYS_HapticOpen(SDL_Haptic * haptic)
{
    Sint32 npad_id_int32;
    if (SDL_NintendoSwitch_JoystickNpadIdForIndex(haptic->index, &npad_id_int32) == -1) {
        return -1;
    }

    const nn::hid::NpadIdType npad_id = (const nn::hid::NpadIdType) npad_id_int32;
    return NpadHapticOpen(haptic, npad_id);
}

int
SDL_SYS_HapticMouse(void)
{
    return -1;  /* no haptic mice on this platform. */
}


int
SDL_SYS_JoystickIsHaptic(SDL_Joystick * joystick)
{
    if (!nn::hid::IsVibrationPermitted()) {
        return 0;
    }

    Sint32 npad_id_int32;
    if (SDL_NintendoSwitch_JoystickNpadId(joystick, &npad_id_int32) == -1) {
        return 0;
    }

    const nn::hid::NpadIdType npad_id = (const nn::hid::NpadIdType) npad_id_int32;
    const nn::hid::NpadStyleSet style = nn::hid::GetNpadStyleSet(npad_id);
    nn::hid::VibrationDeviceHandle vibration_devices[4];
    return (nn::hid::GetVibrationDeviceHandles(vibration_devices, SDL_arraysize(vibration_devices), npad_id, style) > 0);
}


int
SDL_SYS_HapticOpenFromJoystick(SDL_Haptic * haptic, SDL_Joystick * joystick)
{
    Sint32 npad_id_int32;
    if (SDL_NintendoSwitch_JoystickNpadId(joystick, &npad_id_int32) == -1) {
        return -1;
    }
    const nn::hid::NpadIdType npad_id = (const nn::hid::NpadIdType) npad_id_int32;
    return NpadHapticOpen(haptic, npad_id);
}


int
SDL_SYS_JoystickSameHaptic(SDL_Haptic * haptic, SDL_Joystick * joystick)
{
    Sint32 npad_id;
    if (SDL_NintendoSwitch_JoystickNpadId(joystick, &npad_id) == -1) {
        return 0;
    }
    return (haptic->hwdata->npad_id == ((nn::hid::NpadIdType) npad_id)) ? 1 : 0;
}


void
SDL_SYS_HapticClose(SDL_Haptic * haptic)
{
    SDL_AtomicSet(&haptic->hwdata->stopThread, 1);
    SDL_WaitThread(haptic->hwdata->thread, NULL);
    SDL_free(haptic->effects);
    haptic->effects = NULL;
    haptic->neffects = 0;
    SDL_DestroyMutex(haptic->hwdata->mutex);
    SDL_free(haptic->hwdata);
    haptic->hwdata = NULL;
}


void
SDL_SYS_HapticQuit(void)
{
    // no-op.
    // !!! FIXME: the higher level doesn't close open devices on subsystem
    // quit and we don't keep a list ourselves. This is a bug at the higher level.
    // make sure your app closes all devices manually until this is fixed!
}


int
SDL_SYS_HapticNewEffect(SDL_Haptic * haptic,
                        struct haptic_effect *effect, SDL_HapticEffect * base)
{
    SDL_assert(base->type == SDL_HAPTIC_LEFTRIGHT);  /* should catch this at higher level */

    effect->hweffect = (haptic_hweffect *) SDL_calloc(1, sizeof (haptic_hweffect));
    if (effect->hweffect == NULL) {
        SDL_OutOfMemory();
        return -1;
    }

    const int result = SDL_SYS_HapticUpdateEffect(haptic, effect, base);
    if (result < 0) {
        SDL_free(effect->hweffect);
        effect->hweffect = NULL;
    }
    return result;
}

static void
NpadDoVibration(const SDL_Haptic *haptic, const SDL_HapticLeftRight &leftright)
{
    /* XInput docs (what SDL_HapticLeftRight is trying to mimic) say this:
       "The left motor is the low-frequency rumble motor. The right motor is
        the high-frequency rumble motor. The two motors are not the same, and
        they create different vibration effects."
       So I take this to mean if you have a left/right motor pair available on
        your Npad, give the one on the left with a non-zero amplitudeLow, and
        the one on the right a non-zero amplitudeHigh, and use the default
        low/high frequencies for both. If you only have one, we can either
        try setting both amplitudes, or maybe just average them? I'm not sure.
    */

    /* SDL_HapticEffect has max magnitude of 32767, so clamp and normalize */
    const float small = (float) SDL_min(leftright.small_magnitude, 32767);
    const float large = (float) SDL_min(leftright.large_magnitude, 32767);
    const float nnleft = small / 32767.0f;
    const float nnright = large / 32767.0f;

    haptic_hwdata &hw = *haptic->hwdata;
    const nn::hid::VibrationValue vval = nn::hid::VibrationValue::Make(nnleft, nn::hid::VibrationFrequencyLowDefault, nnright, nn::hid::VibrationFrequencyHighDefault);
    for (int i = 0; i < hw.num_vibration_devices; i++) {
        nn::hid::SendVibrationValue(hw.vibration_devices[i], vval);
    }
}


int
SDL_SYS_HapticUpdateEffect(SDL_Haptic * haptic,
                           struct haptic_effect *effect,
                           SDL_HapticEffect * data)
{
    SDL_assert(data->type == SDL_HAPTIC_LEFTRIGHT);
    SDL_LockMutex(haptic->hwdata->mutex);
    if (haptic->hwdata->stopTicks) {  /* running right now? Update it. */
        NpadDoVibration(haptic, data->leftright);
    }
    SDL_UnlockMutex(haptic->hwdata->mutex);
    return 0;
}


int
SDL_SYS_HapticRunEffect(SDL_Haptic * haptic, struct haptic_effect *effect,
                        Uint32 iterations)
{
    SDL_assert(effect->effect.type == SDL_HAPTIC_LEFTRIGHT);  /* should catch this at higher level */
    SDL_LockMutex(haptic->hwdata->mutex);
    if (effect->effect.leftright.length == SDL_HAPTIC_INFINITY || iterations == SDL_HAPTIC_INFINITY) {
        haptic->hwdata->stopTicks = SDL_HAPTIC_INFINITY;
    } else if ((!effect->effect.leftright.length) || (!iterations)) {
        /* do nothing. Effect runs for zero milliseconds. */
    } else {
        haptic->hwdata->stopTicks = SDL_GetTicks() + (effect->effect.leftright.length * iterations);
        if ((haptic->hwdata->stopTicks == SDL_HAPTIC_INFINITY) || (haptic->hwdata->stopTicks == 0)) {
            haptic->hwdata->stopTicks = 1;  /* fix edge cases. */
        }
    }
    SDL_UnlockMutex(haptic->hwdata->mutex);

    NpadDoVibration(haptic, effect->effect.leftright);

    return 0; 
}


int
SDL_SYS_HapticStopEffect(SDL_Haptic * haptic, struct haptic_effect *effect)
{
    return SDL_SYS_HapticStopAll(haptic);
}


void
SDL_SYS_HapticDestroyEffect(SDL_Haptic * haptic, struct haptic_effect *effect)
{
    SDL_SYS_HapticStopEffect(haptic, effect);
    SDL_free(effect->hweffect);
    effect->hweffect = NULL;
}


int
SDL_SYS_HapticGetEffectStatus(SDL_Haptic * haptic,
                              struct haptic_effect *effect)
{
    return SDL_Unsupported();
}


int
SDL_SYS_HapticSetGain(SDL_Haptic * haptic, int gain)
{
    return SDL_Unsupported();
}

int
SDL_SYS_HapticSetAutocenter(SDL_Haptic * haptic, int autocenter)
{
    return SDL_Unsupported();
}

int
SDL_SYS_HapticPause(SDL_Haptic * haptic)
{
    return SDL_Unsupported();
}

int
SDL_SYS_HapticUnpause(SDL_Haptic * haptic)
{
    return SDL_Unsupported();
}

int
SDL_SYS_HapticStopAll(SDL_Haptic * haptic)
{
    SDL_LockMutex(haptic->hwdata->mutex);
    haptic->hwdata->stopTicks = 0;
    SDL_UnlockMutex(haptic->hwdata->mutex);
    const nn::hid::VibrationValue stop_vibrating = nn::hid::VibrationValue::Make();
    for (int i = 0; i < haptic->hwdata->num_vibration_devices; i++) {
        nn::hid::SendVibrationValue(haptic->hwdata->vibration_devices[i], stop_vibrating);
    }
    return 0;
}

#endif /* SDL_HAPTIC_NINTENDO_SWITCH */

/* vi: set ts=4 sw=4 expandtab: */
