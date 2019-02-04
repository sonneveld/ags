// SDL-TODO maybe leave record.cpp, create record_sdl2.cpp

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

#define IS_RECORD_UNIT

#include <queue>

#include "ac/common.h"
#include "media/audio/audiodefines.h"
#include "ac/game.h"
#include "ac/gamesetupstruct.h"
#include "ac/gamestate.h"
#include "ac/global_display.h"
#include "ac/global_game.h"
#include "ac/keycode.h"
#include "ac/mouse.h"
#include "ac/record.h"
#include "game/savegame.h"
#include "main/main.h"
#include "media/audio/soundclip.h"
#include "util/string_utils.h"
#include "gfx/gfxfilter.h"
#include "device/mousew32.h"
#include "util/file.h"
#include "util/filestream.h"
#include "gfx/graphicsdriver.h"
#include "ac/timer.h"

using namespace AGS::Common;
using namespace AGS::Engine;

extern GameSetupStruct game;
extern GameState play;
extern int disable_mgetgraphpos;
extern int mousex,mousey;
extern unsigned int loopcounter,lastcounter;
extern SOUNDCLIP *channels[MAX_SOUND_CHANNELS+1];
extern int pluginSimulatedClick;
extern int displayed_room;
extern char check_dynamic_sprites_at_exit;

extern volatile char want_exit, abort_engine;

extern IGraphicsDriver *gfxDriver;

int mouse_z_was = 0;

static std::queue<SDL_Event> textEventQueue;

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

SDL_Event getTextEventFromQueue() {
    SDL_Event result = { 0 };

    if (!textEventQueue.empty()) {
        result = textEventQueue.front();
        textEventQueue.pop();
    }
    
    if ((getGlobalTimerCounterMs() < play.ignore_user_input_until_time)) {
        // ignoring user input
        result =  { 0 };
    }
    
    return result;
}

void process_pending_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {

        switch (event.type) {
            
            case SDL_WINDOWEVENT: {
                
                switch(event.window.event) {
                    // case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        Size windowSize(event.window.data1, event.window.data2);
                        gfxDriver->UpdateDeviceScreen(windowSize);
                        break;
                }
                break;
            }

            case SDL_KEYDOWN:
            case SDL_TEXTINPUT:
                onkeydown(&event);
                break;
            case SDL_KEYUP:
            case SDL_MOUSEMOTION:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            case SDL_QUIT:
                break;
        }

        sdl2_process_single_event(&event);
    }
}


int rec_getch () {
    return my_readkey();
}

#pragma message ("SDL-TODO: still used in invwindow.  need to check where kbhit was used.. why was ignored?")
int rec_kbhit () {
    int result = keypressed();
    if ((result) && (getGlobalTimerCounterMs() < play.ignore_user_input_until_time))
    {
        // ignoring user input
        my_readkey();
        result = 0;
    }
    return result;  
}

int rec_iskeypressed (int allegroKeycode) {
    return key[allegroKeycode];
}

int rec_isSpeechFinished () {
    return channels[SCHAN_SPEECH]->done;
}

int rec_misbuttondown (int but) {
    return misbuttondown (but);
}

int rec_mgetbutton() {
    int result;

    if (pluginSimulatedClick > NONE) {
        result = pluginSimulatedClick;
        pluginSimulatedClick = NONE;
    }
    else {
        result = mgetbutton();
    }

    if ((result >= 0) && (getGlobalTimerCounterMs() < play.ignore_user_input_until_time))
    {
        // ignoring user input
        result = NONE;
    }

    return result;
}

int check_mouse_wheel () {
    int result = 0;
    if ((mouse_z != mouse_z_was) && (game.options[OPT_MOUSEWHEEL] != 0)) {
        if (mouse_z > mouse_z_was)
            result = 1;
        else
            result = -1;
        mouse_z_was = mouse_z;
    }
    return result;
}


/*
 new my_readkey..
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


// returns AGS key I think.
int my_readkey() {
    int gott=readkey();
    int scancode = ((gott >> 8) & 0x00ff);

    if (gott == READKEY_CODE_ALT_TAB)
    {
        // Alt+Tab, it gets stuck down unless we do this
        return AGS_KEYCODE_ALT_TAB;
    }

    /*  char message[200];
    sprintf(message, "Scancode: %04X", gott);
    Debug::Printf(message);*/

    /*if ((scancode >= KEY_0_PAD) && (scancode <= KEY_9_PAD)) {
    // fix numeric pad keys if numlock is off (allegro 4.2 changed this behaviour)
    SDL_PumpEvents();
    if ((SDL_GetModState() & KMOD_NUM) == 0)
    gott = (gott & 0xff00) | EXTENDED_KEY_CODE;
    }*/

    // alt something , or F keys or numpad/arrows
    if ((gott & 0x00ff) == EXTENDED_KEY_CODE) {
        gott = scancode + AGS_EXT_KEY_SHIFT;

        // convert Allegro KEY_* numbers to scan codes
        // (for backwards compatibility we can't just use the
        // KEY_* constants now, it's too late)
        if ((gott>=347) & (gott<=356)) gott+=12;
        // F11-F12
        else if ((gott==357) || (gott==358)) gott+=76;
        // insert / numpad insert
        else if ((scancode == KEY_0_PAD) || (scancode == KEY_INSERT))
            gott = AGS_KEYCODE_INSERT;
        // delete / numpad delete
        else if ((scancode == KEY_DEL_PAD) || (scancode == KEY_DEL))
            gott = AGS_KEYCODE_DELETE;
        // Home
        else if (gott == 378) gott = 371;
        // End
        else if (gott == 379) gott = 379;
        // PgUp
        else if (gott == 380) gott = 373;
        // PgDn
        else if (gott == 381) gott = 381;
        // left arrow
        else if (gott==382) gott=375;
        // right arrow
        else if (gott==383) gott=377;
        // up arrow
        else if (gott==384) gott=372;
        // down arrow
        else if (gott==385) gott=380;
        // numeric keypad
        else if (gott==338) gott=379;
        else if (gott==339) gott=380;
        else if (gott==340) gott=381;
        else if (gott==341) gott=375;
        else if (gott==342) gott=376;
        else if (gott==343) gott=377;
        else if (gott==344) gott=371;
        else if (gott==345) gott=372;
        else if (gott==346) gott=373;
    }
    else
    {
      gott = gott & 0x00ff;
#if defined(MAC_VERSION)
      if (scancode==KEY_BACKSPACE) {
        gott = 8; //j backspace on mac
      }
#endif
    }

    // Alt+X, abort (but only once game is loaded)
    if ((gott == play.abort_key) && (displayed_room >= 0)) {
        check_dynamic_sprites_at_exit = 0;
        quit("!|");
    }

    //sprintf(message, "Keypress: %d", gott);
    //Debug::Printf(message);

    return gott;
}

void clear_input_buffer()
{
    for(;;) {
        process_pending_events();
        if (!rec_kbhit()) { break; }
        rec_getch();
    }
    for(;;) {
        process_pending_events();
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

int asciiOrAgsKeyCodeFromEvent(SDL_Event event)
{
    int result = asciiFromEvent(event);
    if (result > 0) { return result; }
    result = agsKeyCodeFromEvent(event);
    if (result > 0) { return result; }
    return -1;
}

void wait_until_keypress()
{
    while (!rec_kbhit());
    rec_getch();
}
