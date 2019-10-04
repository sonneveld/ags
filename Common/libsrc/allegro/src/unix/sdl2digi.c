/*         ______   ___    ___
 *        /\  _  \ /\_ \  /\_ \
 *        \ \ \L\ \\//\ \ \//\ \      __     __   _ __   ___
 *         \ \  __ \ \ \ \  \ \ \   /'__`\ /'_ `\/\`'__\/ __`\
 *          \ \ \/\ \ \_\ \_ \_\ \_/\  __//\ \L\ \ \ \//\ \L\ \
 *           \ \_\ \_\/\____\/\____\ \____\ \____ \ \_\\ \____/
 *            \/_/\/_/\/____/\/____/\/____/\/___L\ \/_/ \/___/
 *                                           /\____/
 *                                           \_/__/
 *
 *      SDL2 sound driver.
 *
 *      By Edward Rudd.
 *
 *      See readme.txt for copyright information.
 */

#include "allegro.h"

#if (defined ALLEGRO_WITH_SDL2DIGI) && ((!defined ALLEGRO_WITH_MODULES) || (defined ALLEGRO_MODULE))

#include "allegro/internal/aintern.h"
#ifdef ALLEGRO_QNX
#include "allegro/platform/aintqnx.h"
#else
#include "allegro/platform/aintunix.h"
#endif

#ifndef SCAN_DEPEND
#include <string.h>
#include <math.h>
#include <SDL.h>
#endif

#define PREFIX_I                "al-sdl2 INFO: "
#define PREFIX_W                "al-sdl2 WARNING: "
#define PREFIX_E                "al-sdl2 ERROR: "

static int sdl2_bits, sdl2_signed, sdl2_stereo;
static unsigned int sdl2_rate;

static SDL_AudioSpec sdl2_audiospec;
static SDL_AudioDeviceID sdl2_deviceID;

static char sdl2_desc[256] = EMPTY_STRING;

static int sdl2_detect(int input);
static int sdl2_init(int input, int voices);
static void sdl2_exit(int input);
static int sdl2_set_mixer_volume(int volume);
static int sdl2_get_mixer_volume(void);
static int sdl2_buffer_size(void);

DIGI_DRIVER digi_sdl2 = {
    DIGI_SDL2,
    empty_string,
    empty_string,
    "SDL2",
    0,
    0,
    MIXER_MAX_SFX,
    MIXER_DEF_SFX,

    sdl2_detect,
    sdl2_init,
    sdl2_exit,
    sdl2_set_mixer_volume,
    sdl2_get_mixer_volume,

    NULL,
    NULL,
    sdl2_buffer_size,
    _mixer_init_voice,
    _mixer_release_voice,
    _mixer_start_voice,
    _mixer_stop_voice,
    _mixer_loop_voice,

    _mixer_get_position,
    _mixer_set_position,

    _mixer_get_volume,
    _mixer_set_volume,
    _mixer_ramp_volume,
    _mixer_stop_volume_ramp,

    _mixer_get_frequency,
    _mixer_set_frequency,
    _mixer_sweep_frequency,
    _mixer_stop_frequency_sweep,

    _mixer_get_pan,
    _mixer_set_pan,
    _mixer_sweep_pan,
    _mixer_stop_pan_sweep,

    _mixer_set_echo,
    _mixer_set_tremolo,
    _mixer_set_vibrato,
    0, 0,
    0,
    0,
    0,
    0,
    0,
    0
};

#define DEFAULT_BUFFER_SIZE   1024
#define MIN_BUFFER_SIZE       128

static int sdl2_detect(int input) {
    int ret = SDL_InitSubSystem(SDL_INIT_AUDIO) == 0 ? TRUE : FALSE;
    return ret;
}

static void sdl2_update(int threaded) {
    
}

static void sdl2_callback(void *udata, Uint8 *stream, int len) {
    if((Uint32)len == sdl2_audiospec.size) {
        _mix_some_samples((uintptr_t)stream, 0, sdl2_signed);
    } else {
        printf("ERROR!!!\n");
    }
}

static int sdl2_init(int input, int voices) {
    char tmp1[128], tmp2[128];
    SDL_AudioSpec want;

    if (input) {
        ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Input is not supported"));
        return -1;
    }

    sdl2_bits = (_sound_bits == 8) ? 8 : 16;
    sdl2_stereo = (_sound_stereo) ? 1 : 0;
    sdl2_rate = (_sound_freq > 0) ? _sound_freq : 44100;

    SDL_zero(want);
    want.freq = sdl2_rate;
    want.channels = sdl2_stereo + 1;
    want.samples = DEFAULT_BUFFER_SIZE;
    want.callback = sdl2_callback;

    if (sdl2_bits == 8) {
        want.format = AUDIO_U8;
        sdl2_signed = 0;
    } else if (sdl2_bits == 16) {
        want.format = AUDIO_S16SYS;
        sdl2_signed = 1;
    } else {
        ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Unsupported sample format"));
        return -1;
    }

    sdl2_deviceID = SDL_OpenAudioDevice(NULL, 0, &want, &sdl2_audiospec, 0);
    if (sdl2_deviceID == 0) {
        uszprintf(allegro_error, ALLEGRO_ERROR_SIZE, "Failed to open audio: %s", SDL_GetError());
        return -1;
    }

    digi_driver->voices = voices;

    if (_mixer_init(sdl2_audiospec.samples * (sdl2_stereo ? 2 : 1), sdl2_rate,
            sdl2_stereo, ((sdl2_bits == 16) ? 1 : 0),
            &digi_driver->voices) != 0) {
        ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Can not init software mixer"));
        SDL_CloseAudioDevice(sdl2_deviceID);
        sdl2_deviceID = 0;
        return -1;
    }

    /* Add audio interrupt. */
    _unix_bg_man->register_func(sdl2_update);

    uszprintf(sdl2_desc, sizeof (sdl2_desc),
            get_config_text
            ("SDL2: %d bits, %s, %d bps, %s"),
            sdl2_bits,
            uconvert_ascii((sdl2_signed ? "signed" : "unsigned"), tmp1),
            sdl2_rate, uconvert_ascii((sdl2_stereo ? "stereo" : "mono"), tmp2));

    digi_driver->desc = sdl2_desc;

    SDL_PauseAudioDevice(sdl2_deviceID, 0);

    return 0;
}

static void sdl2_exit(int input) {
    if (input)
        return;

    if (sdl2_deviceID > 0) {
        _unix_bg_man->unregister_func(sdl2_update);

        SDL_PauseAudioDevice(sdl2_deviceID, 1);

        _mixer_exit();

        SDL_CloseAudioDevice(sdl2_deviceID);
    }
}

static int sdl2_set_mixer_volume(int volume) {
    /* Not implemented */
    return 0;
}

static int sdl2_get_mixer_volume(void) {
    /* Not implemented */
    return 255;
}

static int sdl2_buffer_size(void) {
    return sdl2_audiospec.samples;
}

#ifdef ALLEGRO_MODULE

/* _module_init:
 *  Called when loaded as a dynamically linked module.
 */
void _module_init(int system_driver) {
    _unix_register_digi_driver(DIGI_SDL2, &digi_sdl2, TRUE, TRUE);
}

#endif

#endif
