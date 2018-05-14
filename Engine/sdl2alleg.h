#ifndef SDL2_ALLEGRO_H
#define SDL2_ALLEGRO_H

#ifndef ALLEGRO_H
   #error Please include allegro.h before sdl2alleg.h!
#endif

/* Allegro's SDL2 specific interface */
#ifdef __cplusplus
   extern "C" {
#endif

AL_FUNC(void, process_sdl2_events, (void));
AL_FUNC(void, sdl2_present_screen, (void));

#ifdef __cplusplus
   }
#endif

#endif          /* ifndef SDL2_ALLEGRO_H */
