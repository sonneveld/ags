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
#ifndef __AGS_EE_AC__SYS_EVENTS_H
#define __AGS_EE_AC__SYS_EVENTS_H

void process_pending_events();

void ags_clear_input_buffer(); // Clears buffered keypresses and mouse clicks, if any

int  ags_mgetbutton();
void ags_domouse (int what);
int  ags_check_mouse_wheel ();

SDL_Event getTextEventFromQueue();
int  ags_iskeypressed (int keycode);

int asciiFromEvent(SDL_Event event);
int agsKeyCodeFromEvent(SDL_Event event);
int asciiOrAgsKeyCodeFromEvent(SDL_Event event);

#define ASCII_BACKSPACE (8)
#define ASCII_TAB (9)
#define ASCII_RETURN (13)
#define ASCII_ESCAPE (27)

#endif // __AGS_EE_AC__SYS_EVENTS_H
