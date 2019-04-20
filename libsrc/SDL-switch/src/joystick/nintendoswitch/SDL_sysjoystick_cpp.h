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

#ifndef __cplusplus
#error this header is C++ only
#endif

/* we roll a simple enum so we can use this in switch statements,
   which you can't do with the NintendoSDK bitsets. */
enum NpadStyle
{
    NPAD_STYLE_DISCONNECTED,
    NPAD_STYLE_HANDHELD,
    NPAD_STYLE_JOY_DUAL,
    NPAD_STYLE_JOY_RIGHT,
    NPAD_STYLE_JOY_LEFT,
    NPAD_STYLE_FULL_KEY
};

struct NpadDevice
{
    nn::hid::NpadIdType npad_id;
    SDL_JoystickID device_instance;
    NpadStyle previous_style;
};

/* The private structure used to keep track of a joystick */
struct joystick_hwdata
{
    const NpadDevice *npad_device;
    nn::hid::NpadButtonSet previous_buttons;
    nn::hid::AnalogStickState previous_analog_stick_left;
    nn::hid::AnalogStickState previous_analog_stick_right;
    int64_t last_npad_sample;
    SDL_Haptic *haptic;
    int haptic_effect;
};

/* vi: set ts=4 sw=4 expandtab: */
