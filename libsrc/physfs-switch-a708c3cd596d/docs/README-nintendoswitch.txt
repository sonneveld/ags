PhysicsFS for Nintendo Switch, README.
======================================

First things first
------------------

This is a port of PhysicsFS (physfs) to the Nintendo Switch. You
shouldn't have this if you aren't under NDA with Nintendo, so if you haven't,
please register with [Nintendo's developer program](https://developer.nintendo.com/)
right now. Registration is free.

(You'll need to register with Nintendo anyhow to get the SDK you'll need to
build Switch software.)

At the time of this writing, Switch access is not available to the general
public, with or without a Nintendo Developer account, so please delete this
software if you don't have Switch access.

Please do not distribute the source code to this fork of PhysicsFS to people
that aren't registered Nintendo developers. Doing so may violate your NDA
with Nintendo.

You may otherwise use this source code under the usual license agreement
found in LICENSE.txt (the zlib license).


How to build PhysicsFS for Nintendo Switch
------------------------------------------

If you haven't, install Nintendo Developer Interface from
developer.nintendo.com. Set up a new Switch environment. We installed a
default 4.5.0 install to C:\Nintendo\physfs-Switch, but whatever you like 
is fine. You can reuse this Switch environment for your actual project, so
you don't need to make a new one just to build physfs if you already have 
one somewhere else. The project files we supply expect the NINTENDO_SDK_ROOT
environment variable to be set; make sure your Switch environment set this
up for you in any case.

We supply a Visual Studio 2017 project file. If you installed the VSI
package in the Nintendo Developer Interface, you should be able to
double-click physfs-switch/physfs-nx/physfs-nx.sln, click Rebuild, and have
a PhysicsFS library test program ready to install on an EDEV or SDEV unit.
This project can also build as a Windows library that doesn't use the
NintendoSDK, but you should get an API-compatible library for your 
Windows-based NintendoSDK builds.

Visual Studio is not my area of expertise; you might be able to just tell it
to build PhysicsFS in this .sln before building your separate solution, but
it might be easier to just build this once, copy the static libraries to your
project, point your "additional includes" setting at physfs-switch/src
and be done with it.

Alternately: if you list all the PhysicsFS sources in your project, you can
just build it as part of your project. It shouldn't need any magic
preprocessor defines to build and will include all available archive formats
and the proper platform support by default. This might be useful if you depend
on an earlier Visual Studio version or your own build system, too.


Memory Management
-----------------

PhysicsFS lets you define your own allocator with PHYSFS_setAllocator(),
but by default it will use the C runtime's malloc() and friends. There
is one or two uses of C++ operator new to accomodate the NintendoSDK.


Threads
-------

PhysicsFS doesn't create any threads, but it does use some mutexes to
make the same thread-safety guarantees it offers on other platforms.
These are implemented internally with nn::os::MutexType.


Filesystem
----------

During PHYSFS_init(), PhysicsFS will attempt to mount the ROM filesystem
to "rom", using its allocator allocate the cache, and if successful, will
return "rom:/" as the base dir. This path can be used with NintendoSDK's
nn::fs APIs or standard C runtime calls like fopen() (and PHYSFS_mount, of
course). If you're using PhysicsFS, you shouldn't mount NintendoSDK-level
filesystems yourself, as we manage this for you. Notably: if you're also
using SDL on the Switch, don't call SDL_GetBasePath() and friends, as they
will also mount these filesystems, causing problems.

PhysicsFS will also mount the save filesystem during PHYSFS_init().
PHYSFS_getUserDir() will return "save:/". PHYSFS_getPrefDir() will do the
same, ignoring its arguments.

We mount the save filesystem with the UID from 
nn::account::OpenPreselectedUser(). This means your app must have "console
account selection at launch" configured in your NMETA files, and it must not
be set to "None." This lets the OS provide a standard UI for choosing
accounts. At this time, PhysicsFS requires this and doesn't offer a way to
let you choose a specific user (or no user at all). This can change in the
future if there's a need.

Presumably you will call PHYSFS_setWriteDir() with the save directory mount
point. After writing everything you want to save:/, don't forget to call
PHYSFS_NintendoSwitch_CommitSaveData(), which is just a simple C wrapper over
nn::fs::CommitSaveData(), or your data will be lost. PhysicsFS won't commit it 
for you by default at this time. Make sure you close your files before
committing!

The filesystems are unmounted and the cache is freed when you successfully
call PHYSFS_deinit(). PHYSFS_init() will remount it if you deinit and then
init again.


Questions and feedback
----------------------

This port was done by Ryan C. Gordon <icculus@icculus.org>, and he's happy
to hear about questions and bugs by email.
