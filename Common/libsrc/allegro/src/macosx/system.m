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
 *      MacOS X system driver.
 *
 *      By Angelo Mottola.
 *
 *      See readme.txt for copyright information.
 */


#include "allegro.h"
#include "allegro/internal/aintern.h"
#include "allegro/platform/aintosx.h"

#ifndef ALLEGRO_MACOSX
   #error something is wrong with the makefile
#endif

#ifndef SCAN_DEPEND
#include <CoreFoundation/CoreFoundation.h>
#endif


/* These are used to warn the dock about the application */
struct CPSProcessSerNum
{
   UInt32 lo;
   UInt32 hi;
};
extern OSErr CPSGetCurrentProcess(struct CPSProcessSerNum *psn);
extern OSErr CPSEnableForegroundOperation(struct CPSProcessSerNum *psn, UInt32 _arg2, UInt32 _arg3, UInt32 _arg4, UInt32 _arg5);
extern OSErr CPSSetFrontProcess(struct CPSProcessSerNum *psn);



static int osx_sys_init(void);
static void osx_sys_exit(void);
static void osx_sys_message(AL_CONST char *);
static void osx_sys_get_executable_name(char *, int);
static int osx_sys_find_resource(char *, AL_CONST char *, int);
static void osx_sys_set_window_title(AL_CONST char *);
static int osx_sys_set_close_button_callback(void (*proc)(void));
static int osx_sys_set_display_switch_mode(int mode);
static void osx_sys_get_gfx_safe_mode(int *driver, struct GFX_MODE *mode);
static int osx_sys_desktop_color_depth(void);
static int osx_sys_get_desktop_resolution(int *width, int *height);


/* Global variables */
int    __crt0_argc;
char **__crt0_argv;
NSBundle *osx_bundle = NULL;
void* osx_event_mutex;
NSCursor *osx_cursor = NULL;
NSCursor *osx_blank_cursor = NULL;
AllegroWindow *osx_window = NULL;
char osx_window_title[ALLEGRO_MESSAGE_SIZE];
void (*osx_window_close_hook)(void) = NULL;
int osx_gfx_mode = OSX_GFX_NONE;
int osx_emulate_mouse_buttons = FALSE;
int osx_window_first_expose = FALSE;
int osx_skip_events_processing = FALSE;
void* osx_skip_events_processing_mutex;


static RETSIGTYPE (*old_sig_abrt)(int num);
static RETSIGTYPE (*old_sig_fpe)(int num);
static RETSIGTYPE (*old_sig_ill)(int num);
static RETSIGTYPE (*old_sig_segv)(int num);
static RETSIGTYPE (*old_sig_term)(int num);
static RETSIGTYPE (*old_sig_int)(int num);
static RETSIGTYPE (*old_sig_quit)(int num);

static unsigned char *cursor_data = NULL;
static NSBitmapImageRep *cursor_rep = NULL;
static NSImage *cursor_image = NULL;


SYSTEM_DRIVER system_macosx =
{
   SYSTEM_MACOSX,
   empty_string,
   empty_string,
   "MacOS X",
   osx_sys_init,
   osx_sys_exit,
   osx_sys_get_executable_name,
   osx_sys_find_resource,
   osx_sys_set_window_title,
   osx_sys_set_close_button_callback,
   osx_sys_message,
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
   osx_sys_set_display_switch_mode,
   NULL,  /* AL_METHOD(void, display_switch_lock, (int lock, int foreground)); */
   osx_sys_desktop_color_depth,
   osx_sys_get_desktop_resolution,
   osx_sys_get_gfx_safe_mode,
   _unix_yield_timeslice,
   _unix_create_mutex,
   _unix_destroy_mutex,
   _unix_lock_mutex,
   _unix_unlock_mutex,
   NULL,  /* AL_METHOD(_DRIVER_INFO *, gfx_drivers, (void)); */
   NULL,  /* AL_METHOD(_DRIVER_INFO *, digi_drivers, (void)); */
   NULL,  /* AL_METHOD(_DRIVER_INFO *, midi_drivers, (void)); */
   NULL,  /* AL_METHOD(_DRIVER_INFO *, keyboard_drivers, (void)); */
   NULL,  /* AL_METHOD(_DRIVER_INFO *, mouse_drivers, (void)); */
   NULL,  /* AL_METHOD(_DRIVER_INFO *, joystick_drivers, (void)); */
   NULL,  /* AL_METHOD(_DRIVER_INFO *, timer_drivers, (void)); */
};



/* osx_signal_handler:
 *  Used to trap various signals, to make sure things get shut down cleanly.
 */
static RETSIGTYPE osx_signal_handler(int num)
{
   _unix_unlock_mutex(osx_event_mutex);

   allegro_exit();

   _unix_destroy_mutex(osx_event_mutex);

   fprintf(stderr, "Shutting down Allegro due to signal #%d\n", num);
   raise(num);
}


static void osx_scale_mouse(NSPointPointer position, const NSRectPointer window, const NSRectPointer view)
{
    // flip the vertical to top down
    position->y = window->size.height - position->y;
    if (!NSEqualSizes(window->size, view->size)) {
        position->x = position->x / window->size.width * view->size.width;
        position->y = position->y / window->size.height * view->size.height;
    }
}

static NSPoint locationInView (NSEvent *event, NSView *inTermsOfView) {
   NSPoint windowLocation;

   NSWindow *viewWindow = [inTermsOfView window];

   if (event) {
      NSWindow *eventWindow = [event window];

      if (eventWindow) {
         if (eventWindow == viewWindow) {
            windowLocation = [event locationInWindow];
         } else {
            // event from a completely different window.. convert to global and back to _our_ window
            NSPoint globalLocation = [eventWindow convertBaseToScreen: windowLocation];
            windowLocation = [viewWindow convertScreenToBase: globalLocation];
         }
      } else {
         // if window is nil, then it's assumed to be screen coordinates.
         NSPoint globalLocation = [event locationInWindow];
         windowLocation = [viewWindow convertScreenToBase: globalLocation ];
      }  
   } else {
      NSPoint globalLocation = [NSEvent mouseLocation];
      windowLocation = [viewWindow convertScreenToBase: globalLocation ];
   }

   return [inTermsOfView convertPoint: windowLocation fromView: nil];
}


static BOOL cursor_hidden = NO;  // need to keep track because nscursor requires balanced hide/unhide
static int dx = 0, dy = 0, dz = 0;
static int mx=0;
static int my=0;
static int buttons = 0;

void update_cursor() {
   if (!_mouse_installed) { return; }
   if (!_mouse_on) { return; }

   NSPoint point;
   NSRect frame, view;
   view = NSMakeRect(0, 0, gfx_driver->w, gfx_driver->h);

   point = NSMakePoint(0, 0);
   if (osx_window) {
      frame = [[osx_window contentView] frame];
      point = locationInView (nil, [osx_window contentView]);
   } else {
      frame = [[NSScreen mainScreen] frame];
   }
   osx_scale_mouse(&point, &frame, &view);

   if (NSPointInRect(point, view)) {

      // hide the cursor because sometimes osx will reshow cursor for no reason
      if (!cursor_hidden) {
         [NSCursor hide];
         cursor_hidden = YES;
      } else {
         // if dock is shown, having the mouse near the edge might show the cursor, so unhide/hide
         [NSCursor unhide];
         [NSCursor hide];
      }
      }
}

void osx_mouse_available() 
{
   if (!_mouse_installed) { return; }

   NSPoint point;
   NSRect frame, view;
   view = NSMakeRect(0, 0, gfx_driver->w, gfx_driver->h);

   point = NSMakePoint(0, 0);
   if (osx_window) {
      frame = [[osx_window contentView] frame];
      point = locationInView (nil, [osx_window contentView]);
   } else {
      frame = [[NSScreen mainScreen] frame];
   }
   osx_scale_mouse(&point, &frame, &view);

   if (NSPointInRect(point, view)) {

      mx = point.x;
      my = point.y;
      buttons = 0;
      _mouse_on = TRUE;

      update_cursor();

      osx_mouse_handler(mx, my, dx, dy, dz, buttons);
   }
}

void osx_mouse_not_available()
{
   if (!_mouse_installed) { return; }

   if (cursor_hidden) {
      [NSCursor unhide];
      cursor_hidden = NO;
   }

   _mouse_on = FALSE;

   osx_mouse_handler(mx, my, dx, dy, dz, buttons);
}

void osx_add_event_monitor() {

   [NSEvent addLocalMonitorForEventsMatchingMask:NSAnyEventMask handler:^NSEvent * (NSEvent *event) {

   NSDate *distant_past = [NSDate distantPast];
   NSPoint point;
   NSRect frame, view;
   dx = 0, dy = 0, dz = 0;
   mx=_mouse_x;
   my=_mouse_y;
   int event_type;
   BOOL gotmouseevent = NO;
   static int alt_left_mouse_up_event = -1;

   _unix_lock_mutex(osx_skip_events_processing_mutex);
   int skip_events_processing = osx_skip_events_processing;
   _unix_unlock_mutex(osx_skip_events_processing_mutex);

   if ((skip_events_processing) || (osx_gfx_mode == OSX_GFX_NONE)) {
      return event;
   }

   view = NSMakeRect(0, 0, gfx_driver->w, gfx_driver->h);

   point = NSMakePoint(0, 0);
   if (osx_window) {
      frame = [[osx_window contentView] frame];
      point = locationInView (event, [osx_window contentView]);
   } else {
      frame = [[NSScreen mainScreen] frame];
   }
   osx_scale_mouse(&point, &frame, &view);

   event_type = [event type];
   switch (event_type) {

      case NSKeyDown:
         if (_keyboard_installed) {
            osx_keyboard_handler(TRUE, event);
         }
         if (! ([event modifierFlags] & NSCommandKeyMask) )
         {
            event = nil;
         }
         break;


      case NSKeyUp:
         if (_keyboard_installed) {
            osx_keyboard_handler(FALSE, event);
         }
         if (! ([event modifierFlags] & NSCommandKeyMask) )
         {
            event = nil;
         }
         break;

      case NSFlagsChanged:
         if (_keyboard_installed) {
            osx_keyboard_modifiers([event modifierFlags]);
         }
         event = nil;
         break;

      case NSLeftMouseDown:
      case NSOtherMouseDown:
      case NSRightMouseDown:
         if (![NSApp isActive]) {
            /* App is regaining focus */
            if (_mouse_installed) {
               if ((osx_window) && (NSPointInRect(point, view))) {
                  mx = point.x;
                  my = point.y;
                  buttons = 0;
                  alt_left_mouse_up_event = -1;
                  _mouse_on = TRUE;
               }
            }
            if (_keyboard_installed)
               osx_keyboard_focused(TRUE, 0);
            _switch_in();
            gotmouseevent = YES;
            break;
         }
         /* fallthrough */
      case NSLeftMouseUp:
      case NSOtherMouseUp:
      case NSRightMouseUp:
         {
            int mouse_event_type = event_type;

            if (mouse_event_type == NSLeftMouseDown) {
               //  TODO: if mouse down but hasn't clearned alt-mouse.. keep with the alt.
               if (osx_emulate_mouse_buttons) {
                  if (key[KEY_ALT]) {
                     mouse_event_type = NSOtherMouseDown;
                     alt_left_mouse_up_event = NSOtherMouseUp;
                  }
                  if (key[KEY_LCONTROL]) {
                     mouse_event_type = NSRightMouseDown;
                     alt_left_mouse_up_event = NSRightMouseUp;
                  }
               }
            }

            if ((mouse_event_type == NSLeftMouseUp) && (alt_left_mouse_up_event >= 0)) {
               mouse_event_type = alt_left_mouse_up_event;
               alt_left_mouse_up_event = -1;
            }

            if ((!osx_window) || (NSPointInRect(point, view))) {
               /* Deliver mouse downs only if cursor is on the window */
               buttons |= ((mouse_event_type == NSLeftMouseDown) ? 0x1 : 0);
               buttons |= ((mouse_event_type == NSRightMouseDown) ? 0x2 : 0);
               buttons |= ((mouse_event_type == NSOtherMouseDown) ? 0x4 : 0);
            }
            buttons &= ~((mouse_event_type == NSLeftMouseUp) ? 0x1 : 0);
            buttons &= ~((mouse_event_type == NSRightMouseUp) ? 0x2 : 0);
            buttons &= ~((mouse_event_type == NSOtherMouseUp) ? 0x4 : 0);

            gotmouseevent = YES;
         }
         break;

      case NSLeftMouseDragged:
      case NSRightMouseDragged:
      case NSOtherMouseDragged:
      case NSMouseMoved:

         // a bit dodgy but hide mouse cursor 
         update_cursor();

         dx += [event deltaX];
         dy += [event deltaY];

         mx=point.x;
         my=point.y;

         gotmouseevent = YES;
         break;

      case NSScrollWheel:
         dz += [event deltaY];
         gotmouseevent = YES;
         break;

      case NSMouseEntered:
         if (([event trackingNumber] == osx_mouse_tracking_rect) && ([NSApp isActive])) {
            osx_mouse_available();
            return event;  // return immediately since osx_mouse_available will call handler.
         }
         break;

      case NSMouseExited:
         if ([event trackingNumber] == osx_mouse_tracking_rect) {
            osx_mouse_not_available();
            return event;  // return immediately since osx_mouse_available will call handler.
         }
         break;

      default:
         break;
   }

   if (gotmouseevent == YES) {
      osx_mouse_handler(mx, my, dx, dy, dz, buttons);
   }

   return event;

   }];

}

/* osx_sys_init:
 *  Initalizes the MacOS X system driver.
 */
static int osx_sys_init(void)
{
   long result;

   /* Install emergency-exit signal handlers */
   old_sig_abrt = signal(SIGABRT, osx_signal_handler);
   old_sig_fpe  = signal(SIGFPE,  osx_signal_handler);
   old_sig_ill  = signal(SIGILL,  osx_signal_handler);
   old_sig_segv = signal(SIGSEGV, osx_signal_handler);
   old_sig_term = signal(SIGTERM, osx_signal_handler);
   old_sig_int  = signal(SIGINT,  osx_signal_handler);
   old_sig_quit = signal(SIGQUIT, osx_signal_handler);


   /* Setup OS type & version */
   os_type = OSTYPE_MACOSX;
   Gestalt(gestaltSystemVersion, &result);
   os_version = (((result >> 12) & 0xf) * 10) + ((result >> 8) & 0xf);
   os_revision = (result >> 4) & 0xf;
   os_multitasking = TRUE;

   /* Setup a blank cursor */
   cursor_data = calloc(1, 16 * 16 * 4);
   cursor_rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes: &cursor_data
      pixelsWide: 16
      pixelsHigh: 16
      bitsPerSample: 8
      samplesPerPixel: 4
      hasAlpha: YES
      isPlanar: NO
      colorSpaceName: NSDeviceRGBColorSpace
      bytesPerRow: 64
      bitsPerPixel: 32];
   cursor_image = [[NSImage alloc] initWithSize: NSMakeSize(16, 16)];
   [cursor_image addRepresentation: cursor_rep];
   osx_blank_cursor = [[NSCursor alloc] initWithImage: cursor_image
      hotSpot: NSMakePoint(0, 0)];
   osx_cursor = osx_blank_cursor;
   
   osx_gfx_mode = OSX_GFX_NONE;
   
   set_display_switch_mode(SWITCH_BACKGROUND);
   set_window_title([[[NSProcessInfo processInfo] processName] cString]);
   
   return 0;
}



/* osx_sys_exit:
 *  Shuts down the system driver.
 */
static void osx_sys_exit(void)
{
   signal(SIGABRT, old_sig_abrt);
   signal(SIGFPE,  old_sig_fpe);
   signal(SIGILL,  old_sig_ill);
   signal(SIGSEGV, old_sig_segv);
   signal(SIGTERM, old_sig_term);
   signal(SIGINT,  old_sig_int);
   signal(SIGQUIT, old_sig_quit);
   
   if (osx_blank_cursor)
      [osx_blank_cursor release];
   if (cursor_image)
      [cursor_image release];
   if (cursor_rep)
      [cursor_rep release];
   if (cursor_data)
      free(cursor_data);
   osx_cursor = NULL;
   cursor_image = NULL;
   cursor_rep = NULL;
   cursor_data = NULL;
}



/* osx_sys_get_executable_name:
 *  Returns the full path to the application executable name. Note that if the
 *  exe is inside a bundle, this returns the full path of the bundle.
 */
static void osx_sys_get_executable_name(char *output, int size)
{
   if (osx_bundle)
      do_uconvert([[osx_bundle bundlePath] lossyCString], U_ASCII, output, U_CURRENT, size);
   else
      do_uconvert(__crt0_argv[0], U_ASCII, output, U_CURRENT, size);
}



/* osx_sys_find_resource:
 *  Searches the resource in the bundle resource path if the app is in a
 *  bundle, otherwise calls the unix resource finder routine.
 */
static int osx_sys_find_resource(char *dest, AL_CONST char *resource, int size)
{
   const char *path;
   char buf[256], tmp[256];
   
   if (osx_bundle) {
      path = [[osx_bundle resourcePath] cString];
      append_filename(buf, uconvert_ascii(path, tmp), resource, sizeof(buf));
      if (exists(buf)) {
         ustrzcpy(dest, size, buf);
	 return 0;
      }
   }
   return _unix_find_resource(dest, resource, size);
}



/* osx_sys_message:
 *  Displays a message using an alert panel.
 */
static void osx_sys_message(AL_CONST char *msg)
{
   char tmp[ALLEGRO_MESSAGE_SIZE];
   NSString *ns_title, *ns_msg;
   
   fputs(uconvert_toascii(msg, tmp), stderr);
   
   do_uconvert(msg, U_CURRENT, tmp, U_UTF8, ALLEGRO_MESSAGE_SIZE);
   ns_title = [NSString stringWithUTF8String: osx_window_title];
   ns_msg = [NSString stringWithUTF8String: tmp];

   NSRunAlertPanel(ns_title, ns_msg, nil, nil, nil);
}



/* osx_sys_set_window_title:
 *  Sets the title for both the application menu and the window if present.
 */
static void osx_sys_set_window_title(AL_CONST char *title)
{
   char tmp[ALLEGRO_MESSAGE_SIZE];
   
   if (osx_window_title != title)
      _al_sane_strncpy(osx_window_title, title, ALLEGRO_MESSAGE_SIZE);
   do_uconvert(title, U_CURRENT, tmp, U_UTF8, ALLEGRO_MESSAGE_SIZE);

   NSString *ns_title = [NSString stringWithUTF8String: tmp];
   
   if (osx_window)
      [osx_window setTitle: ns_title];
}



/* osx_sys_set_close_button_callback:
 *  Sets the window close callback. Also used when user hits Command-Q or
 *  selects "Quit" from the application menu.
 */
static int osx_sys_set_close_button_callback(void (*proc)(void))
{
   osx_window_close_hook = proc;
   return 0;
}



/* osx_sys_set_display_switch_mode:
 *  Sets the current display switch mode.
 */
static int osx_sys_set_display_switch_mode(int mode)
{
   if (mode != SWITCH_BACKGROUND)
      return -1;   
   return 0;
}



/* osx_sys_get_gfx_safe_mode:
 *  Defines the safe graphics mode for this system.
 */
static void osx_sys_get_gfx_safe_mode(int *driver, struct GFX_MODE *mode)
{
#ifdef ENABLE_QUICKDRAW
   *driver = GFX_QUARTZ_WINDOW;
   mode->width = 320;
   mode->height = 200;
   mode->bpp = 8;
#else
   *driver = GFX_COCOAGL_WINDOW;
   mode->width = 640;
   mode->height = 480;
   mode->bpp = 32;
#endif
}



/* osx_sys_desktop_color_depth:
 *  Queries the desktop color depth.
 */
static int osx_sys_desktop_color_depth(void)
{
   int color_depth;
   
   NSWindowDepth depth = [[NSScreen mainScreen] depth];
   color_depth = NSBitsPerPixelFromDepth(depth);
   
   return color_depth == 16 ? 15 : color_depth;
}


/* osx_sys_get_desktop_resolution:
 *  Queries the desktop resolution.
 */
static int osx_sys_get_desktop_resolution(int *width, int *height)
{
   NSRect r = [[NSScreen mainScreen] frame];
   *width = r.size.width;
   *height = r.size.height;

    return 0;
}
