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
 *      main() function replacement for MacOS X.
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

#include "allegro/internal/alconfig.h"
#undef main


/* For compatibility with the unix code */
extern int    __crt0_argc;
extern char **__crt0_argv;
extern void *_mangled_main_address;

static char *arg0, *arg1 = NULL;

static volatile BOOL ready_to_terminate = NO;

extern void osx_add_event_monitor();

static BOOL in_bundle(void)
{
   /* This comes from the ADC tips & tricks section: how to detect if the app
    * lives inside a bundle
    */
   FSRef processRef;
   ProcessSerialNumber psn = { 0, kCurrentProcess };
   FSCatalogInfo processInfo;
   GetProcessBundleLocation(&psn, &processRef);
   FSGetCatalogInfo(&processRef, kFSCatInfoNodeFlags, &processInfo, NULL, NULL, NULL);
   if (processInfo.nodeFlags & kFSNodeIsDirectoryMask)
     return YES;
   else
     return NO;
}

@interface AllegroAppDelegate ()
@end

@implementation AllegroAppDelegate

- (BOOL)application: (NSApplication *)theApplication openFile: (NSString *)filename
{
  arg1 = strdup([filename lossyCString]);
  return YES;
}


/* applicationDidFinishLaunching:
 *  Called when the app is ready to run. This runs the system events pump and
 *  updates the app window if it exists.
 */
- (void)applicationDidFinishLaunching: (NSNotification *)aNotification
{

   CFDictionaryRef mode;
   NSString* exename, *resdir;
   NSFileManager* fm;
   BOOL isDir;

   /* create mutex */
   osx_event_mutex = _unix_create_mutex();
   osx_skip_events_processing_mutex = _unix_create_mutex();

   if (in_bundle() == YES)
   {
      /* In a bundle, so chdir to the containing directory,
       * or to the 'magic' resource directory if it exists.
       * (see the readme.osx file for more info)
       */
      osx_bundle = [NSBundle mainBundle];
      exename = [[osx_bundle executablePath] lastPathComponent];
      resdir = [[osx_bundle resourcePath] stringByAppendingPathComponent: exename];
      fm = [NSFileManager defaultManager];
      if ([fm fileExistsAtPath: resdir isDirectory: &isDir] && isDir) {
           // Yes, it exists inside the bundle
          [fm changeCurrentDirectoryPath: resdir];
      }
      else {
         /* No, change to the 'standard' OSX resource directory if it exists*/
         if ([fm fileExistsAtPath: [osx_bundle resourcePath] isDirectory: &isDir] && isDir)
         {
            [fm changeCurrentDirectoryPath: [osx_bundle resourcePath]];
         }
         /* It doesn't exist - this is unusual for a bundle. Don't chdir */
      }
      arg0 = strdup([[osx_bundle bundlePath] fileSystemRepresentation]);
      if (arg1) {
         static char *args[2];
         args[0] = arg0;
         args[1] = arg1;
         __crt0_argv = args;
         __crt0_argc = 2;
      }
      else {
         __crt0_argv = &arg0;
         __crt0_argc = 1;
      }
   }
   /* else: not in a bundle so don't chdir */

    osx_add_event_monitor();

   [NSThread detachNewThreadSelector: @selector(app_main:)
      toTarget: [AllegroAppDelegate class]
      withObject: nil];
}

/* applicationDidChangeScreenParameters:
 *  Invoked when the screen did change resolution/color depth.
 */
- (void)applicationDidChangeScreenParameters: (NSNotification *)aNotification
{
}



/* Call the user main() */
static void call_user_main(void)
{
   int (*real_main)(int, char*[]) = (int (*)(int, char*[])) _mangled_main_address;
   real_main(__crt0_argc, __crt0_argv);
   allegro_exit();
   ready_to_terminate = YES;
   runOnMainQueueWithoutDeadlocking(^{
     [NSApp terminate:nil];
   });
}

/* app_main:
 *  Thread dedicated to the user program; real main() gets called here.
 */
+ (void)app_main: (id)arg
{
  @autoreleasepool {
    call_user_main();
  }
}


/* applicationShouldTerminate:
 *  Called upon Command-Q or "Quit" menu item selection.
 *  If the window close callback is set, calls it, otherwise does
 *  not exit.
 */
- (NSApplicationTerminateReply) applicationShouldTerminate: (id)sender
{
   if (ready_to_terminate) {
      return NSTerminateNow;
   }
   else {
      if (osx_window_close_hook)
         osx_window_close_hook();
      return NSTerminateCancel;
   }
}



/* quitAction:
 *  This is connected to the Quit menu.  The menu sends this message to the
 *  delegate, rather than sending terminate: directly to NSApp, so that we get a
 *  chance to validate the menu item first.
 */
- (void) quitAction: (id) sender {
   [NSApp terminate: self];
}



/* validateMenuItem:
 *  If the user has not set a window close hook, return NO from this function so
 *  that the Quit menu item is greyed out.
 */
- (BOOL) validateMenuItem: (NSMenuItem*) item {
   if ([item action] == @selector(quitAction:)) {
      return (osx_window_close_hook == NULL) ? NO : YES;
   }
   /* Default, all other items are valid (there are none at the moment */
   return YES;
}

/* end of AllegroAppDelegate implementation */
@end



/* main:
 *  Replacement for main function.
 */
int main(int argc, char *argv[])
{
   @autoreleasepool {

   NSMenu *menu;
   NSMenuItem *menu_item, *temp_item;

   __crt0_argc = argc;
   __crt0_argv = argv;

   AllegroAppDelegate *app_delegate = [[AllegroAppDelegate alloc] init];

    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

   /* Load the main menu nib if possible */
   if ((!in_bundle()) || ([NSBundle loadNibNamed: @"MainMenu"
                                    owner: NSApp] == NO))
   {
       /* Didn't load the nib; create a default menu programmatically */
       NSString* title = nil;
       NSDictionary* app_dictionary = [[NSBundle mainBundle] infoDictionary];
       if (app_dictionary)
       {
          title = [app_dictionary objectForKey: @"CFBundleName"];
       }
       if (title == nil)
       {
          title = [[NSProcessInfo processInfo] processName];
       }
       [NSApp setMainMenu: [[NSMenu allocWithZone: [NSMenu menuZone]] initWithTitle: @"temp"]];
       menu = [[NSMenu allocWithZone: [NSMenu menuZone]] initWithTitle: @"temp"];
       temp_item = [[NSMenuItem allocWithZone: [NSMenu menuZone]]
                     initWithTitle: @"temp"
                     action: NULL
                     keyEquivalent: @""];
       [[NSApp mainMenu] addItem: temp_item];
       [[NSApp mainMenu] setSubmenu: menu forItem: temp_item];
       [NSApp setAppleMenu: menu];
       NSString *quit = [@"Quit " stringByAppendingString: title];
       menu_item = [[NSMenuItem allocWithZone: [NSMenu menuZone]]
                     initWithTitle: quit
                     action: @selector(quitAction:)
                     keyEquivalent: @"q"];
       [menu_item setKeyEquivalentModifierMask: NSCommandKeyMask];
       [menu_item setTarget: app_delegate];
       [menu addItem: menu_item];
   }

   [NSApp setDelegate: app_delegate];

   [NSApp run];
   /* Can never get here */

   return 0;
   }
}

/* Local variables:       */
/* c-basic-offset: 3      */
/* indent-tabs-mode: nil  */
/* c-file-style: "linux" */
/* End:                   */
