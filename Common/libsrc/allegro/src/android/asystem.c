/*         ______   ___    ___
 *        /\  _  \ /\_ \  /\_ \
 *        \ \ \\ \\//\ \ \//\ \      __     __   _ __   ___
 *         \ \  __ \ \ \ \  \ \ \   /'__`\ /'_ `\/\`'__\/ __`\
 *          \ \ \/\ \ \_\ \_ \_\ \_/\  __//\ \L\ \ \ \//\ \L\ \
 *           \ \_\ \_\/\____\/\____\ \____\ \____ \ \_\\ \____/
 *            \/_/\/_/\/____/\/____/\/____/\/___L\ \/_/ \/___/
 *                                           /\____/
 *                                           \_/__/
 *
 *      System driver.
 *
 *      By JJS for the Adventure Game Studio runtime port.
 *      Based on the Allegro PSP port.
 *
 *      See readme.txt for copyright information.
 */


#include "allegro.h"
#include "allegro/internal/aintern.h"
#include "allegro/platform/aintand.h"

#ifndef ALLEGRO_ANDROID
   #error something is wrong with the makefile
#endif


static int android_sys_init(void);
static void android_sys_exit(void);
static int android_get_desktop_resolution(int *width, int *height);
static void android_get_gfx_safe_mode(int *driver, struct GFX_MODE *mode);



SYSTEM_DRIVER system_android =
{
   SYSTEM_ANDROID,
   empty_string,
   empty_string,
   "Android Device",
   android_sys_init,
   android_sys_exit,
   NULL,  /* AL_METHOD(void, get_executable_name, (char *output, int size)); */
   NULL,  /* AL_METHOD(int, find_resource, (char *dest, AL_CONST char *resource, int size)); */
   NULL,  /* AL_METHOD(void, set_window_title, (AL_CONST char *name)); */
   NULL,  /* AL_METHOD(int, set_close_button_callback, (AL_METHOD(void, proc, (void)))); */
   NULL,  /* AL_METHOD(void, message, (AL_CONST char *msg)); */
   NULL,  /* AL_METHOD(void, assert, (AL_CONST char *msg)); */
   NULL,  /* AL_METHOD(void, save_console_state, (void)); */
   NULL,  /* AL_METHOD(void, restore_console_state, (void)); */
   NULL,  /* AL_METHOD(struct BITMAP *, create_bitmap, (int color_depth, int width, int height)); */
   NULL,  /* AL_METHOD(void, created_bitmap, (struct BITMAP *bmp)); */
   NULL,  /* AL_METHOD(struct BITMAP *, create_sub_bitmap, (struct BITMAP *parent, int x, int y, int width, int height)); */
   NULL,  /* AL_METHOD(void, created_sub_bitmap, (struct BITMAP *bmp, struct BITMAP *parent)); */
   NULL,  /* AL_METHOD(int, destroy_bitmap, (struct BITMAP *bitmap)); */
   NULL,  /* AL_METHOD(void, read_hardware_palette, (void)); */
   NULL,  /* AL_METHOD(void, set_palette_range, (AL_CONST struct RGB *p, int from, int to, int retracesync)); */
   NULL,  /* AL_METHOD(struct GFX_VTABLE *, get_vtable, (int color_depth)); */
   NULL,  /* AL_METHOD(int, set_display_switch_mode, (int mode)); */
   NULL,  /* AL_METHOD(void, display_switch_lock, (int lock, int foreground)); */
   NULL,  /* AL_METHOD(int, desktop_color_depth, (void)); */
   android_get_desktop_resolution,  /* AL_METHOD(int, get_desktop_resolution, (int *width, int *height)); */
   android_get_gfx_safe_mode,  /*AL_METHOD(void, get_gfx_safe_mode, (int *driver, struct GFX_MODE *mode));*/
   NULL,  /* AL_METHOD(void, yield_timeslice, (void)); */
   NULL,  /* AL_METHOD(void *, create_mutex, (void)); */
   NULL,  /* AL_METHOD(void, destroy_mutex, (void *handle)); */
   NULL,  /* AL_METHOD(void, lock_mutex, (void *handle)); */
   NULL,  /* AL_METHOD(void, unlock_mutex, (void *handle)); */
   NULL,  /* AL_METHOD(_DRIVER_INFO *, gfx_drivers, (void)); */
   NULL,  /* AL_METHOD(_DRIVER_INFO *, digi_drivers, (void)); */
   NULL,  /* AL_METHOD(_DRIVER_INFO *, midi_drivers, (void)); */
   NULL,  /* AL_METHOD(_DRIVER_INFO *, keyboard_drivers, (void)); */
   NULL,  /* AL_METHOD(_DRIVER_INFO *, mouse_drivers, (void)); */
   NULL,  /* AL_METHOD(_DRIVER_INFO *, joystick_drivers, (void)); */
   NULL   /* AL_METHOD(_DRIVER_INFO *, timer_drivers, (void)); */
};



static int android_sys_init(void)
{
   os_type = OSTYPE_ANDROID;

   return 0;
}



static void android_sys_exit(void)
{
   
}



extern unsigned int android_screen_physical_width;
extern unsigned int android_screen_physical_height;

static int android_get_desktop_resolution(int *width, int *height)
{
   *width = android_screen_physical_width;
   *height = android_screen_physical_height;
   return 0;
}



static void android_get_gfx_safe_mode(int *driver, struct GFX_MODE *mode)
{
   *driver = GFX_ANDROID;
   mode->width = 320;
   mode->height = 200;
   mode->bpp = 16;
}
