//
//  sdlwrap.c
//  AGSKit
//
//  Created by Nick Sonneveld on 9/08/2016.
//  Copyright © 2016 Adventure Game Studio. All rights reserved.
//

#include "sdlwrap.h"
#include <SDL2/SDL.h>
#include "allegro.h"
#include <strings.h>
#include <ctype.h>

static void (*_on_close_callback)(void) = 0;
int set_close_button_callback(void (*proc)(void))
{
    _on_close_callback = proc;
    return 0;
}

static void process_available_events();

// --------------------------------------------------------------------------
// Keyboard
// --------------------------------------------------------------------------

static int allegro_scancode_from_sdl (int code);
static int sdl_scancode_from_allegro (int code);
static int allegro_mod_flags_from_sdl(int code);

//#define KEY_MAX (127)
volatile char key[KEY_MAX];  // uses KEY_SCRLOCK
volatile int key_shifts;



#define _READKEY_BUF_SIZE 32
SDL_Keysym _readkey_buf[_READKEY_BUF_SIZE];
int _readkey_start = 0;  // oldest
int _readkey_end = 0;    // spot to write next one.

int _readkey_buffer_is_full() {
    return (_readkey_end+1)%_READKEY_BUF_SIZE == _readkey_start;
}

int _readkey_buffer_is_empty() {
    return _readkey_start == _readkey_end;
}

void _readkey_buffer_push(SDL_Keysym value)  {
    //assert(!_readkey_buffer_is_full());
    
    _readkey_buf[_readkey_end] = value;
    _readkey_end = (_readkey_end+1)%_READKEY_BUF_SIZE;
    
    //assert (_readkey_start != _readkey_end);
}

SDL_Keysym _readkey_buffer_pop() {
    //assert(!_readkey_buffer_is_empty());
    SDL_Keysym value = _readkey_buf[_readkey_start];
    _readkey_start = (_readkey_start+1)%_READKEY_BUF_SIZE;
    return value;
}

void _readkey_buffer_clear() {
    _readkey_start = 0;
    _readkey_end = 0;
    
}







int install_keyboard() { return 0; }
int keyboard_needs_poll() { return 1; }

static void on_sdl_key_up_down(SDL_KeyboardEvent *event) {
    const int keycode = allegro_scancode_from_sdl(event->keysym.scancode);
    
    if (keycode > 0) {
        key[keycode] = event->state == SDL_PRESSED;
    }
    
    const int keyflags = allegro_mod_flags_from_sdl (event->keysym.scancode);
    if (keyflags) {
        if (event->state == SDL_PRESSED) {
            key_shifts &= ~keyflags;
        } else {
            key_shifts |= keyflags;
        }
    }
    
    
    if (event->type == SDL_KEYDOWN) {
        
        
        
        if (!_readkey_buffer_is_full()) {
            
        
            int allegro_scancode = allegro_scancode_from_sdl(event->keysym.scancode);
            if (allegro_scancode <= 0)
                return;
        
            _readkey_buffer_push(event->keysym);
        }
    }
    
    
}

int poll_keyboard()
{
    process_available_events();
    return 0;
}
int keypressed()
{
    poll_keyboard();
    return !_readkey_buffer_is_empty();
}
int readkey()
{

    
    
    for (;;) {
        poll_keyboard();
        if (!_readkey_buffer_is_empty())
            break;
        SDL_Delay(1);
    }
    
    SDL_Keysym sdlkeysym = _readkey_buffer_pop();
    int allegro_scancode = allegro_scancode_from_sdl(sdlkeysym.scancode);
    int allegro_ascii = sdlkeysym.sym & 0x7f;
    int value = allegro_ascii | (allegro_scancode << 8);
    return value;
    
}
void set_leds(int leds) { /* the leds, they do nothing */ }



// --------------------------------------------------------------------------
// Mouse
// --------------------------------------------------------------------------

volatile int mouse_x = 0;              /* user-visible position */
volatile int mouse_y = 0;
volatile int mouse_z = 0;
volatile int mouse_b = 0;

static int _current_mickey_x = 0;
static int _current_mickey_y = 0;

static int _mouse_range_rect_set = 0;
static SDL_Rect _mouse_range_rect = {100, 100, 100, 100};

static int _within_rect(SDL_Rect *rect, int x, int y)
{
    return x >= rect->x && x < (rect->x+rect->w) && y >= rect->y && y < (rect->y+rect->h);
}

static int _custom_filter(void *userdata, SDL_Event *event)
{
    switch (event->type){
        case SDL_MOUSEMOTION:
            if (_mouse_range_rect_set)
                return _within_rect(&_mouse_range_rect, event->motion.x, event->motion.y);
        case SDL_MOUSEBUTTONDOWN:
            if (_mouse_range_rect_set)
                return _within_rect(&_mouse_range_rect, event->button.x, event->button.y);
        case SDL_MOUSEBUTTONUP:
            // don't actually filter mouse up events!
            break;
    }
    return 1;
}

static int _sdl_button_to_allegro_bit(int button)
{
    switch (button) {
        case SDL_BUTTON_LEFT: return 0x1;
        case SDL_BUTTON_MIDDLE: return 0x4;
        case SDL_BUTTON_RIGHT: return 0x2;
        case SDL_BUTTON_X1: return 0x8;
        case SDL_BUTTON_X2: return 0x10;
    }
    return 0x0;
}

static void _on_sdl_mouse_button_down(SDL_MouseButtonEvent *event)
{
    mouse_b |= _sdl_button_to_allegro_bit(event->button);
}

static void _on_sdl_mouse_button_up(SDL_MouseButtonEvent *event)
{
    mouse_b &= ~_sdl_button_to_allegro_bit(event->button);
}

static void _on_sdl_mouse_motion(SDL_MouseMotionEvent *event)
{
    mouse_x = event->x;
    mouse_y = event->y;
    
    _current_mickey_x += event->xrel;
    _current_mickey_y += event->yrel;
}

static void _on_sdl_mouse_wheel(SDL_MouseWheelEvent *event)
{
    mouse_z += event->y;
}

int install_mouse()
{
    SDL_SetEventFilter(&_custom_filter, 0);
    SDL_ShowCursor(SDL_DISABLE);
    
    // return number of mouse buttons.  for SDL it's 5 or so.
    return SDL_BUTTON_X2;
}

int poll_mouse()
{
    process_available_events();
    return 0;
}

void get_mouse_mickeys(int *mickeyx, int *mickeyy)
{
    *mickeyx = _current_mickey_x;
    *mickeyy = _current_mickey_y;
}

void position_mouse(int x, int y)
{
    // disabled cause it makes it difficult to actually close the window if we're
    // constantly warping the mouse.
#if 0
    if (_original_screen && _original_screen->extra) {
        SDL_WarpMouseInWindow(_original_screen->extra, x, y);
        // the new pos appears in allegro, so make sure it is
        // set even if the event doesn't occur.
        alw_mouse_x = x;
        alw_mouse_y = y;
    }
#endif
}

void set_mouse_range(int x1, int y1, int x2, int y2)
{
    _mouse_range_rect_set = 1;
    _mouse_range_rect.x = x1;
    _mouse_range_rect.y = y1;
    _mouse_range_rect.w = x2-x1+1;
    _mouse_range_rect.h = y2-y1+1;
}

int show_os_cursor(int cursor)
{
    switch (cursor) {
        case MOUSE_CURSOR_NONE:
            SDL_ShowCursor(SDL_DISABLE);
            return -1;
            break;
        default:
            SDL_ShowCursor(SDL_ENABLE);
            return 0;
    }
}


// --------------------------------------------------------------------------
// Event processing
// --------------------------------------------------------------------------

static void process_available_events() {
    SDL_Event event;
    
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
                
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                on_sdl_key_up_down(&event.key);
                break;

            case SDL_MOUSEBUTTONDOWN:
                _on_sdl_mouse_button_down(&event.button);
                break;
            case SDL_MOUSEBUTTONUP:
                _on_sdl_mouse_button_up(&event.button);
                break;
            case SDL_MOUSEMOTION:
                _on_sdl_mouse_motion(&event.motion);
                break;
            case SDL_MOUSEWHEEL:
                _on_sdl_mouse_wheel(&event.wheel);
                break;
                
            case SDL_QUIT:
                if (_on_close_callback) {
                    _on_close_callback();
                }
                break;
        }
    }
}



// --------------------------------------------------------------------------
// Type Mapping
// --------------------------------------------------------------------------

static int sdl_scancode_from_allegro (int code) {
    switch (code) {
        case __allegro_KEY_A: return SDL_SCANCODE_A;
        case __allegro_KEY_B: return SDL_SCANCODE_B;
        case __allegro_KEY_C: return SDL_SCANCODE_C;
        case __allegro_KEY_D: return SDL_SCANCODE_D;
        case __allegro_KEY_E: return SDL_SCANCODE_E;
        case __allegro_KEY_F: return SDL_SCANCODE_F;
        case __allegro_KEY_G: return SDL_SCANCODE_G;
        case __allegro_KEY_H: return SDL_SCANCODE_H;
        case __allegro_KEY_I: return SDL_SCANCODE_I;
        case __allegro_KEY_J: return SDL_SCANCODE_J;
        case __allegro_KEY_K: return SDL_SCANCODE_K;
        case __allegro_KEY_L: return SDL_SCANCODE_L;
        case __allegro_KEY_M: return SDL_SCANCODE_M;
        case __allegro_KEY_N: return SDL_SCANCODE_N;
        case __allegro_KEY_O: return SDL_SCANCODE_O;
        case __allegro_KEY_P: return SDL_SCANCODE_P;
        case __allegro_KEY_Q: return SDL_SCANCODE_Q;
        case __allegro_KEY_R: return SDL_SCANCODE_R;
        case __allegro_KEY_S: return SDL_SCANCODE_S;
        case __allegro_KEY_T: return SDL_SCANCODE_T;
        case __allegro_KEY_U: return SDL_SCANCODE_U;
        case __allegro_KEY_V: return SDL_SCANCODE_V;
        case __allegro_KEY_W: return SDL_SCANCODE_W;
        case __allegro_KEY_X: return SDL_SCANCODE_X;
        case __allegro_KEY_Y: return SDL_SCANCODE_Y;
        case __allegro_KEY_Z: return SDL_SCANCODE_Z;
        case __allegro_KEY_0: return SDL_SCANCODE_0;
        case __allegro_KEY_1: return SDL_SCANCODE_1;
        case __allegro_KEY_2: return SDL_SCANCODE_2;
        case __allegro_KEY_3: return SDL_SCANCODE_3;
        case __allegro_KEY_4: return SDL_SCANCODE_4;
        case __allegro_KEY_5: return SDL_SCANCODE_5;
        case __allegro_KEY_6: return SDL_SCANCODE_6;
        case __allegro_KEY_7: return SDL_SCANCODE_7;
        case __allegro_KEY_8: return SDL_SCANCODE_8;
        case __allegro_KEY_9: return SDL_SCANCODE_9;
        case __allegro_KEY_0_PAD: return SDL_SCANCODE_KP_0;
        case __allegro_KEY_1_PAD: return SDL_SCANCODE_KP_1;
        case __allegro_KEY_2_PAD: return SDL_SCANCODE_KP_2;
        case __allegro_KEY_3_PAD: return SDL_SCANCODE_KP_3;
        case __allegro_KEY_4_PAD: return SDL_SCANCODE_KP_4;
        case __allegro_KEY_5_PAD: return SDL_SCANCODE_KP_5;
        case __allegro_KEY_6_PAD: return SDL_SCANCODE_KP_6;
        case __allegro_KEY_7_PAD: return SDL_SCANCODE_KP_7;
        case __allegro_KEY_8_PAD: return SDL_SCANCODE_KP_8;
        case __allegro_KEY_9_PAD: return SDL_SCANCODE_KP_9;
        case __allegro_KEY_F1: return SDL_SCANCODE_F1;
        case __allegro_KEY_F2: return SDL_SCANCODE_F2;
        case __allegro_KEY_F3: return SDL_SCANCODE_F3;
        case __allegro_KEY_F4: return SDL_SCANCODE_F4;
        case __allegro_KEY_F5: return SDL_SCANCODE_F5;
        case __allegro_KEY_F6: return SDL_SCANCODE_F6;
        case __allegro_KEY_F7: return SDL_SCANCODE_F7;
        case __allegro_KEY_F8: return SDL_SCANCODE_F8;
        case __allegro_KEY_F9: return SDL_SCANCODE_F9;
        case __allegro_KEY_F10: return SDL_SCANCODE_F10;
        case __allegro_KEY_F11: return SDL_SCANCODE_F11;
        case __allegro_KEY_F12: return SDL_SCANCODE_F12;
        case __allegro_KEY_ESC: return SDL_SCANCODE_ESCAPE;
        case __allegro_KEY_TILDE: return SDL_SCANCODE_GRAVE;
        case __allegro_KEY_MINUS: return SDL_SCANCODE_MINUS;
        case __allegro_KEY_EQUALS: return SDL_SCANCODE_EQUALS;
        case __allegro_KEY_BACKSPACE: return SDL_SCANCODE_BACKSPACE;
        case __allegro_KEY_TAB: return SDL_SCANCODE_TAB;
        case __allegro_KEY_OPENBRACE: return SDL_SCANCODE_LEFTBRACKET;
        case __allegro_KEY_CLOSEBRACE: return SDL_SCANCODE_RIGHTBRACKET;
        case __allegro_KEY_ENTER: return SDL_SCANCODE_RETURN;
        case __allegro_KEY_COLON: return SDL_SCANCODE_SEMICOLON;
        case __allegro_KEY_QUOTE: return SDL_SCANCODE_APOSTROPHE;
        case __allegro_KEY_BACKSLASH: return SDL_SCANCODE_BACKSLASH;
        case __allegro_KEY_COMMA: return SDL_SCANCODE_COMMA;
        case __allegro_KEY_STOP: return SDL_SCANCODE_PERIOD;
        case __allegro_KEY_SLASH: return SDL_SCANCODE_SLASH;
        case __allegro_KEY_SPACE: return SDL_SCANCODE_SPACE;
        case __allegro_KEY_INSERT: return SDL_SCANCODE_INSERT;
        case __allegro_KEY_DEL: return SDL_SCANCODE_DELETE;
        case __allegro_KEY_HOME: return SDL_SCANCODE_HOME;
        case __allegro_KEY_END: return SDL_SCANCODE_END;
        case __allegro_KEY_PGUP: return SDL_SCANCODE_PAGEUP;
        case __allegro_KEY_PGDN: return SDL_SCANCODE_PAGEDOWN;
        case __allegro_KEY_LEFT: return SDL_SCANCODE_LEFT;
        case __allegro_KEY_RIGHT: return SDL_SCANCODE_RIGHT;
        case __allegro_KEY_UP: return SDL_SCANCODE_UP;
        case __allegro_KEY_DOWN: return SDL_SCANCODE_DOWN;
        case __allegro_KEY_SLASH_PAD: return SDL_SCANCODE_KP_DIVIDE;
        case __allegro_KEY_ASTERISK: return SDL_SCANCODE_KP_MULTIPLY;
        case __allegro_KEY_MINUS_PAD: return SDL_SCANCODE_KP_MINUS;
        case __allegro_KEY_PLUS_PAD: return SDL_SCANCODE_KP_PLUS;
        case __allegro_KEY_DEL_PAD: return SDL_SCANCODE_KP_BACKSPACE;
        case __allegro_KEY_ENTER_PAD: return SDL_SCANCODE_KP_ENTER;
        case __allegro_KEY_PRTSCR: return SDL_SCANCODE_PRINTSCREEN;
        case __allegro_KEY_PAUSE: return SDL_SCANCODE_PAUSE;
        case __allegro_KEY_EQUALS_PAD: return SDL_SCANCODE_KP_EQUALS;
        case __allegro_KEY_BACKQUOTE: return SDL_SCANCODE_GRAVE;
        case __allegro_KEY_SEMICOLON: return SDL_SCANCODE_SEMICOLON;
        case __allegro_KEY_COMMAND: return SDL_SCANCODE_LGUI;
        case __allegro_KEY_LSHIFT: return SDL_SCANCODE_LSHIFT;
        case __allegro_KEY_RSHIFT: return SDL_SCANCODE_RSHIFT;
        case __allegro_KEY_LCONTROL: return SDL_SCANCODE_LCTRL;
        case __allegro_KEY_RCONTROL: return SDL_SCANCODE_RCTRL;
        case __allegro_KEY_ALT: return SDL_SCANCODE_LALT;
        case __allegro_KEY_ALTGR: return SDL_SCANCODE_RALT;
        case __allegro_KEY_LWIN: return SDL_SCANCODE_LGUI;
        case __allegro_KEY_RWIN: return SDL_SCANCODE_RGUI;
        case __allegro_KEY_MENU: return SDL_SCANCODE_APPLICATION;
        case __allegro_KEY_SCRLOCK: return SDL_SCANCODE_SCROLLLOCK;
        case __allegro_KEY_NUMLOCK: return SDL_SCANCODE_NUMLOCKCLEAR;
        case __allegro_KEY_CAPSLOCK: return SDL_SCANCODE_CAPSLOCK;
        default: return -1;
    }
}


static int allegro_scancode_from_sdl (int code) {
    switch (code) {
        case SDL_SCANCODE_A: return __allegro_KEY_A;
        case SDL_SCANCODE_B: return __allegro_KEY_B;
        case SDL_SCANCODE_C: return __allegro_KEY_C;
        case SDL_SCANCODE_D: return __allegro_KEY_D;
        case SDL_SCANCODE_E: return __allegro_KEY_E;
        case SDL_SCANCODE_F: return __allegro_KEY_F;
        case SDL_SCANCODE_G: return __allegro_KEY_G;
        case SDL_SCANCODE_H: return __allegro_KEY_H;
        case SDL_SCANCODE_I: return __allegro_KEY_I;
        case SDL_SCANCODE_J: return __allegro_KEY_J;
        case SDL_SCANCODE_K: return __allegro_KEY_K;
        case SDL_SCANCODE_L: return __allegro_KEY_L;
        case SDL_SCANCODE_M: return __allegro_KEY_M;
        case SDL_SCANCODE_N: return __allegro_KEY_N;
        case SDL_SCANCODE_O: return __allegro_KEY_O;
        case SDL_SCANCODE_P: return __allegro_KEY_P;
        case SDL_SCANCODE_Q: return __allegro_KEY_Q;
        case SDL_SCANCODE_R: return __allegro_KEY_R;
        case SDL_SCANCODE_S: return __allegro_KEY_S;
        case SDL_SCANCODE_T: return __allegro_KEY_T;
        case SDL_SCANCODE_U: return __allegro_KEY_U;
        case SDL_SCANCODE_V: return __allegro_KEY_V;
        case SDL_SCANCODE_W: return __allegro_KEY_W;
        case SDL_SCANCODE_X: return __allegro_KEY_X;
        case SDL_SCANCODE_Y: return __allegro_KEY_Y;
        case SDL_SCANCODE_Z: return __allegro_KEY_Z;
        case SDL_SCANCODE_0: return __allegro_KEY_0;
        case SDL_SCANCODE_1: return __allegro_KEY_1;
        case SDL_SCANCODE_2: return __allegro_KEY_2;
        case SDL_SCANCODE_3: return __allegro_KEY_3;
        case SDL_SCANCODE_4: return __allegro_KEY_4;
        case SDL_SCANCODE_5: return __allegro_KEY_5;
        case SDL_SCANCODE_6: return __allegro_KEY_6;
        case SDL_SCANCODE_7: return __allegro_KEY_7;
        case SDL_SCANCODE_8: return __allegro_KEY_8;
        case SDL_SCANCODE_9: return __allegro_KEY_9;
        case SDL_SCANCODE_KP_0: return __allegro_KEY_0_PAD;
        case SDL_SCANCODE_KP_1: return __allegro_KEY_1_PAD;
        case SDL_SCANCODE_KP_2: return __allegro_KEY_2_PAD;
        case SDL_SCANCODE_KP_3: return __allegro_KEY_3_PAD;
        case SDL_SCANCODE_KP_4: return __allegro_KEY_4_PAD;
        case SDL_SCANCODE_KP_5: return __allegro_KEY_5_PAD;
        case SDL_SCANCODE_KP_6: return __allegro_KEY_6_PAD;
        case SDL_SCANCODE_KP_7: return __allegro_KEY_7_PAD;
        case SDL_SCANCODE_KP_8: return __allegro_KEY_8_PAD;
        case SDL_SCANCODE_KP_9: return __allegro_KEY_9_PAD;
        case SDL_SCANCODE_F1: return __allegro_KEY_F1;
        case SDL_SCANCODE_F2: return __allegro_KEY_F2;
        case SDL_SCANCODE_F3: return __allegro_KEY_F3;
        case SDL_SCANCODE_F4: return __allegro_KEY_F4;
        case SDL_SCANCODE_F5: return __allegro_KEY_F5;
        case SDL_SCANCODE_F6: return __allegro_KEY_F6;
        case SDL_SCANCODE_F7: return __allegro_KEY_F7;
        case SDL_SCANCODE_F8: return __allegro_KEY_F8;
        case SDL_SCANCODE_F9: return __allegro_KEY_F9;
        case SDL_SCANCODE_F10: return __allegro_KEY_F10;
        case SDL_SCANCODE_F11: return __allegro_KEY_F11;
        case SDL_SCANCODE_F12: return __allegro_KEY_F12;
        case SDL_SCANCODE_ESCAPE: return __allegro_KEY_ESC;
        case SDL_SCANCODE_MINUS: return __allegro_KEY_MINUS;
        case SDL_SCANCODE_EQUALS: return __allegro_KEY_EQUALS;
        case SDL_SCANCODE_BACKSPACE: return __allegro_KEY_BACKSPACE;
        case SDL_SCANCODE_TAB: return __allegro_KEY_TAB;
        case SDL_SCANCODE_LEFTBRACKET: return __allegro_KEY_OPENBRACE;
        case SDL_SCANCODE_RIGHTBRACKET: return __allegro_KEY_CLOSEBRACE;
        case SDL_SCANCODE_RETURN: return __allegro_KEY_ENTER;
        case SDL_SCANCODE_APOSTROPHE: return __allegro_KEY_QUOTE;
        case SDL_SCANCODE_BACKSLASH: return __allegro_KEY_BACKSLASH;
        case SDL_SCANCODE_COMMA: return __allegro_KEY_COMMA;
        case SDL_SCANCODE_PERIOD: return __allegro_KEY_STOP;
        case SDL_SCANCODE_SLASH: return __allegro_KEY_SLASH;
        case SDL_SCANCODE_SPACE: return __allegro_KEY_SPACE;
        case SDL_SCANCODE_INSERT: return __allegro_KEY_INSERT;
        case SDL_SCANCODE_DELETE: return __allegro_KEY_DEL;
        case SDL_SCANCODE_HOME: return __allegro_KEY_HOME;
        case SDL_SCANCODE_END: return __allegro_KEY_END;
        case SDL_SCANCODE_PAGEUP: return __allegro_KEY_PGUP;
        case SDL_SCANCODE_PAGEDOWN: return __allegro_KEY_PGDN;
        case SDL_SCANCODE_LEFT: return __allegro_KEY_LEFT;
        case SDL_SCANCODE_RIGHT: return __allegro_KEY_RIGHT;
        case SDL_SCANCODE_UP: return __allegro_KEY_UP;
        case SDL_SCANCODE_DOWN: return __allegro_KEY_DOWN;
        case SDL_SCANCODE_KP_DIVIDE: return __allegro_KEY_SLASH_PAD;
        case SDL_SCANCODE_KP_MULTIPLY: return __allegro_KEY_ASTERISK;
        case SDL_SCANCODE_KP_MINUS: return __allegro_KEY_MINUS_PAD;
        case SDL_SCANCODE_KP_PLUS: return __allegro_KEY_PLUS_PAD;
        case SDL_SCANCODE_KP_BACKSPACE: return __allegro_KEY_DEL_PAD;
        case SDL_SCANCODE_KP_ENTER: return __allegro_KEY_ENTER_PAD;
        case SDL_SCANCODE_PRINTSCREEN: return __allegro_KEY_PRTSCR;
        case SDL_SCANCODE_PAUSE: return __allegro_KEY_PAUSE;
        case SDL_SCANCODE_KP_EQUALS: return __allegro_KEY_EQUALS_PAD;
        case SDL_SCANCODE_GRAVE: return __allegro_KEY_BACKQUOTE;
        case SDL_SCANCODE_SEMICOLON: return __allegro_KEY_SEMICOLON;
        case SDL_SCANCODE_LSHIFT: return __allegro_KEY_LSHIFT;
        case SDL_SCANCODE_RSHIFT: return __allegro_KEY_RSHIFT;
        case SDL_SCANCODE_LCTRL: return __allegro_KEY_LCONTROL;
        case SDL_SCANCODE_RCTRL: return __allegro_KEY_RCONTROL;
        case SDL_SCANCODE_LALT: return __allegro_KEY_ALT;
        case SDL_SCANCODE_RALT: return __allegro_KEY_ALT;
#ifdef ALLEGRO_MACOSX
        case SDL_SCANCODE_LGUI: return __allegro_KEY_COMMAND;
        case SDL_SCANCODE_RGUI: return __allegro_KEY_COMMAND;
#else
        case SDL_SCANCODE_LGUI: return __allegro_KEY_LWIN;
        case SDL_SCANCODE_RGUI: return __allegro_KEY_RWIN;
#endif
        case SDL_SCANCODE_APPLICATION: return __allegro_KEY_MENU;
        case SDL_SCANCODE_SCROLLLOCK: return __allegro_KEY_SCRLOCK;
        case SDL_SCANCODE_NUMLOCKCLEAR: return __allegro_KEY_NUMLOCK;
        case SDL_SCANCODE_CAPSLOCK: return __allegro_KEY_CAPSLOCK;
        default: return -1;
    }
}

static int allegro_mod_flags_from_sdl(int code) {
    
    // AGS only cares about scroll-lock, numlock, capslock, alt, ctrl
    
    switch (code) {
            
        case  SDL_SCANCODE_LSHIFT:  return KB_SHIFT_FLAG;
        case  SDL_SCANCODE_RSHIFT:  return KB_SHIFT_FLAG;
            
        case  SDL_SCANCODE_LCTRL:  return KB_CTRL_FLAG;
        case  SDL_SCANCODE_RCTRL:  return KB_CTRL_FLAG;
            
        case  SDL_SCANCODE_LALT:  return KB_ALT_FLAG;
        case  SDL_SCANCODE_RALT:  return KB_ALT_FLAG;
#ifdef ALLEGRO_MACOSX
        case  SDL_SCANCODE_LGUI:  return KB_COMMAND_FLAG;
        case  SDL_SCANCODE_RGUI:  return KB_COMMAND_FLAG;
#else
        case SDL_SCANCODE_LGUI:  return KB_LWIN_FLAG;
        case SDL_SCANCODE_RGUI:  return KB_RWIN_FLAG;
#endif
            
        case  SDL_SCANCODE_APPLICATION:  return KB_MENU_FLAG;
        case  SDL_SCANCODE_SCROLLLOCK:  return KB_SCROLOCK_FLAG;
        case  SDL_SCANCODE_NUMLOCKCLEAR:  return KB_NUMLOCK_FLAG;
        case  SDL_SCANCODE_CAPSLOCK:  return KB_CAPSLOCK_FLAG;
    }
    return 0;
    
}