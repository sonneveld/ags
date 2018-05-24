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
//
//
//
//=============================================================================
#ifndef __AGS_EE_AC__RECORD_H
#define __AGS_EE_AC__RECORD_H

#include "SDL.h"

// If this is defined for record unit it will cause endless recursion!
#ifndef IS_RECORD_UNIT
#undef kbhit
#define mgetbutton rec_mgetbutton
#define misbuttondown rec_misbuttondown
#define kbhit rec_kbhit
#define getch rec_getch
#define iskeypressed rec_iskeypressed
#define isSpeechFinished rec_isSpeechFinished
#endif

int  rec_getch ();
int  rec_kbhit ();
int  rec_iskeypressed (int keycode);
int  rec_isSpeechFinished ();
int  rec_misbuttondown (int but);
int  rec_mgetbutton();
int  check_mouse_wheel ();
int  my_readkey();
void clear_input_buffer(); // Clears buffered keypresses and mouse clicks, if any

SDL_Event getTextEventFromQueue();

int asciiFromEvent(SDL_Event event);
int agsKeyCodeFromEvent(SDL_Event event);
int asciiOrAgsKeyCodeFromEvent(SDL_Event event);

#define ASCII_BACKSPACE (8)
#define ASCII_TAB (9)
#define ASCII_RETURN (13)
#define ASCII_ESCAPE (27)

void process_pending_events();

#endif // __AGS_EE_AC__RECORD_H
