#include "allegro.h"
#include "allegro/internal/aintern.h"

#include "SDL.h"
#include <assert.h>

const int DEFAULT_DISPLAY_INDEX = 0;

static void (*_on_close_callback)(void) = 0;

// ----------------------------------------------------------------------------
// EVENTS!
// ----------------------------------------------------------------------------

static void on_window_properties_changed();

void sdl2_process_single_event (SDL_Event *event) {
   switch (event->type) {
      case SDL_QUIT:
         if (_on_close_callback) {
            _on_close_callback();
         }
         break;
      case SDL_WINDOWEVENT:
         if (event->window.event == SDL_WINDOWEVENT_MOVED) {
            on_window_properties_changed();
         }
         break;
   }
}

void sdl2_process_all_pending_events (void) {
   SDL_Event event;
   while (SDL_PollEvent(&event)) {
      sdl2_process_single_event(&event);
   }
}


// ----------------------------------------------------------------------------
// SYSTEM
// ----------------------------------------------------------------------------

static int trace_handler(AL_CONST char *msg) {
   SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%s", msg);
   return 0;
}

static int sdl2_sys_init(void)
{
   SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);

   if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER|SDL_INIT_EVENTS|SDL_INIT_GAMECONTROLLER) != 0) {
      SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
      return -1;
   }

   register_trace_handler(trace_handler);
   
   os_type = OSTYPE_SDL2;
   os_version = -1; 
   os_revision = -1;
   os_multitasking = TRUE;
   
   return 0;
}

static void sdl2_sys_exit(void)
{
   SDL_Quit();
}

int _unix_find_resource(char *dest, AL_CONST char *resource, int size);

static int sdl2_sys_find_resource(char *dest, AL_CONST char *resource, int size)
{
   char buf[4096], tmp[4096];
   
   char *base_path = SDL_GetBasePath();
   if (base_path) {
      append_filename(buf, uconvert_ascii(base_path, tmp), resource, sizeof(buf));
      SDL_free(base_path);
      if (exists(buf)) {
         ustrzcpy(dest, size, buf);
	      return 0;
      }
   }

   /* char *pref_path = SDL_GetPrefPath();
   if (pref_path) {
      append_filename(buf, uconvert_ascii(pref_path, tmp), resource, sizeof(buf));
      SDL_free(pref_path);
      if (exists(buf)) {
         ustrzcpy(dest, size, buf);
	      return 0;
      }
   } */

#ifdef ALLEGRO_MSVC_SDL2
   return -1; 
#else
   return _unix_find_resource(dest, resource, size);
#endif
}


static void sdl2_gfx_set_window_title(AL_CONST char *title);

static int sdl2_sys_set_close_button_callback(void (*proc)(void)) {
   _on_close_callback = proc;
   return 0;
}

static void sdl2_sys_message(AL_CONST char *msg) {
   SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", msg, NULL);
}
 
static int sdl2_sys_get_desktop_resolution(int *width, int *height) {
   SDL_Rect r;
   if (SDL_GetDisplayBounds(DEFAULT_DISPLAY_INDEX, &r) != 0) {
      SDL_Log("SDL_GetDisplayBounds failed: %s", SDL_GetError());
      return -1;
   }
   *width = r.w;
   *height = r.h;
   return 0;
}

void *sdl2_create_mutex(void) {
   void *result = SDL_CreateMutex();
   if (result == NULL) {
      SDL_Log("SDL_CreateMutex failed: %s", SDL_GetError());
   }
   return result;
}

void sdl2_destroy_mutex(void *handle) {
   SDL_DestroyMutex(handle);
}

void sdl2_lock_mutex(void *handle) {
   SDL_LockMutex(handle);
}

void sdl2_unlock_mutex(void *handle) {
   SDL_UnlockMutex(handle);
}

SYSTEM_DRIVER system_sdl2 =
{
   .id = SYSTEM_SDL2,
   .name = empty_string,
   .desc = empty_string,
   .ascii_name = "MacOS X",

   .init = sdl2_sys_init,
   .exit = sdl2_sys_exit,

   .find_resource = sdl2_sys_find_resource,

   .set_window_title = sdl2_gfx_set_window_title,
   .set_close_button_callback = sdl2_sys_set_close_button_callback,

   .message = sdl2_sys_message,

   .get_desktop_resolution = sdl2_sys_get_desktop_resolution,

   .create_mutex = sdl2_create_mutex,
   .destroy_mutex = sdl2_destroy_mutex,
   .lock_mutex = sdl2_lock_mutex,
   .unlock_mutex = sdl2_unlock_mutex,
};


// ----------------------------------------------------------------------------
// TIMER
// ----------------------------------------------------------------------------

#define TIMER_TO_MSEC(x)  ((long)( ((x) / 1193.1810) + 0.5))
// #define MSEC_TO_TIMER(x)  ((long)((x) * (TIMERS_PER_SECOND / 1000.)))

static SDL_TimerID my_timer_id;

Uint32 sdl2_timerint(Uint32 interval_ms, void *param)
{
   int interval_timer = _handle_timer_tick(MSEC_TO_TIMER(interval_ms));
   int result = TIMER_TO_MSEC(interval_timer);
   if (result <= 0) { result = 1; }  // has to be at least 1 or the sdl2 timer will stop
   return result;
}

static int sdl2_timer_init(void)
{
   my_timer_id = SDL_AddTimer(25, sdl2_timerint, NULL);
   return 0;
}

static void sdl2_timer_exit(void)
{
   SDL_RemoveTimer(my_timer_id);
}

void sdl2_rest(unsigned int ms, void (*callback) (void)) {
   // callback ignored
   assert(callback == NULL);
   SDL_Delay(ms);
}

TIMER_DRIVER timer_sdl2 =
{
   TIMER_SDL2,
   empty_string,
   empty_string,
   "SDL2 timer",
   sdl2_timer_init,
   sdl2_timer_exit,
   NULL, NULL,		/* install_int, remove_int */
   NULL, NULL,		/* install_param_int, remove_param_int */
   NULL, NULL,		/* can_simulate_retrace, simulate_retrace */
   sdl2_rest		/* rest */
};


#if 0

// ----------------------------------------------------------------------------
// AUDIO
// ----------------------------------------------------------------------------

static SDL_AudioDeviceID dev;

static int digi_sdl2_detect(int input)
{
   if (input) {
      return 0;
   }
   return 1;
}

void MyAudioCallback(void*  userdata, Uint8* stream, int len) 
{
   _mix_some_samples((uintptr_t)stream, 0, TRUE);
}

static int digi_sdl2_init(int input, int voices) {
   if (input) {
      ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Input is not supported"));
      return -1;
   }

   digi_sdl2.voices = voices;

   SDL_AudioSpec want, have;

   SDL_memset(&want, 0, sizeof(want)); /* or SDL_zero(want) */
   want.freq = 48000;
   want.format = AUDIO_S16SYS;
   want.channels = 2;
   want.samples = 2048;  // number of frames, not samples
   want.callback = MyAudioCallback; 

   dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);

   if (_mixer_init(have.samples * have.channels, have.freq, TRUE, 1, &digi_sdl2.voices) != 0) {
      return -1;
   }

   SDL_PauseAudioDevice(dev, 0);

   return 0;
}

static void digi_sdl2_exit(int input) {
   SDL_CloseAudioDevice(dev);
}


DIGI_DRIVER digi_sdl2 =
{
   0,
   empty_string,
   empty_string,
   empty_string,
   0,              // available voices
   0,              // voice number offset
   MIXER_MAX_SFX,  // maximum voices we can support
   MIXER_DEF_SFX,  // default number of voices to use

   /* setup routines */
   digi_sdl2_detect,
   digi_sdl2_init,
   digi_sdl2_exit,
   NULL, // digi_sdl2_set_mixer_volume,
   NULL, // digi_sdl2_get_mixer_volume,

   /* audiostream locking functions */
   NULL,  // AL_METHOD(void *, lock_voice, (int voice, int start, int end));
   NULL,  // AL_METHOD(void, unlock_voice, (int voice));
   NULL, // digi_sdl2_buffer_size,

   /* voice control functions */
   _mixer_init_voice,
   _mixer_release_voice,
   _mixer_start_voice,
   _mixer_stop_voice,
   _mixer_loop_voice,

   /* position control functions */
   _mixer_get_position,
   _mixer_set_position,

   /* volume control functions */
   _mixer_get_volume,
   _mixer_set_volume,
   _mixer_ramp_volume,
   _mixer_stop_volume_ramp,

   /* pitch control functions */
   _mixer_get_frequency,
   _mixer_set_frequency,
   _mixer_sweep_frequency,
   _mixer_stop_frequency_sweep,

   /* pan control functions */
   _mixer_get_pan,
   _mixer_set_pan,
   _mixer_sweep_pan,
   _mixer_stop_pan_sweep,

   /* effect control functions */
   _mixer_set_echo,
   _mixer_set_tremolo,
   _mixer_set_vibrato,

   /* input functions */
   0,     // int rec_cap_bits;
   0,     // int rec_cap_stereo;
   NULL,  // AL_METHOD(int,  rec_cap_rate, (int bits, int stereo));
   NULL,  // AL_METHOD(int,  rec_cap_parm, (int rate, int bits, int stereo));
   NULL,  // AL_METHOD(int,  rec_source, (int source));
   NULL,  // AL_METHOD(int,  rec_start, (int rate, int bits, int stereo));
   NULL,  // AL_METHOD(void, rec_stop, (void));
   NULL   // AL_METHOD(int,  rec_read, (void *buf));
};

#endif


// ----------------------------------------------------------------------------
// GRAPHICS
// ----------------------------------------------------------------------------

static BITMAP * gfx_backing_bitmap = NULL;
static BITMAP * gfx_display_bitmap = NULL;
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *screenTex = NULL;
static const char * window_title = "AGS";

static BITMAP *gfx_sdl2_init_driver(GFX_DRIVER *drv, int w, int h, int v_w, int v_h, int color_depth) {

   switch(color_depth) {
      case 8:
      case 16:
      case 32:
         break;
      default:
         ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Unsupported color depth"));
         return NULL;
   }

   gfx_display_bitmap = create_bitmap_ex(32, w, h);
   if (color_depth != 32) {
      gfx_backing_bitmap = create_bitmap_ex(color_depth, w, h);
   }

   Uint32 flags = SDL_WINDOW_RESIZABLE;
   if (!drv->windowed) {
      flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
   }

   window = SDL_CreateWindow(
        window_title,                  // window title
        SDL_WINDOWPOS_CENTERED_DISPLAY(DEFAULT_DISPLAY_INDEX),           // initial x position
        SDL_WINDOWPOS_CENTERED_DISPLAY(DEFAULT_DISPLAY_INDEX),           // initial y position
        w,                               // width, in pixels
        h,                               // height, in pixels
        flags                 // flags 
    );

   renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);

   SDL_RendererInfo rinfo = {0};
   if (SDL_GetRendererInfo(renderer, &rinfo) == 0) {
      TRACE("Created Renderer: %s\n", rinfo.name);
      TRACE("Available texture formats:\n");
      for (int i = 0; i < rinfo.num_texture_formats; i++) {
         TRACE(" - %s\n", SDL_GetPixelFormatName(rinfo.texture_formats[i]));
      }
   }

   SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
   // SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");  // make the scaled rendering look smoother.
   SDL_RenderSetLogicalSize(renderer, w, h);

   screenTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
   
   /*
   SDL_PIXELFORMAT_ARGB8888:
   *Amask = masks[0] = 0xFF000000;   24
   *Rmask = masks[1] = 0x00FF0000;   16  
   *Gmask = masks[2] = 0x0000FF00;   8
   *Bmask = masks[3] = 0x000000FF;   0 
   */

   _rgb_r_shift_15 = 10;
   _rgb_g_shift_15 = 5;
   _rgb_b_shift_15 = 0;

   _rgb_r_shift_16 = 11;
   _rgb_g_shift_16 = 5;
   _rgb_b_shift_16 = 0;

   _rgb_r_shift_24 = 16;
   _rgb_g_shift_24 = 8;
   _rgb_b_shift_24 = 0;

   _rgb_a_shift_32 = 24;
   _rgb_r_shift_32 = 16; 
   _rgb_g_shift_32 = 8; 
   _rgb_b_shift_32 = 0;

   drv->w = w;
   drv->h = h;

   if (gfx_backing_bitmap) {
      return gfx_backing_bitmap;
   }
   return gfx_display_bitmap;
}


static BITMAP *gfx_sdl2_init_window(int w, int h, int v_w, int v_h, int color_depth) {
   return gfx_sdl2_init_driver(&gfx_sdl2_window, w, h, v_w, v_h, color_depth);
}

static BITMAP *gfx_sdl2_init_fullscreen(int w, int h, int v_w, int v_h, int color_depth) {
   return gfx_sdl2_init_driver(&gfx_sdl2_fullscreen, w, h, v_w, v_h, color_depth);
}


static void gfx_sdl2_exit(BITMAP *bmp) {
   SDL_DestroyTexture(screenTex);
   SDL_DestroyRenderer(renderer);
   SDL_DestroyWindow(window);
}

static void sdl2_gfx_set_window_title(AL_CONST char *title) {
   window_title = title;
   if (window) {
      SDL_SetWindowTitle(window, window_title);
   }
}

static Uint32 _sdl2_palette[256];


void sdl2_present_screen() {

   if (!renderer) { return; }
   if (!gfx_display_bitmap) { return; }

   SDL_RenderClear(renderer);

   /*unsigned char * pixels;
   int pitch;

   SDL_LockTexture(screenTex, NULL, (void **)&pixels, &pitch);

   for (int i = 0; i < gfx_display_bitmap->h; i++) {
      memcpy(pixels, gfx_display_bitmap->line[i], pitch);
      pixels += pitch;
   }

   SDL_UnlockTexture(screenTex); */

   if (gfx_backing_bitmap) {
      if (_color_depth == 8) {
         int size = gfx_backing_bitmap->h * gfx_backing_bitmap->w;
         Uint8 *src = gfx_backing_bitmap->line[0];
         Uint32 *dest = gfx_display_bitmap->line[0];
         for (int i=0; i < size; i++) {
            *dest = _sdl2_palette[*src];
            src++;
            dest++;
         }
      } else {
         blit(gfx_backing_bitmap, gfx_display_bitmap, 0, 0, 0, 0, gfx_backing_bitmap->w, gfx_backing_bitmap->h);
      } 
   }

   SDL_UpdateTexture(screenTex, NULL, gfx_display_bitmap->line[0], gfx_display_bitmap->w * sizeof (Uint32));

   SDL_RenderCopy(renderer, screenTex, NULL, NULL);
   SDL_RenderPresent(renderer);
}

static void on_window_properties_changed() {
   if (!renderer) { return; }
   if (!gfx_display_bitmap) { return; }

   SDL_RenderSetLogicalSize(renderer, gfx_display_bitmap->w, gfx_display_bitmap->h);
}

static void gfx_sdl2_set_palette(AL_CONST struct RGB *p, int from, int to, int retracesync) {
   for (int i=from; i<=to; i++) {
      RGB c = p[i];
      _sdl2_palette[i] = (_rgb_scale_6[c.r] << 16) | (_rgb_scale_6[c.g] << 8) | (_rgb_scale_6[c.b]);
   }
   sdl2_present_screen();
}

void sdl2_toggle_fullscreen() {

   int isFullscreen = SDL_GetWindowFlags(window) & (SDL_WINDOW_FULLSCREEN|SDL_WINDOW_FULLSCREEN_DESKTOP);

   int result = 0;
   if (isFullscreen) {
      result = SDL_SetWindowFullscreen(window, 0);
   } else {
      result = SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
   }
   if (result != 0) {
      SDL_Log("Unable to toggle fullscreen: %s", SDL_GetError());
   }
}

static int gfx_sdl2_show_mouse(struct BITMAP *bmp, int x, int y) {
   SDL_ShowCursor(SDL_ENABLE);
   return -1; // show cursor, but "fail" because we're not supporting hardware cursors
}

static void gfx_sdl2_hide_mouse() {
   SDL_ShowCursor(SDL_DISABLE);
}

static GFX_MODE_LIST *gfx_sdl2_window_fetch_mode_list (void) {
   SDL_Rect rect;
#if SDL_VERSION_ATLEAST(2, 0, 5)
   int result = SDL_GetDisplayUsableBounds(DEFAULT_DISPLAY_INDEX, &rect);
#else
   int result = SDL_GetDisplayBounds(DEFAULT_DISPLAY_INDEX, &rect);
#endif
   if (result != 0) { return NULL; }

   GFX_MODE_LIST *mode_list = _AL_MALLOC(sizeof(GFX_MODE_LIST));
   mode_list->num_modes = 1;
   mode_list->mode = _AL_MALLOC(sizeof(GFX_MODE)*2);
   mode_list->mode[0] = (GFX_MODE){
      .width = rect.w,
      .height = rect.h,
      .bpp = 32
   };
   mode_list->mode[1] = (GFX_MODE){0};
   return mode_list;
}

static GFX_MODE_LIST *gfx_sdl2_fullscreen_fetch_mode_list (void) {
   SDL_Rect rect;
   int result = SDL_GetDisplayBounds(DEFAULT_DISPLAY_INDEX, &rect);
   if (result != 0) { return NULL; }

   GFX_MODE_LIST *mode_list = _AL_MALLOC(sizeof(GFX_MODE_LIST));
   mode_list->num_modes = 1;
   mode_list->mode = _AL_MALLOC(sizeof(GFX_MODE)*2);
   mode_list->mode[0] = (GFX_MODE){
      .width = rect.w,
      .height = rect.h,
      .bpp = 32
   };
   mode_list->mode[1] = (GFX_MODE){0};
   return mode_list;
}

GFX_DRIVER gfx_sdl2_window =
{
   .id = GFX_SDL2_WINDOW,
   .name = empty_string,
   .desc = empty_string,
   .ascii_name = "SDL2 Windowed",
   .init = gfx_sdl2_init_window,
   .exit = gfx_sdl2_exit,
   //.vsync = gfx_sdl2_vsync,   
   .set_palette = gfx_sdl2_set_palette,          
   // .create_video_bitmap = NULL, //gfx_sdl2_create_video_bitmap,
   // .destroy_video_bitmap = NULL, // gfx_sdl2_destroy_video_bitmap,
   // .set_mouse_sprite = NULL, // gfx_sdl2_set_mouse_sprite,
   .show_mouse = gfx_sdl2_show_mouse, // gfx_sdl2_show_mouse,
   .hide_mouse = gfx_sdl2_hide_mouse, // gfx_sdl2_hide_mouse,
   // .move_mouse = NULL, // gfx_sdl2_move_mouse,
   .fetch_mode_list = gfx_sdl2_window_fetch_mode_list,
   .linear = TRUE,
   .windowed = TRUE                   
};

GFX_DRIVER gfx_sdl2_fullscreen =
{
   .id = GFX_SDL2_FULLSCREEN,
   .name = empty_string,
   .desc = empty_string,
   .ascii_name = "SDL2 Fullscreen",
   .init = gfx_sdl2_init_fullscreen,
   .exit = gfx_sdl2_exit,
   //.vsync = gfx_sdl2_vsync,             
   .set_palette = gfx_sdl2_set_palette,          
   // .create_video_bitmap = NULL, //gfx_sdl2_create_video_bitmap,
   // .destroy_video_bitmap = NULL, // gfx_sdl2_destroy_video_bitmap,
   // .set_mouse_sprite = NULL, // gfx_sdl2_set_mouse_sprite,
   .show_mouse = gfx_sdl2_show_mouse, // gfx_sdl2_show_mouse,
   .hide_mouse = gfx_sdl2_hide_mouse, // gfx_sdl2_hide_mouse,
   // .move_mouse = NULL, // gfx_sdl2_move_mouse,
   .fetch_mode_list = gfx_sdl2_fullscreen_fetch_mode_list,
   .linear = TRUE,
   .windowed = FALSE                   
};


// ----------------------------------------------------------------------------
// MOUSE
// ----------------------------------------------------------------------------

void sdl2_mouse_position(int x, int y) {
   if (renderer) {
      SDL_Rect viewport;
      float scalex, scaley;

      SDL_RenderGetViewport(renderer, &viewport);
      SDL_RenderGetScale(renderer, &scalex, &scaley);

      x = scalex * (viewport.x + x);
      y = scaley * (viewport.y + y);
   }

   SDL_WarpMouseInWindow(window, x, y);
}
