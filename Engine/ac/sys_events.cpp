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

#include "core/platform.h"
#include <queue>
#include "ac/common.h"
#include "ac/gamesetupstruct.h"
#include "ac/gamestate.h"
#include "ac/keycode.h"
#include "ac/mouse.h"
#include "ac/sys_events.h"
#include "device/mousew32.h"
#include "platform/base/agsplatformdriver.h"
#include "ac/timer.h"

using namespace AGS::Common;
using namespace AGS::Engine;

extern GameSetupStruct game;
extern GameState play;

extern int pluginSimulatedClick;
extern int displayed_room;
extern char check_dynamic_sprites_at_exit;
extern volatile char want_exit;

extern void domouse(int str);
extern int mgetbutton();
extern int misbuttondown(int buno);

int mouse_z_was = 0;

static std::queue<SDL_Event> textEventQueue;

SDL_Event getTextEventFromQueue() {
    SDL_Event result = { 0 };

    if (!textEventQueue.empty()) {
        result = textEventQueue.front();
        textEventQueue.pop();
    }
    
    if ((AGS_Clock::now() < play.ignore_user_input_until_time)) {
        // ignoring user input
        result =  { 0 };
    }
    
    return result;
}

static void onkeydown(SDL_Event *event) {
#pragma message ("SDL-TODO: if text_input, we should emit multiple unicode codepoints. check out ugetx")
    textEventQueue.push(*event);
    
#pragma message ("SDL-TODO: this might break since running in a different thread.  maybe set a quit flag.")
    // Alt+X, abort (but only once game is loaded)
    int gott = agsKeyCodeFromEvent(*event);
    if ((gott == play.abort_key) && (displayed_room >= 0)) {
        check_dynamic_sprites_at_exit = 0;
        want_exit = 1;
//        quit("!|");
    }
}


volatile int mouse_x = 0;
volatile int mouse_y = 0;
volatile int mouse_z = 0;

static void on_sdl_mouse_motion(SDL_MouseMotionEvent *event) {
   mouse_x = event->x;
   mouse_y = event->y;
}

static int sdl_button_to_allegro_bit(int button)
{
   switch (button) {
      case SDL_BUTTON_LEFT: return 0x1;
      case SDL_BUTTON_RIGHT: return 0x2;
      case SDL_BUTTON_MIDDLE: return 0x4;
      case SDL_BUTTON_X1: return 0x8;
      case SDL_BUTTON_X2: return 0x10;
   }
   return 0x0;
}

/*
 Button tracking:
 On OSX, some tap to click up/down events happen too quickly to be detected on the polled mouse_b global variable.
 Instead we accumulate button presses over a couple of timer loops.
 */

static int _button_state = 0;
static int _accumulated_button_state = 0;
static auto _clear_at_global_timer_counter = AGS_Clock::now();

static void on_sdl_mouse_button(SDL_MouseButtonEvent *event) 
{
   mouse_x = event->x;
   mouse_y = event->y;

   if (event->type == SDL_MOUSEBUTTONDOWN) {
        _button_state |= sdl_button_to_allegro_bit(event->button);
        _accumulated_button_state |= sdl_button_to_allegro_bit(event->button);
   } else {
        _button_state &= ~sdl_button_to_allegro_bit(event->button);
   }
}

static void on_sdl_mouse_wheel(SDL_MouseWheelEvent *event) 
{
   mouse_z += event->y;
}

extern void engine_on_window_size_changed();

void process_pending_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {

        switch (event.type) {
            
            case SDL_WINDOWEVENT: {
                
                switch(event.window.event) {
                    // case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        engine_on_window_size_changed();
                        break;
                }
                break;
            }

            case SDL_KEYDOWN:
            case SDL_TEXTINPUT:
                onkeydown(&event);
                break;
            case SDL_KEYUP:
                break;
            case SDL_MOUSEMOTION:
                on_sdl_mouse_motion(&event.motion);
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                on_sdl_mouse_button(&event.button);
                break;
            case SDL_MOUSEWHEEL:
                on_sdl_mouse_wheel(&event.wheel);
                break;
            case SDL_QUIT:
                break;
        }

        sdl2_process_single_event(&event);
    }
}

#pragma message ("SDL-TODO: still used in invwindow.  need to check where kbhit was used.. why was ignored?")
#if 0
int ags_kbhit () {
    int result = keypressed();
    if ((result) && (AGS_Clock::now() < play.ignore_user_input_until_time))
    {
        // ignoring user input
        ags_getch();
        result = 0;
    }
    return result;  
}

int ags_iskeypressed (int keycode) {
    if (keycode >= 0 && keycode < __allegro_KEY_MAX)
    {
        return key[keycode];
    }
    return 0;
}
#endif

int get_mouse_b()
{
    auto now = AGS_Clock::now();
    int result = _button_state | _accumulated_button_state;
    if (now >= _clear_at_global_timer_counter) {
        _accumulated_button_state = 0;
        _clear_at_global_timer_counter = now + std::chrono::milliseconds(50);
    }
    return result;
}

void set_mouse_range(int x1, int y_1, int x2, int y2) {
    // TODO
}

void position_mouse(int x, int y) {
    sdl2_mouse_position(x, y);
}



// this is eKeyCode defined in agsdefns.sh
int ags_iskeypressed (int keycode) {
    SDL_PumpEvents();
    const Uint8 *state = SDL_GetKeyboardState(NULL);

    // ascii lookup
    switch (keycode) {
        case 'A': return state[SDL_GetScancodeFromKey(SDLK_a)];
        case 'B': return state[SDL_GetScancodeFromKey(SDLK_b)];
        case 'C': return state[SDL_GetScancodeFromKey(SDLK_c)];
        case 'D': return state[SDL_GetScancodeFromKey(SDLK_d)];
        case 'E': return state[SDL_GetScancodeFromKey(SDLK_e)];
        case 'F': return state[SDL_GetScancodeFromKey(SDLK_f)];
        case 'G': return state[SDL_GetScancodeFromKey(SDLK_g)];
        case 'H': return state[SDL_GetScancodeFromKey(SDLK_h)];
        case 'I': return state[SDL_GetScancodeFromKey(SDLK_i)];
        case 'J': return state[SDL_GetScancodeFromKey(SDLK_j)];
        case 'K': return state[SDL_GetScancodeFromKey(SDLK_k)];
        case 'L': return state[SDL_GetScancodeFromKey(SDLK_l)];
        case 'M': return state[SDL_GetScancodeFromKey(SDLK_m)];
        case 'N': return state[SDL_GetScancodeFromKey(SDLK_n)];
        case 'O': return state[SDL_GetScancodeFromKey(SDLK_o)];
        case 'P': return state[SDL_GetScancodeFromKey(SDLK_p)];
        case 'Q': return state[SDL_GetScancodeFromKey(SDLK_q)];
        case 'R': return state[SDL_GetScancodeFromKey(SDLK_r)];
        case 'S': return state[SDL_GetScancodeFromKey(SDLK_s)];
        case 'T': return state[SDL_GetScancodeFromKey(SDLK_t)];
        case 'U': return state[SDL_GetScancodeFromKey(SDLK_u)];
        case 'V': return state[SDL_GetScancodeFromKey(SDLK_v)];
        case 'W': return state[SDL_GetScancodeFromKey(SDLK_w)];
        case 'X': return state[SDL_GetScancodeFromKey(SDLK_x)];
        case 'Y': return state[SDL_GetScancodeFromKey(SDLK_y)];
        case 'Z': return state[SDL_GetScancodeFromKey(SDLK_z)];

        case '0': return state[SDL_SCANCODE_0];
        case '1': return state[SDL_SCANCODE_1];
        case '2': return state[SDL_SCANCODE_2];
        case '3': return state[SDL_SCANCODE_3];
        case '4': return state[SDL_SCANCODE_4];
        case '5': return state[SDL_SCANCODE_5];
        case '6': return state[SDL_SCANCODE_6];
        case '7': return state[SDL_SCANCODE_7];
        case '8': return state[SDL_SCANCODE_8];
        case '9': return state[SDL_SCANCODE_9];

        case 8: return state[SDL_SCANCODE_BACKSPACE] || state[SDL_SCANCODE_KP_BACKSPACE];
        case 9: return state[SDL_SCANCODE_TAB] || state[SDL_SCANCODE_KP_TAB];

        // check both the main return key and the numeric pad enter
        case 13: return state[SDL_SCANCODE_RETURN] || state[SDL_SCANCODE_RETURN2] || state[SDL_SCANCODE_KP_ENTER];
        
        case ' ': return state[SDL_SCANCODE_SPACE];

        case 27: return state[SDL_SCANCODE_ESCAPE];

        // check both the main - key and the numeric pad
        case '-': return state[SDL_SCANCODE_MINUS] || state[SDL_SCANCODE_KP_MINUS];
        
        // check both the main + key and the numeric pad
        case '+': return state[SDL_SCANCODE_EQUALS] || state[SDL_SCANCODE_KP_PLUS];
        
        // check both the main / key and the numeric pad
        case '/': return state[SDL_SCANCODE_SLASH] || state[SDL_SCANCODE_KP_DIVIDE];

        case '=': return state[SDL_SCANCODE_EQUALS] || state[SDL_SCANCODE_KP_EQUALS];
        case '[': return state[SDL_SCANCODE_LEFTBRACKET];
        case ']': return state[SDL_SCANCODE_RIGHTBRACKET];
        case '\\': return state[SDL_SCANCODE_BACKSLASH];
        case ';': return state[SDL_SCANCODE_SEMICOLON];
        case '\'': return state[SDL_SCANCODE_APOSTROPHE];
        case ',': return state[SDL_SCANCODE_COMMA] || state[SDL_SCANCODE_KP_COMMA];
        case '.': return state[SDL_SCANCODE_PERIOD] || state[SDL_SCANCODE_KP_PERIOD];
    }
    
    // made up numbers, not based on allegro3 keycodes
    switch (keycode) {
        // deal with shift/ctrl/alt
        case 403: return state[SDL_SCANCODE_LSHIFT];
        case 404: return state[SDL_SCANCODE_RSHIFT];
        case 405: return state[SDL_SCANCODE_LCTRL];
        case 406: return state[SDL_SCANCODE_RCTRL];
        case 407: return state[SDL_SCANCODE_LALT] || state[SDL_SCANCODE_RALT];
        // F11-F12
        case 433: return state[SDL_SCANCODE_F11];
        case 434: return state[SDL_SCANCODE_F12];
    }
    
    // AGS_EXT_KEY_SHIFT is >= 300 and sort of maps to original allegro3 keycodes (+300)
    
    if (keycode >= AGS_EXT_KEY_SHIFT) {
        // allegro 3 codes
        switch (keycode-AGS_EXT_KEY_SHIFT) {
            case 30 /*KEY_A*/: return state[SDL_SCANCODE_A];
            case 48 /*KEY_B*/: return state[SDL_SCANCODE_B];
            case 46 /*KEY_C*/: return state[SDL_SCANCODE_C];
            case 32 /*KEY_D*/: return state[SDL_SCANCODE_D];
            case 18 /*KEY_E*/: return state[SDL_SCANCODE_E];
            case 33 /*KEY_F*/: return state[SDL_SCANCODE_F];
            case 34 /*KEY_G*/: return state[SDL_SCANCODE_G];
            case 35 /*KEY_H*/: return state[SDL_SCANCODE_H];
            case 23 /*KEY_I*/: return state[SDL_SCANCODE_I];
            case 36 /*KEY_J*/: return state[SDL_SCANCODE_J];
            case 37 /*KEY_K*/: return state[SDL_SCANCODE_K];
            case 38 /*KEY_L*/: return state[SDL_SCANCODE_L];
            case 50 /*KEY_M*/: return state[SDL_SCANCODE_M];
            case 49 /*KEY_N*/: return state[SDL_SCANCODE_N];
            case 24 /*KEY_O*/: return state[SDL_SCANCODE_O];
            case 25 /*KEY_P*/: return state[SDL_SCANCODE_P];
            case 16 /*KEY_Q*/: return state[SDL_SCANCODE_Q];
            case 19 /*KEY_R*/: return state[SDL_SCANCODE_R];
            case 31 /*KEY_S*/: return state[SDL_SCANCODE_S];
            case 20 /*KEY_T*/: return state[SDL_SCANCODE_T];
            case 22 /*KEY_U*/: return state[SDL_SCANCODE_U];
            case 47 /*KEY_V*/: return state[SDL_SCANCODE_V];
            case 17 /*KEY_W*/: return state[SDL_SCANCODE_W];
            case 45 /*KEY_X*/: return state[SDL_SCANCODE_X];
            case 21 /*KEY_Y*/: return state[SDL_SCANCODE_Y];
            case 44 /*KEY_Z*/: return state[SDL_SCANCODE_Z];
                
            case 2 /*KEY_1*/: return state[SDL_SCANCODE_1];
            case 3 /*KEY_2*/: return state[SDL_SCANCODE_2];
            case 4 /*KEY_3*/: return state[SDL_SCANCODE_3];
            case 5 /*KEY_4*/: return state[SDL_SCANCODE_4];
            case 6 /*KEY_5*/: return state[SDL_SCANCODE_5];
            case 7 /*KEY_6*/: return state[SDL_SCANCODE_6];
            case 8 /*KEY_7*/: return state[SDL_SCANCODE_7];
            case 9 /*KEY_8*/: return state[SDL_SCANCODE_8];
            case 10 /*KEY_9*/: return state[SDL_SCANCODE_9];
            case 11 /*KEY_0*/: return state[SDL_SCANCODE_0];
                
            case 59 /*KEY_F1*/: return state[SDL_SCANCODE_F1];
            case 60 /*KEY_F2*/: return state[SDL_SCANCODE_F2];
            case 61 /*KEY_F3*/: return state[SDL_SCANCODE_F3];
            case 62 /*KEY_F4*/: return state[SDL_SCANCODE_F4];
            case 63 /*KEY_F5*/: return state[SDL_SCANCODE_F5];
            case 64 /*KEY_F6*/: return state[SDL_SCANCODE_F6];
            case 65 /*KEY_F7*/: return state[SDL_SCANCODE_F7];
            case 66 /*KEY_F8*/: return state[SDL_SCANCODE_F8];
            case 67 /*KEY_F9*/: return state[SDL_SCANCODE_F9];
            case 68 /*KEY_F10*/: return state[SDL_SCANCODE_F10];
            case 87 /*KEY_F11*/: return state[SDL_SCANCODE_F11];
            case 88 /*KEY_F12*/: return state[SDL_SCANCODE_F12];
                
            case 1 /*KEY_ESC*/: return state[SDL_SCANCODE_ESCAPE];
            case 12 /*KEY_MINUS*/: return state[SDL_SCANCODE_MINUS];
            case 13 /*KEY_EQUALS*/: return state[SDL_SCANCODE_EQUALS];
            case 14 /*KEY_BACKSPACE*/: return state[SDL_SCANCODE_BACKSPACE];
            case 15 /*KEY_TAB*/: return state[SDL_SCANCODE_TAB];
            case 26 /*KEY_OPENBRACE*/: return state[SDL_SCANCODE_LEFTBRACKET];
            case 27 /*KEY_CLOSEBRACE*/: return state[SDL_SCANCODE_RIGHTBRACKET];
            case 28 /*KEY_ENTER*/: return state[SDL_SCANCODE_RETURN] || state[SDL_SCANCODE_RETURN2] || state[SDL_SCANCODE_KP_ENTER];
            case 29 /*KEY_CONTROL*/: return state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL];
            case 39 /*KEY_COLON*/: return state[SDL_SCANCODE_SEMICOLON];
            case 40 /*KEY_QUOTE*/: return state[SDL_SCANCODE_APOSTROPHE];
            case 41 /*KEY_TILDE*/: return state[SDL_SCANCODE_GRAVE];
            case 42 /*KEY_LSHIFT*/: return state[SDL_SCANCODE_LSHIFT];
            case 43 /*KEY_BACKSLASH*/: return state[SDL_SCANCODE_BACKSLASH];
            case 51 /*KEY_COMMA*/: return state[SDL_SCANCODE_COMMA];
            case 52 /*KEY_STOP*/: return state[SDL_SCANCODE_PERIOD];
            case 53 /*KEY_SLASH*/: return state[SDL_SCANCODE_SLASH];
            case 54 /*KEY_RSHIFT*/: return state[SDL_SCANCODE_RSHIFT];
            case 55 /*KEY_ASTERISK*/: return state[SDL_SCANCODE_KP_MULTIPLY];
            case 56 /*KEY_ALT*/: return state[SDL_SCANCODE_LALT] || state[SDL_SCANCODE_RALT];
            case 57 /*KEY_SPACE*/: return state[SDL_SCANCODE_SPACE];
                
            case 71 /*KEY_HOME*/: return state[SDL_SCANCODE_HOME] || state[SDL_SCANCODE_KP_7];
            case 72 /*KEY_UP*/: return state[SDL_SCANCODE_UP] || state[SDL_SCANCODE_KP_8];
            case 73 /*KEY_PGUP*/: return state[SDL_SCANCODE_PAGEUP] || state[SDL_SCANCODE_KP_9];
            case 74 /*KEY_MINUS_PAD*/: return state[SDL_SCANCODE_KP_MINUS];
            case 75 /*KEY_LEFT*/: return state[SDL_SCANCODE_LEFT] || state[SDL_SCANCODE_KP_4];
            case 76 /*KEY_5_PAD*/: return state[SDL_SCANCODE_KP_5];
            case 77 /*KEY_RIGHT*/: return state[SDL_SCANCODE_RIGHT] || state[SDL_SCANCODE_KP_6];
            case 78 /*KEY_PLUS_PAD*/: return state[SDL_SCANCODE_KP_PLUS];
            case 79 /*KEY_END*/: return state[SDL_SCANCODE_END] || state[SDL_SCANCODE_KP_1];
            case 80 /*KEY_DOWN*/: return state[SDL_SCANCODE_DOWN] || state[SDL_SCANCODE_KP_2];
            case 81 /*KEY_PGDN*/: return state[SDL_SCANCODE_PAGEDOWN] || state[SDL_SCANCODE_KP_3];
            case 82 /*KEY_INSERT*/: return state[SDL_SCANCODE_INSERT] || state[SDL_SCANCODE_KP_0];
            case 83 /*KEY_DEL*/: return state[SDL_SCANCODE_DELETE] || state[SDL_SCANCODE_KP_PERIOD];
                
            case 120 /*KEY_RCONTROL*/: return state[SDL_SCANCODE_RCTRL];
        }

#if 0
        // allegro 4 codes (last resort)
        switch (keycode-AGS_EXT_KEY_SHIFT) {
            case 1 /*KEY_A*/: return state[SDL_SCANCODE_A];
            case 2 /*KEY_B*/: return state[SDL_SCANCODE_B];
            case 3 /*KEY_C*/: return state[SDL_SCANCODE_C];
            case 4 /*KEY_D*/: return state[SDL_SCANCODE_D];
            case 5 /*KEY_E*/: return state[SDL_SCANCODE_E];
            case 6 /*KEY_F*/: return state[SDL_SCANCODE_F];
            case 7 /*KEY_G*/: return state[SDL_SCANCODE_G];
            case 8 /*KEY_H*/: return state[SDL_SCANCODE_H];
            case 9 /*KEY_I*/: return state[SDL_SCANCODE_I];
            case 10 /*KEY_J*/: return state[SDL_SCANCODE_J];
            case 11 /*KEY_K*/: return state[SDL_SCANCODE_K];
            case 12 /*KEY_L*/: return state[SDL_SCANCODE_L];
            case 13 /*KEY_M*/: return state[SDL_SCANCODE_M];
            case 14 /*KEY_N*/: return state[SDL_SCANCODE_N];
            case 15 /*KEY_O*/: return state[SDL_SCANCODE_O];
            case 16 /*KEY_P*/: return state[SDL_SCANCODE_P];
            case 17 /*KEY_Q*/: return state[SDL_SCANCODE_Q];
            case 18 /*KEY_R*/: return state[SDL_SCANCODE_R];
            case 19 /*KEY_S*/: return state[SDL_SCANCODE_S];
            case 20 /*KEY_T*/: return state[SDL_SCANCODE_T];
            case 21 /*KEY_U*/: return state[SDL_SCANCODE_U];
            case 22 /*KEY_V*/: return state[SDL_SCANCODE_V];
            case 23 /*KEY_W*/: return state[SDL_SCANCODE_W];
            case 24 /*KEY_X*/: return state[SDL_SCANCODE_X];
            case 25 /*KEY_Y*/: return state[SDL_SCANCODE_Y];
            case 26 /*KEY_Z*/: return state[SDL_SCANCODE_Z];
                
            case 27 /*KEY_0*/: return state[SDL_SCANCODE_0];
            case 28 /*KEY_1*/: return state[SDL_SCANCODE_1];
            case 29 /*KEY_2*/: return state[SDL_SCANCODE_2];
            case 30 /*KEY_3*/: return state[SDL_SCANCODE_3];
            case 31 /*KEY_4*/: return state[SDL_SCANCODE_4];
            case 32 /*KEY_5*/: return state[SDL_SCANCODE_5];
            case 33 /*KEY_6*/: return state[SDL_SCANCODE_6];
            case 34 /*KEY_7*/: return state[SDL_SCANCODE_7];
            case 35 /*KEY_8*/: return state[SDL_SCANCODE_8];
            case 36 /*KEY_9*/: return state[SDL_SCANCODE_9];
            case 37 /*KEY_0_PAD*/: return state[SDL_SCANCODE_KP_0];
            case 38 /*KEY_1_PAD*/: return state[SDL_SCANCODE_KP_1];
            case 39 /*KEY_2_PAD*/: return state[SDL_SCANCODE_KP_2];
            case 40 /*KEY_3_PAD*/: return state[SDL_SCANCODE_KP_3];
            case 41 /*KEY_4_PAD*/: return state[SDL_SCANCODE_KP_4];
            case 42 /*KEY_5_PAD*/: return state[SDL_SCANCODE_KP_5];
            case 43 /*KEY_6_PAD*/: return state[SDL_SCANCODE_KP_6];
            case 44 /*KEY_7_PAD*/: return state[SDL_SCANCODE_KP_7];
            case 45 /*KEY_8_PAD*/: return state[SDL_SCANCODE_KP_8];
            case 46 /*KEY_9_PAD*/: return state[SDL_SCANCODE_KP_9];
                
            case 47 /*KEY_F1*/: return state[SDL_SCANCODE_F1];
            case 48 /*KEY_F2*/: return state[SDL_SCANCODE_F2];
            case 49 /*KEY_F3*/: return state[SDL_SCANCODE_F3];
            case 50 /*KEY_F4*/: return state[SDL_SCANCODE_F4];
            case 51 /*KEY_F5*/: return state[SDL_SCANCODE_F5];
            case 52 /*KEY_F6*/: return state[SDL_SCANCODE_F6];
            case 53 /*KEY_F7*/: return state[SDL_SCANCODE_F7];
            case 54 /*KEY_F8*/: return state[SDL_SCANCODE_F8];
            case 55 /*KEY_F9*/: return state[SDL_SCANCODE_F9];
            case 56 /*KEY_F10*/: return state[SDL_SCANCODE_F10];
            case 57 /*KEY_F11*/: return state[SDL_SCANCODE_F11];
            case 58 /*KEY_F12*/: return state[SDL_SCANCODE_F12];
                
            case 59 /*KEY_ESC*/: return state[SDL_SCANCODE_ESCAPE];
            case 60 /*KEY_TILDE*/: return state[SDL_SCANCODE_GRAVE];
            case 61 /*KEY_MINUS*/: return state[SDL_SCANCODE_MINUS];
            case 62 /*KEY_EQUALS*/: return state[SDL_SCANCODE_EQUALS];
            case 63 /*KEY_BACKSPACE*/: return state[SDL_SCANCODE_BACKSPACE];
            case 64 /*KEY_TAB*/: return state[SDL_SCANCODE_TAB];
            case 65 /*KEY_OPENBRACE*/: return state[SDL_SCANCODE_LEFTBRACKET];
            case 66 /*KEY_CLOSEBRACE*/: return state[SDL_SCANCODE_RIGHTBRACKET];
            case 67 /*KEY_ENTER*/: return state[SDL_SCANCODE_RETURN];
            case 68 /*KEY_COLON*/: return state[SDL_SCANCODE_SEMICOLON];
            case 69 /*KEY_QUOTE*/: return state[SDL_SCANCODE_APOSTROPHE];
            case 70 /*KEY_BACKSLASH*/: return state[SDL_SCANCODE_BACKSLASH];
            case 72 /*KEY_COMMA*/: return state[SDL_SCANCODE_COMMA];
            case 73 /*KEY_STOP*/: return state[SDL_SCANCODE_PERIOD];
            case 74 /*KEY_SLASH*/: return state[SDL_SCANCODE_SLASH];
            case 75 /*KEY_SPACE*/: return state[SDL_SCANCODE_SPACE];
            case 76 /*KEY_INSERT*/: return state[SDL_SCANCODE_INSERT];
            case 77 /*KEY_DEL*/: return state[SDL_SCANCODE_DELETE];
            case 78 /*KEY_HOME*/: return state[SDL_SCANCODE_HOME];
            case 79 /*KEY_END*/: return state[SDL_SCANCODE_END];
            case 80 /*KEY_PGUP*/: return state[SDL_SCANCODE_PAGEUP];
            case 81 /*KEY_PGDN*/: return state[SDL_SCANCODE_PAGEDOWN];
            case 82 /*KEY_LEFT*/: return state[SDL_SCANCODE_LEFT];
            case 83 /*KEY_RIGHT*/: return state[SDL_SCANCODE_RIGHT];
            case 84 /*KEY_UP*/: return state[SDL_SCANCODE_UP];
            case 85 /*KEY_DOWN*/: return state[SDL_SCANCODE_DOWN];
            case 86 /*KEY_SLASH_PAD*/: return state[SDL_SCANCODE_KP_DIVIDE];
            case 87 /*KEY_ASTERISK*/: return state[SDL_SCANCODE_KP_MULTIPLY];
            case 88 /*KEY_MINUS_PAD*/: return state[SDL_SCANCODE_KP_MINUS];
            case 89 /*KEY_PLUS_PAD*/: return state[SDL_SCANCODE_KP_PLUS];
            case 90 /*KEY_DEL_PAD*/: return state[SDL_SCANCODE_KP_BACKSPACE];
            case 91 /*KEY_ENTER_PAD*/: return state[SDL_SCANCODE_KP_ENTER];
            case 101 /*KEY_COLON2*/: return state[SDL_SCANCODE_KP_COLON];
            case 103 /*KEY_EQUALS_PAD*/: return state[SDL_SCANCODE_KP_EQUALS];
            case 104 /*KEY_BACKQUOTE*/: return state[SDL_SCANCODE_GRAVE];
            case 105 /*KEY_SEMICOLON*/: return state[SDL_SCANCODE_SEMICOLON];
            case 115 /*KEY_LSHIFT*/: return state[SDL_SCANCODE_LSHIFT];
            case 116 /*KEY_RSHIFT*/: return state[SDL_SCANCODE_RSHIFT];
            case 117 /*KEY_LCONTROL*/: return state[SDL_SCANCODE_LCTRL];
            case 118 /*KEY_RCONTROL*/: return state[SDL_SCANCODE_RCTRL];
            case 119 /*KEY_ALT*/: return state[SDL_SCANCODE_LALT] || state[SDL_SCANCODE_RALT];
        }
#endif 
    }
    return -1;
}

int ags_mgetbutton() {
    int result;

    if (pluginSimulatedClick > NONE) {
        result = pluginSimulatedClick;
        pluginSimulatedClick = NONE;
    }
    else {
        result = mgetbutton();
    }

    if ((result >= 0) && (AGS_Clock::now() < play.ignore_user_input_until_time))
    {
        // ignoring user input
        result = NONE;
    }

    return result;
}

void ags_domouse (int what) {
    // do mouse is "update the mouse x,y and also the cursor position", unless DOMOUSE_NOCURSOR is set.
    if (what == DOMOUSE_NOCURSOR)
        mgetgraphpos();
    else
        domouse(what);
}

int ags_check_mouse_wheel () {
    if (game.options[OPT_MOUSEWHEEL] == 0) { return 0; }
    if (mouse_z == mouse_z_was) { return 0; }

    int result = 0;
    if (mouse_z > mouse_z_was)
        result = 1;   // eMouseWheelNorth
    else
        result = -1;  // eMouseWheelSouth
    mouse_z_was = mouse_z;
    return result;
}

/*
 new ags_getch
 who reads this.. just ags?  we might not need to convert to crazy stuff..
 
 oh wait, there's the abort key. so at least one thing to check.
 and the skip dialog key
 plus we send to plugins
 
 ok, so keep a buffer/queue of key downs (use stl queue if written here)
 add a callback for "on key events"
 with a function to map to old AGS values if we need to.
 
 the asci byte changes
 no mod - ascii
 shift - capitalises, shifts
 ctrl - a-0, b-1, etc.. or maybe it's a==1
 alt - 0, only scancode
 
 
 check out massive 2nd table
 http://www.plantation-productions.com/Webster/www.artofasm.com/DOS/ch20/CH20-1.html
 and
 https://github.com/Henne/dosbox-svn/blob/ac06986809899ea5f922cb29a194e0770169e1ad/src/ints/bios_keyboard.cpp
 
 and some of the keycodes are ascii
 https://github.com/spurious/SDL-mirror/blob/master/include/SDL_keycode.h
 
*/

void ags_clear_input_buffer()
{
    process_pending_events();
    while(!textEventQueue.empty()) { textEventQueue.pop(); }
    for(;;) {
        if (mgetbutton() == NONE) { break; }
    }
}

int asciiFromEvent(SDL_Event event)
{
    
    if (event.type == SDL_TEXTINPUT) {
#pragma message ("SDL-TODO: this breaks unicode...")
        int textch = event.text.text[0];
        if ((textch >= 32) && (textch <= 126)) {
            return textch;
        }
        return 0;
    }
    
    if (event.type == SDL_KEYDOWN) {
        switch(event.key.keysym.scancode) {
            case SDL_SCANCODE_BACKSPACE/* Backspace */: return ASCII_BACKSPACE;
                
            case SDL_SCANCODE_TAB/* Tab */:
            case SDL_SCANCODE_KP_TAB/* Tab */: return ASCII_TAB;
                
            case SDL_SCANCODE_RETURN/* Return */:
            case SDL_SCANCODE_RETURN2/* Return */:
            case SDL_SCANCODE_KP_ENTER/* Return */: return ASCII_RETURN;
                
            case SDL_SCANCODE_ESCAPE/* Escape */: return ASCII_ESCAPE;
                
            default: return 0;
        }
    }
    
    return 0;
}


/*
Used to pass to "on_key_press" event handler in scripts and to check for custom abort key.

These map to the eKeyCode enum values defined in agsdefns.sh
Special case of alt-a/z which starts from 300
*/
int agsKeyCodeFromEvent(SDL_Event event)
{
    
    if (event.type == SDL_TEXTINPUT) {
        int textch = event.text.text[0];
        if ((textch >= 32) && (textch <= 126)) {
            if (isalpha(textch)) {
                textch = toupper(textch);
            }
            return textch;
        }
        return 0;
    }
    
    if (event.type == SDL_KEYDOWN) {
        // if ALT, convert to allegro(4!!)(FOUR) scancode then add 300.
        // e.g alt-x is 324 which is 300 +
        // allegro 4 has ctrl-a-z mapped to 1-26
        
        // CTRL is a=1, z=26.. some others have weird ones.
        
        // so just support ctrl or alt -letters
        if (event.key.keysym.mod & (KMOD_ALT|KMOD_CTRL)) {
            int offset = 0;
            if (event.key.keysym.mod & KMOD_ALT) {
                offset = 300;
            }
            switch (event.key.keysym.sym) {
                case SDLK_a: return offset + 1;
                case SDLK_b: return offset + 2;
                case SDLK_c: return offset + 3;
                case SDLK_d: return offset + 4;
                case SDLK_e: return offset + 5;
                case SDLK_f: return offset + 6;
                case SDLK_g: return offset + 7;
                case SDLK_h: return offset + 8;
                case SDLK_i: return offset + 9;
                case SDLK_j: return offset + 10;
                case SDLK_k: return offset + 11;
                case SDLK_l: return offset + 12;
                case SDLK_m: return offset + 13;
                case SDLK_n: return offset + 14;
                case SDLK_o: return offset + 15;
                case SDLK_p: return offset + 16;
                case SDLK_q: return offset + 17;
                case SDLK_r: return offset + 18;
                case SDLK_s: return offset + 19;
                case SDLK_t: return offset + 20;
                case SDLK_u: return offset + 21;
                case SDLK_v: return offset + 22;
                case SDLK_w: return offset + 23;
                case SDLK_x: return offset + 24;
                case SDLK_y: return offset + 25;
                case SDLK_z: return offset + 26;
                default: return 0;
            }
        }
        
        switch(event.key.keysym.scancode) {
            case SDL_SCANCODE_BACKSPACE/* Backspace */: return ASCII_BACKSPACE;
                
            case SDL_SCANCODE_TAB/* Tab */:
            case SDL_SCANCODE_KP_TAB/* Tab */: return ASCII_TAB;
                
            case SDL_SCANCODE_RETURN/* Return */:
            case SDL_SCANCODE_RETURN2/* Return */:
            case SDL_SCANCODE_KP_ENTER/* Return */: return ASCII_RETURN;
                
            case SDL_SCANCODE_ESCAPE/* Escape */: return ASCII_ESCAPE;
                
            case SDL_SCANCODE_F1/* F1 */: return 359;
            case SDL_SCANCODE_F2/* F2 */: return 360;
            case SDL_SCANCODE_F3/* F3 */: return 361;
            case SDL_SCANCODE_F4/* F4 */: return 362;
            case SDL_SCANCODE_F5/* F5 */: return 363;
            case SDL_SCANCODE_F6/* F6 */: return 364;
            case SDL_SCANCODE_F7/* F7 */: return 365;
            case SDL_SCANCODE_F8/* F8 */: return 366;
            case SDL_SCANCODE_F9/* F9 */: return 367;
            case SDL_SCANCODE_F10/* F10 */: return 368;
            case SDL_SCANCODE_F11/* F11 */: return 433;
            case SDL_SCANCODE_F12/* F12 */: return 434;
                
            case SDL_SCANCODE_KP_7/* Home */:
            case SDL_SCANCODE_HOME/* Home */: return 371;
                
            case SDL_SCANCODE_KP_8/* UpArrow */:
            case SDL_SCANCODE_UP/* UpArrow */: return 372;
                
            case SDL_SCANCODE_KP_9/* PageUp */:
            case SDL_SCANCODE_PAGEUP/* PageUp */: return 373;
                
            case SDL_SCANCODE_KP_4/* LeftArrow */:
            case SDL_SCANCODE_LEFT/* LeftArrow */: return 375;
                
            case SDL_SCANCODE_KP_5/* NumPad5 */: return 376;
                
            case SDL_SCANCODE_KP_6/* RightArrow */:
            case SDL_SCANCODE_RIGHT/* RightArrow */: return 377;
                
            case SDL_SCANCODE_KP_1/* End */:
            case SDL_SCANCODE_END/* End */: return 379;
                
            case SDL_SCANCODE_KP_2/* End */:
            case SDL_SCANCODE_DOWN/* DownArrow */: return 380;
                
            case SDL_SCANCODE_KP_3/* End */:
            case SDL_SCANCODE_PAGEDOWN/* PageDown */: return 381;
                
            case SDL_SCANCODE_KP_0/* End */:
            case SDL_SCANCODE_INSERT/* Insert */: return 382;
                
            case SDL_SCANCODE_KP_PERIOD/* End */:
            case SDL_SCANCODE_DELETE/* Delete */: return 383;
                
            default: return 0;
        }
    }
    
    return 0;
}

// This tries to return AGS3 keyboard events. It's not great as you risk double keys.
int asciiOrAgsKeyCodeFromEvent(SDL_Event event)
{
    int result = asciiFromEvent(event);
    if (result > 0) { return result; }
    result = agsKeyCodeFromEvent(event);
    if (result > 0) { return result; }
    return -1;
}
