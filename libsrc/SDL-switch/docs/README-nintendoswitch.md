Simple Directmedia Layer 2.0.x for Nintendo Switch, README.
===========================================================

I ACCEPT DONATIONS
------------------

This code is free for you to use, but I _do_ accept donations.
If you're shipping a commercial game that uses this, I don't
require payment, but I appreciate if you throw a few dollars
at me.

Patreon: https://patreon.com/icculus
Paypal: icculus@icculus.org
cash.me: $icculus

Thanks!



First things first
------------------

This is a port of Simple Directmedia Layer (SDL) to the Nintendo Switch. You
shouldn't have this if you aren't under NDA with Nintendo, so if you haven't,
please register with [Nintendo's developer program](https://developer.nintendo.com/)
right now. Registration is free.

(You'll need to register with Nintendo anyhow to get the SDK you'll need to
build Switch software.)

At the time of this writing, Switch access is not available to the general
public, with or without a Nintendo Developer account, so please delete this
software if you don't have Switch access.

Please do not distribute the source code to this fork of SDL to people that
aren't registered Nintendo developers. Doing so may violate your NDA with
Nintendo.

You may otherwise use this source code under the usual license agreement
found in COPYING.txt (the zlib license).


How to build SDL for Nintendo Switch
------------------------------------

If you haven't, install Nintendo Developer Interface from
developer.nintendo.com. Set up a new Switch environment. We installed a
default 5.6.0(en) install to C:\Nintendo\SDL-Switch, but whatever you like is
fine. You can reuse this Switch environment for your actual project, so you
don't need to make a new one just to build SDL if you already have one
somewhere else. The project files we supply expect the NINTENDO_SDK_ROOT
environment variable to be set; make sure your Switch environment set this up
for you in any case.

We supply a Visual Studio 2017 project file. If you installed the VSI
package in the Nintendo Developer Interface, you should be able to
double-click SDL-switch/VisualC-NX/SDL/SDL.sln, click Rebuild, and have an SDL
library and several test programs ready to install on an EDEV or SDEV unit.
This project will fail to build as a Windows library because it tries to
compile several POSIX pieces that work on a Switch but not Win32. Please use
the usual solution in the VisualC directory for Win32/Win64 builds, and you
should get an API-compatible library for your Windows-based NintendoSDK
builds.

Visual Studio is not my area of expertise; you might be able to just tell it
to build SDL in this .sln before building your separate solution, but it might
be easier to just build this once, copy the static libraries to your project,
point your "additional includes" setting at SDL-switch/include and be done
with it.

Alternately: if you list all the SDL sources in your project, you can just
build it as part of your project. It shouldn't need any magic preprocessor
defines to build. This might be useful if you depend on an earlier Visual
Studio version, too.

Your application needs to link against SDL, and optionally SDLmain (see
"Main entry point" section for details). The SDLtest library is just some
helper code for SDL's own test apps; you can ignore it.

Additionally, you should link against OpenGL and Vulkan from the SDK.
In Visual Studio, right click your app's project, select "Properties",
"Configuration Properties", "Linker", "Input", then make sure
"Additional NSO Files" has "opengl.nso;vulkan.nso" in the list.
Be sure to set this for NX32 and NX64 targets, as appropriate, and not
Windows targets. You have to do this even if your program doesn't use
OpenGL or Vulkan.


Main entry point
----------------

SDL optionally provides nnMain() for your application, so you can just supply
a standard Unix-style main() function like you do on other platforms. Make
sure that you...

    #include "SDL.h"

...before your main() implementation, since SDL will #define this as
"SDL_main" and will expect to be able to call into that function.

If you are supplying your own nnMain(), just don't link with SDL's. All it
does is call SDL_main() with an argc/argv command line as a portability
convenience, but you aren't required to use it.


Memory Management
-----------------

SDL_malloc() is backed by the Switch's standard C runtime's malloc,
and the system doesn't need any special handling of memory. Since the SDK
offers a C++ interface, it's possible that operator new might be called
internally on occasionally, but when given the option, SDL will favor
SDL_malloc.


Power
-----

SDL_GetPowerInfo() always returns SDL_POWERSTATE_UNKNOWN (battery status isn't
exposed to apps on the Switch).


Touchscreen
-----------

The touchscreen on the device is supported, and can send touch events for
multiple fingers at once. The first finger to touch will also be treated as
a mouse (with the device id set to SDL_TOUCH_MOUSEID), so your mouse-driven
game might Just Work with the touch screen if it only needs simple left 
button up/down and drags. There is no mouse cursor, and motion is only
reported while the finger is down on the screen (and thus the left button
is reported as pressed), of course. If you only care about pure touch events,
ignore mouse events from SDL_TOUCH_MOUSEID devices.

Touch coordinates are scaled to your SDL window. So if you're not running at
the native resolution of the screen, scaling video, SDL will adjust the
input to match where you touched on the screen, relative to your window size.


Joysticks
---------

Joystick input on the Switch is a complicated topic, and you'll likely need
to read the NintendoSDK documentation to get comfortable with the fine details
if you need more than a single player using both hands to play. SDL makes an
attempt to make this all work like SDL joysticks expect to, with hotplug
support and multiple devices.

There are lots of things you can do with the NintendoSDK that are beyond the
scope of SDL, such as query for the color of a given Joy-Con or change how
controllers behave. Joystick devices (which might be a separate Left and Right
Joy-Con working together to look like one device) are exposed in the
NintendoSDK as nn::hid::NpadIdType objects (which are just 32-bit ints at the
lowest level). If you want to obtain the NpadIdType for a given SDL joystick
device index, you can use...

int SDL_NintendoSwitch_JoystickNpadIdForIndex(const int index, Sint32 *npadid);

(or, if you have an opened SDL_Joystick object...)

int SDL_NintendoSwitch_JoystickNpadId(SDL_Joystick *joystick, Sint32 *npadid);

...and then manipulate that NpadIdType directly in various ways. Note that if
you change the controller's style, SDL will report the joystick as disconnected
and immediately reconnect it as a new joystick. Expect this to happen if you,
for example, force a dual Left/Right Joy-Con pair into single assignment mode.

During SDL_Init(SDL_INIT_JOYSTICK), SDL will call:

    nn::hid::SetSupportedNpadStyleSet(
        nn::hid::NpadStyleJoyLeft::Mask | 
        nn::hid::NpadStyleJoyRight::Mask | 
        nn::hid::NpadStyleJoyDual::Mask | 
        nn::hid::NpadStyleFullKey::Mask |
        nn::hid::NpadStyleHandheld::Mask
    );

    nn::hid::SetNpadJoyHoldType(nn::hid::NpadJoyHoldType_Horizontal);

    nn::hid::SetSupportedNpadIdType([npad IDs from No1 to No8 and Handheld]);

...this is in an effort to catch and support every possible configuration. If
you want to forcefully limit the number of controllers that can connect,
or the styles of control, you should call these functions directly some time
after SDL_Init has finished.

Different styles report different button and axis counts, and while SDL's API
makes no promises about which button/axis is which on a given controller, they
follow definite rules on the Switch. Alternately, you can use SDL's Game
Controller API for consistency across devices, so you always know that you
pressed a specific physical button regardless of the device details.

The following lists start at zero and increment as the list continues. The Home
and Record buttons are never available to apps.

Note that SDL will notice if you call nn::hid::SetNpadJoyHoldType() to make
the controllers vertical. If this case, it will rotate the analog sticks and
D-Pad, X, Y, A, and B to match the holding style (that is, if you are holding
a right-hand Joy-Con horizontally, the button labelled 'X' will report the
user pressed 'A' so you get the same _physical_ inputs when rotated.

The following lists assume you are holding single Joy-Cons vertically.

JoyLeft style: 2 axes, 10 buttons
buttons: up, down, left, right, l, zl, stickl, minus, leftsl, leftsr
axes: stick x, stick y

JoyRight style: 2 axes, 10 buttons
buttons: a, b, x, y, r, zr, stickr, plus, rightsl, rightsr
axes: stick x, stick y

JoyDual style: 4 axes, 20 buttons
buttons: up, down, left, right, a, b, x, y, l, r, zl, zr, stickl, stickr, plus, minus, leftsl, leftsr, rightsl, rightsr
axes: left stick x, left stick y, right stick x, right stick y

FullKey and Handheld styles: 4 axes, 16 buttons
buttons: up, down, left, right, a, b, x, y, l, r, zl, zr, stickl, stickr, plus, minus
axes: left stick x, left stick y, right stick x, right stick y


Game controllers
----------------

All the possible joystick styles have matching game controller configs
built-in, so you can always use the Game Controller API instead of the
joystick API if that's easier for you.

Note that single Joy-Cons don't have nearly enough buttons to map to the usual
game console controller one expects, so be sure to forbid this style if you
need more. As it stands, they each report a left stick, A/B/X/Y, Start, L and
ZL, even if it's a Right Joy-Con. The SR and SL buttons are mapped to R and ZR
out of desperation, but nothing really maps well to those buttons in this
paradigm.

The other styles (handheld, fullkey, dual) all have enough buttons in the
right places to do what you'd expect them to as a game controller.


Keyboards and mice
------------------

If you plug a USB keyboard or mouse (or keyboards and mice) into the Switch,
SDL will be able to report input from them in the standard SDL event ways.
This is separate from touch screen input, on-screen virtual keyboards, etc.

All connected physical keyboards look like one to the NintendoSDK (it ORs
the value of a key on all keyboards to decide if it's pressed, etc), and
likewise for mice, so you can't report multiple distinct physical inputs
for this platform. All physical mice look like mouseid 0 (which is separate,
of course, from SDL_MOUSEID_TOUCH for touchscreen input simulating a mouse).


Haptic
------

Haptic works. Most (all?) devices support vibration on the Switch. We only
expose the basic left/right effect (like XInput). SDL_HapticRumblePlay()
works, too, in case you just want to make the controller buzz a little.

If the user has disabled vibration in the system settings, SDL will not report
any haptic devices, SDL_JoystickIsHaptic() will return 0, and 
SDL_HapticOpen*() will fail.

There are no haptic mice on the Switch (even if your USB mouse would otherwise
support it).

When you open a haptic device, it might take the system several hundred
milliseconds to initialize it; SDL will not block during this time, but any
vibrations you try to run on the hardware during this period will be ignored.
Plan accordingly. If you open your haptic devices at startup or when joysticks
are connected, they'll likely be ready by the time you actually need them to
vibrate.


Immediate events
----------------

Just like iOS and Android, there are a few system events you must respond to
immediately, if you choose to respond to them.

For these, you should have called SDL_SetEventFilter() near startup and
use a callback to process the SDL_APP_* events. For these specific events,
if you wait to get them from SDL_PollEvent(), it may be too late, so you
use the event filter to see them as soon as SDL itself sees them.

* SDL_APP_TERMINATING: This happens when the Switch is preparing to kill
your process (perhaps because the user has asked it to do so on the home
screen). This maps to nn::oe::MessageExitRequest. You will only see this
event if you've called nn::oe::EnterExitRequestHandlingSection() at some
point, and you should call nn::oe::LeaveExitRequestHandlingSection() after
receiving this event, which will terminate your process. You can either
call nn::oe::EnterExitRequestHandlingSection() somewhere near startup and
leave it enable always, or call Enter/Leave pairs during crucial moments
when you need to deal with a sudden termination. If this message is to
be fired by the system and you haven't entered a handling section, your
app will terminate without warning. It's totally possible that you don't
need to do any of this for your game, but the functionality is available
to you if you need it. Handle this event as quickly as possible.


CPU info
--------

The Switch is an Tegra X1 device; that is to say, the CPU is a 4-core 
ARM Cortex-A57 (ARMv8) with NEON support, so SDL_HasNEON() will
correctly return SDL_TRUE. Non-ARM checks like SDL_HasSSE2() will return
SDL_FALSE. You can build 32 or 64-bit programs for the Switch.


Atomics
-------

All atomic operations (SDL_AtomicAdd(), etc) are supported.


Threads
-------

Threads work as expected (this uses the existing Unix pthreads interface
behind the scenes). For debugging purposes, thread names are reported in
the debugger. Note that by default on the Switch, pthread_create() makes
threads with a stack of 80 kilobytes, which is way less than Windows,
Linux, or macOS. If you need more, either make your thread use less stack,
or use SDL_CreateThreadWithStackSize() instead of SDL_CreateThread(). Setting
the stack size to zero says "use the platform's default stack size" (80
kilobytes). Your stack size must be a multiple of 4096 on the Switch
or things will fail.


Dynamic API
-----------

Presumably you don't want to override your SDL with a different build at
runtime, so the Dynamic API is disabled on Nintendo Switch.


Timing
------

SDL_GetTicks(), performance counters, and SDL_Delay() work as expected.


Audio
-----

Multiple audio devices are supported (the Switch lets you open two at a time
at most), and can be enumerated by SDL. The Switch hardware always runs at
48000Hz, in either stereo or 5.1. SDL will let you open the device in any
format and quietly convert on the fly, but it's more efficient to aim your
asset pipeline to this exact format so SDL can just feed it directly to the
hardware.

The Switch has a concept of a "default audio device" and SDL will use this
if you pass a NULL to SDL_OpenAudioDevice(). Likely this is what you want
to do, as it will do the right thing as you move between speakers,
headphones, and HDMI (probably).

The Switch does not have a built-in microphone, so you probably won't see 
any capture devices at this time. The SDK's API offers audio capture, though,
so if they add bluetooth audio or some microphone adapter to the console
later, it should already work here.


Video
-----

The Switch has a single on-device display. The LCD screen on the device runs 
at 720p (1280x720 pixels), and when docked, it can drive an HDMI television
screen at 1080p (1920x1080 pixels). The system will scale output up or down
as appropriate, and you can not drive both the LCD and the HDMI displays 
simultaneously. When docked, the console runs faster, presumably because it 
won't have to live off battery and because it should render to the higher
resolution if possible.

SDL reports a single display with 720p and 1080p resolutions. Only one
window can exist on a display at a time, and will render fullscreen.
A window can be set to any size, and SDL will scale it, respecting aspect 
ratio, adding letterboxing as appropriate. This might be useful if you want
to render something smaller than the screen to save on pixel shader time and
scale it up.

If you set your window to SDL_WINDOW_FULLSCREEN_DESKTOP, it'll make it the
size of the display, and send window resize events as the device is docked
and undocked. If you want your game to always render at the native size of the
output, this is what option you should choose. If you want your game to
always render to a specific size (and scale to the display if necessary),
just create a (not-fullscreen-desktop) SDL window.

If you want to manually change the window size, SDL_SetWindowSize() will
destroy the backbuffer and create a new one at the new resolution;
aspect-correct scaling will continue to work as expected on the new
backbuffer. This can be useful if, say, you want to change resolutions
for the game vs a prerendered cutscene, etc. OpenGL contexts are preserved
when recreating the backbuffer, so you should be able to just carry on with
the new output dimensions.

If you create a window with SDL_WINDOW_OPENGL, SDL will set up an EGL
surface and SDL_GL_CreateContext will work. OpenGL 4.5 (Core and Compatibility
profiles) and OpenGL ES 3.2 are supported. SDL_WINDOW_VULKAN will produce a
Vulkan 1.0-compatible surface. If you specify neither, SDL will set up a layer
and native window but nothing else, allowing you to use nvn yourself for
rendering.


SysWM
-----

SDL_SysWMinfo has a subsystem of SDL_SYSWM_NINTENDOSWITCH, and offers a 
"nintendoswitch" field in the "info" union, like this:

    struct
    {
        void *egl_surface;  /* EGLSurface, NULL if not using GL/GLES */
        void *vi_window;    /* nn::vi::NativeWindowHandle, used with every rendering API */
        void *vi_layer;     /* nn::vi::Layer *, used with every rendering API. */
    } nintendoswitch;

SDL_SysWMmsg doesn't have any Switch-specific data right now.


RWOPS
-----

See notes on "Filesystem" below about what pathnames are available and how
to mount/commit your data, but otherwise, RWOPS works as expected.


Filesystem
----------

The first call to SDL_GetBasePath() will attempt to mount the ROM filesystem
to "rom", using SDL_malloc to allocate the cache, and if successful, will
return "rom:/" as the base path. This path can be used with NintendoSDK's
nn::fs APIs or standard C runtime calls like fopen() (and SDL_rwops, of
course). If you plan to do all your own file management through
the NintendoSDK, this function is meaningless and you shouldn't call it, 
since it will mount your ROM filesystem either a second time, or somewhere 
you didn't expect it to be mounted.

The filesystem is unmounted and the cache is SDL_free()'d when you call
SDL_Quit(). SDL_GetBasePath() will remount it if you Quit and then Init
SDL again.

SDL_GetPrefPath() works in roughly the same way. If you call it, we currently
ignore the arguments to it, and return you a path of "save:/" with the UID
from nn::account::OpenPreselectedUser() chosen when mounting save data. This
means if you plan to use this function, your app must have "console account
selection at launch" configured in your NMETA files, and it must not be set
to "None." This lets the OS provide a standard UI for choosing accounts; if
you plan to implement your own UI for choosing accounts, you shouldn't call
this function, and you should plan to mount your own save data, too.

After writing everything you want to save:/, don't forget to call
SDL_NintendoSwitch_CommitSaveData(), which is just a simple C wrapper over
nn::fs::CommitSaveData(), or your data will be lost. SDL won't commit it 
for you by default at this time. Make sure you close your files before
committing!


Render
------

There is not an "nvn" backend for rendering, as both OpenGL and GLES are
supported on the Switch (one can be added if there is demand for it). 
The system will default to GLES, but you can select the full OpenGL
renderer if you plan to make OpenGL calls of your own on top of SDL's
renderer API. Once a Vulkan renderer is implemented, this should work
too, and will likely become the default.


stdlib
------

Nintendo's SDK ships with a robust and modern POSIX C runtime, so you don't
need stdlib here. All the SDL_* stdlib functions just call their C runtime
equivalents.


loadso
------

SDL_LoadObject() and SDL_LoadFunction() work. Shared libraries on the Switch
are split into two files: an NRR file and an NRO file. SDL_LoadObject()
expects to find both files in the same directory, with the same (likely
case-sensitive!) base name. One file will have an .nrr extension and the
other .nro. The filename you pass to SDL will remove any extension and
add the correct ones as necessary. That means that passing in "mylib",
"mylib.nrr", and "mylib.nro" will all work, as will
"mylib.totally-bogus-extension". Like all paths on the Switch, you need to
specify the mount point for the library (see "filesystem" section for 
details).


Message boxes
-------------

Message boxes are not implemented on this platform. The NintendoSDK offers
an extremely basic error dialog in the nn::err namespace, but it's not
flexible enough to fit SDL's needs, but it might be useful for you for
one-off messages. You probably do not want to ship a console game
that shows an error dialog in release builds.


SDL_assert
----------

Assertions work, but by default only trigger a simple dialog that shows the
error and then carries on as if the user had clicked "abort," that is, it
terminates your app. You are free to supply your own assertion-failure UI
with SDL_SetAssertionHandler(), or set the SDL_ASSERT environment variable
to bypass a UI at all. It's probably considered extremely bad form to ship
a console title with assertions enabled in any case, so plan accordingly
in your release builds.


On-screen keyboard
------------------

You can't use the Switch's software keyboard app from the usual SDL APIs,
since using it blocks your app until the user finishes entering text.
Therefore, SDL_StartTextInput() is a no-op on the Switch and will not
display the on-screen keyboard. However, you can use the helper function
SDL_NintendoSwitch_RunSoftwareKeyboard(). Doing so will manage the
software keyboard for you, and return a UTF-8 string of the user's input
(which might contain endline characters if they hit the return key in
multiline mode). If the user hit Cancel, an empty string is returned, and
if there was an error (out of memory, etc), NULL is returned and
SDL_GetError() will report the problem. Don't forget to SDL_free()
the return value when done with it!

This function can set the keyboard to a handful of different modes that
are the most likely use-cases (single-line, multi-line, passwords and
numbers), but if you need more flexibility, you can use nn::swkbd yourself.


Questions and feedback
----------------------

This port was done by Ryan C. Gordon <icculus@icculus.org>, and he's happy
to hear about questions and bugs by email.

