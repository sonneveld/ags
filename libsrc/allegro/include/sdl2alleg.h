#ifndef SDL2_ALLEGRO_H
#define SDL2_ALLEGRO_H

#ifndef ALLEGRO_H
   #error Please include allegro.h before sdl2alleg.h!
#endif

/* Allegro's SDL2 specific interface */
#ifdef __cplusplus
   extern "C" {
#endif

#include "SDL.h"

AL_FUNC(void, sdl2_process_single_event, (SDL_Event *event));
AL_FUNC(void, sdl2_process_all_pending_events, (void));
AL_FUNC(void, sdl2_present_screen, (void));
AL_FUNC(void, sdl2_toggle_fullscreen, (void));
AL_FUNC(void, sdl2_mouse_position, (int x, int y));

#ifdef __cplusplus
   }
#endif

#endif          /* ifndef SDL2_ALLEGRO_H */
