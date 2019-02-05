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

#include "ac/timer.h"
#include "util/wgt2allg.h"

unsigned int loopcounter=0,lastcounter=0;

uint64_t currentFrameTicks = 0;

static uint64_t cachedPerformanceFrequency = 1000;
static int startFps = 40;
static uint64_t startTicksOffset = 0;
static uint64_t startPlatformOffset = 0;

void setTimerFps(int fps) {
    cachedPerformanceFrequency = SDL_GetPerformanceFrequency();  // should only change between system reboots
    SDL_assert(cachedPerformanceFrequency > 0);
    startTicksOffset = getAgsTicks();
    startPlatformOffset = SDL_GetPerformanceCounter();
    startFps = 40;
}

uint32_t getGlobalTimerCounterMs() {
    return SDL_GetTicks();
}

uint64_t getAgsTicks() {
    return startTicksOffset + (SDL_GetPerformanceCounter() - startPlatformOffset) * startFps / cachedPerformanceFrequency;
}
