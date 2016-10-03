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
#include "allegro/internal/aintern.h"
#include <strings.h>
#include <ctype.h>


int *allegro_errno = NULL;



char empty_string[] = EMPTY_STRING;

extern void *_mangled_main_address;
#undef main
int main(int argc, char *argv[]) {
    int (*real_main)(int, char*[]) = (int (*)(int, char*[])) _mangled_main_address;
    real_main(argc, argv);
    return 0;
}

// System
int install_allegro(int system_id, int *errno_ptr, int (*atexit_ptr)(void (*func)(void))) {
    
    //  configure SDL2
    if( SDL_Init( SDL_INIT_TIMER|SDL_INIT_AUDIO|SDL_INIT_VIDEO|SDL_INIT_EVENTS ) < 0 )
    {
        return -1;
    }
    
    allegro_errno = &errno;
    return 0;
}

void allegro_exit(void)
{
    SDL_Quit();
}

char allegro_error[ALLEGRO_ERROR_SIZE];


// --------------------------------------------------------------------------
// timers
// --------------------------------------------------------------------------

#define TIMER_TO_MSEC(x)      ((long)(x) / (TIMERS_PER_SECOND / 1000))
#define MAX_TIMERS      (16)

struct _allegro_timer {
    void *proc;
    void *param;
    long speed;
    int param_used;
    
    int sdl_timer_id;
};

static struct _allegro_timer _timers[MAX_TIMERS];

int install_timer(void) { return 0; }

static Uint32 _timer_callback(Uint32 interval, struct _allegro_timer *param) {
    if (param->param_used) {
        void (*_allegro_callback)(void *) = param->proc;
        (*_allegro_callback)(param->param);
    } else {
        void (*_allegro_callback)(void) = param->proc;
        (*_allegro_callback)();
    }
    
    return TIMER_TO_MSEC(param->speed);
};

static int install_timer_int(void *proc, void *param, long speed, int param_used)
{
    // if exists, and same parameters,
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (_timers[i].sdl_timer_id != 0 && _timers[i].proc == proc && _timers[i].param == param && _timers[i].param_used == param_used) {
            _timers[i].speed = speed;
        }
    }
    
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (_timers[i].sdl_timer_id == 0) {
            _timers[i].proc = proc;
            _timers[i].param = param;
            _timers[i].param_used = param_used;
            _timers[i].speed = speed;
            _timers[i].sdl_timer_id = SDL_AddTimer(TIMER_TO_MSEC(speed), _timer_callback, &_timers[i]);
            
        }
    }
    
    return -1;
}

int install_int_ex(void (*proc)(void), long speed)
{
    return install_timer_int((void *)proc, NULL, speed, FALSE);
}
int install_param_int(void (*proc)(void *param), void *param, long speed)
{
    return install_timer_int((void *)proc, param, MSEC_TO_TIMER(speed), TRUE);
}
int install_param_int_ex(void (*proc)(void *param), void *param, long speed)
{
    return install_timer_int((void *)proc, param, speed, TRUE);
}
void remove_param_int(void (*proc)(void *param), void *param)
{
    // remove based on unique proc/param pair
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (_timers[i].sdl_timer_id != 0 && _timers[i].proc == proc && _timers[i].param == param && _timers[i].param_used == TRUE) {
            SDL_RemoveTimer(_timers[i].sdl_timer_id);
            _timers[i].sdl_timer_id = 0;
        }
    }
    
}

void rest(unsigned int time)
{
    SDL_Delay(time);
}



// --------------------------------------------------------------------------
// config
// --------------------------------------------------------------------------

const char *get_config_text(const char *msg) { return msg; } // used by set_allegro_error




// --------------------------------------------------------------------------
// strings
// --------------------------------------------------------------------------

int _alemu_stricmp(const char *s1, const char *s2)
{
    return strcasecmp(s1, s2);
}
char *_alemu_strlwr(char *str)
{
    for (char *p = str; *p; p++) {
        *p = tolower(*p);
    }
    return str;
}
char *_alemu_strupr(char *str)
{
    for (char *p = str; *p; p++) {
        *p = toupper(*p);
    }
    return str;
}


// --------------------------------------------------------------------------
// unicode
// --------------------------------------------------------------------------
static int _uformat = 0;
void set_uformat(int type) { _uformat = type; }
int get_uformat(void) { return _uformat; }
int need_uconvert(const char *s, int type, int newtype) { return FALSE; }
int utf8_getc(const char *s) { return *s; }
int utf8_getx(char **s) { return *((unsigned char *)((*s)++)); }
int (*ugetc)(AL_CONST char *s) = utf8_getc;
int (*ugetxc)(AL_CONST char** s) = (int (*)(AL_CONST char**)) utf8_getx;
int ustrsize(const char *s) { return strlen(s); }
int uvszprintf(char *buf, int size, const char *format, va_list args) { return vsprintf(buf, format, args);}
char *uconvert(const char *s, int type, char *buf, int newtype, int size) { return s; }

