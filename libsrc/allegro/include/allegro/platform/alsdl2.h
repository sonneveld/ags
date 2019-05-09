#ifndef ALSDL2_H
#define ALSDL2_H

#ifndef ALLEGRO_SDL2
   #error bad include
#endif

/* System driver */
#define SYSTEM_SDL2            AL_ID('S','D','L',' ')
AL_VAR(SYSTEM_DRIVER, system_sdl2);

/* Timer driver */
#define TIMER_SDL2               AL_ID('S','D','L','T')
AL_VAR(TIMER_DRIVER, timer_sdl2);

/* Mouse drivers */
#define MOUSE_SDL2               AL_ID('S','D','L','M')
AL_VAR(MOUSE_DRIVER, mouse_sdl2);

/* Gfx driver */
#define GFX_SDL2_WINDOW                     AL_ID('S','D','L','W')
#define GFX_SDL2_FULLSCREEN                 AL_ID('S','D','L','F')
AL_VAR(GFX_DRIVER, gfx_sdl2_window);
AL_VAR(GFX_DRIVER, gfx_sdl2_fullscreen);

/* Digital sound driver */
#define DIGI_SDL2                AL_ID('S','D','L','S')
AL_VAR(DIGI_DRIVER, digi_sdl2);

/* Joystick drivers */
#define JOYSTICK_SDL2            AL_ID('S','D','L','J')
AL_VAR(JOYSTICK_DRIVER, joystick_sdl2);

#endif
