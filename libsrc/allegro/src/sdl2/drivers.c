
#include "allegro.h"
#include "allegro/internal/aintern.h"

#ifndef ALLEGRO_SDL2
#error Something is wrong with the makefile
#endif


_DRIVER_INFO _keyboard_driver_list[] =
{
   { 0,                       NULL,                     0     }
};


_DRIVER_INFO _timer_driver_list[] =
{
   { TIMER_SDL2,  &timer_sdl2,  TRUE  },
   { 0,                       NULL,                     0     }
};


_DRIVER_INFO _mouse_driver_list[] =
{
   { 0,                       NULL,                     0     }
};


BEGIN_GFX_DRIVER_LIST
{   GFX_SDL2_WINDOW,        &gfx_sdl2_window,         TRUE  },
{   GFX_SDL2_FULLSCREEN,    &gfx_sdl2_fullscreen,         TRUE  },
END_GFX_DRIVER_LIST


BEGIN_DIGI_DRIVER_LIST
END_DIGI_DRIVER_LIST


BEGIN_MIDI_DRIVER_LIST
END_MIDI_DRIVER_LIST


BEGIN_JOYSTICK_DRIVER_LIST
END_JOYSTICK_DRIVER_LIST
