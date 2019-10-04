#include "allegro.h"
#include "allegro/internal/aintern.h"
#include "allegro/platform/aintosx.h"

#ifndef ALLEGRO_MACOSX
#error something is wrong with the makefile
#endif

// Quickdraw specific
#ifdef ENABLE_QUICKDRAW
void osx_qz_mark_dirty();
void prepare_window_for_animation(int refresh_view);
#endif

extern void _hack_stop_keyboard_repeat();
extern void osx_mouse_available();
extern void osx_mouse_not_available();

@implementation AllegroWindow

#ifdef ENABLE_QUICKDRAW
/* display:
 *  Called when the window is about to be deminiaturized.
 */
- (void)display
{
    [super display];
    prepare_window_for_animation(TRUE);
}



/* miniaturize:
 *  Called when the window is miniaturized.
 */
- (void)miniaturize: (id)sender
{
    prepare_window_for_animation(FALSE);
    [super miniaturize: sender];
}
#endif

- (BOOL)canBecomeKeyWindow
{
    return YES;
}

- (BOOL)canBecomeMainWindow
{
    return YES;
}

@end

static int _isFullScreen = 0;

@implementation AllegroWindowDelegate

/* windowShouldClose:
 *  Called when the user attempts to close the window.
 *  Default behaviour is to call the user callback (if any) and deny closing.
 */
- (BOOL)windowShouldClose: (id)sender
{
    if (osx_window_close_hook)
        osx_window_close_hook();
    return NO;
}



/* windowDidDeminiaturize:
 *  Called when the window deminiaturization animation ends; marks the whole
 *  window contents as dirty, so it is updated on next refresh.
 */
- (void)windowDidDeminiaturize: (NSNotification *)aNotification
{
#ifdef ENABLE_QUICKDRAW
    _unix_lock_mutex(osx_window_mutex);
    osx_qz_mark_dirty();
    _unix_unlock_mutex(osx_window_mutex);
#endif
}

// We call osx_mouse_* here because we sometimes don't detect mouse entering window area if alt-tabbing from another context

- (void)windowDidBecomeMain:(NSNotification *)notification
{
    if (_isFullScreen) {
        [[NSApplication sharedApplication] setPresentationOptions:NSApplicationPresentationAutoHideMenuBar | NSApplicationPresentationHideDock];
    }
}

- (void)windowDidResignMain:(NSNotification *)notification
{
}

/* windowDidBecomeKey:
 * Sent by the default notification center immediately after an NSWindow
 * object has become key.
 */
- (void)windowDidBecomeKey:(NSNotification *)notification
{
    if (_isFullScreen) {
        [[NSApplication sharedApplication] setPresentationOptions:NSApplicationPresentationAutoHideMenuBar | NSApplicationPresentationHideDock];
    }
    _unix_lock_mutex(osx_skip_events_processing_mutex);
    osx_skip_events_processing = FALSE;
    _unix_unlock_mutex(osx_skip_events_processing_mutex);

    osx_mouse_available();
}

/* windowDidResignKey:
 * Sent by the default notification center immediately after an NSWindow
 * object has resigned its status as key window.
 */
- (void)windowDidResignKey:(NSNotification *)notification
{
    _unix_lock_mutex(osx_skip_events_processing_mutex);
    osx_skip_events_processing = TRUE;
    _unix_unlock_mutex(osx_skip_events_processing_mutex);

    osx_mouse_not_available();

    // Stop repeating keys since we might not actually see the "key up" event.
    _hack_stop_keyboard_repeat();
}

- (void)windowWillEnterFullScreen:(NSNotification *)notification
{
    [[NSApplication sharedApplication] setPresentationOptions:NSApplicationPresentationAutoHideMenuBar | NSApplicationPresentationHideDock];
    _hack_stop_keyboard_repeat();
}

- (void)windowWillExitFullScreen:(NSNotification *)notification
{
    [[NSApplication sharedApplication] setPresentationOptions:NSApplicationPresentationDefault];
    _hack_stop_keyboard_repeat();
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
    _isFullScreen = 1;
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
    _isFullScreen = 0;
}

@end

void runOnMainQueueWithoutDeadlocking(void (^block)(void))
{
    if ([NSThread isMainThread])
    {
        block();
    }
    else
    {
        dispatch_sync(dispatch_get_main_queue(), block);
    }
}

/* Local variables:       */
/* c-basic-offset: 3      */
/* indent-tabs-mode: nil  */
/* c-file-style: "linux" */
/* End:                   */
