# Experimental SDL2 Port

This requires the allegro fork with SDL2 as a platform. Refer to this branch:
https://github.com/sonneveld/allegro/tree/sdl2

The aim of this branch is to attempt to slowly migrate AGS to SDL2. To do this, Allegro4 has been
patched to use SDL2 as the platform that provides input, audio, graphics, etc. That way we can still
rely on Allegro for things like bitmap graphics drawing/conversion, audio mixing, midi rendering, etc.

Current status is that games seem to work with the ported allegro library. AGS still calls allegro, which
calls SDL2 functions. Work has started on replacing those Allegro calls with direct SDL2 calls.

We might not be able to rid AGS completely of Allegro, but at least it will be kept at a minimum while 
we use a slightly more up-to-date library.


## Building AGS/SDL2 on Linux

Check out the sdl ports of AGS and Allegro:
https://github.com/sonneveld/allegro/tree/sdl2
https://github.com/sonneveld/agscommunity/tree/sdl2-port

Set environment variables to point to source directories:

    ALLEGRO_SRC=...
    AGS_SRC=...

Install dependencies (for ubuntu):

    apt-get update
    apt-get install build-essential pkg-config cmake libsdl2-2.0 libsdl2-dev libsdl2-dev libfreetype6-dev libogg-dev libtheora-dev libvorbis-dev liballegro4-dev libaldmb1-dev

Build allegro:

    cd $ALLEGRO_SRC
    mkdir build-sdl2
    cd build-sdl2
    cmake -D SHARED=off -DCMAKE_BUILD_TYPE=Debug ..
    make
    make install

This will install into /usr/local

Build AGS:

    cd $AGS_SRC
    cd Engine
    make


## Building AGS/SDL2 on macOS

Check out the sdl ports of AGS and Allegro:
https://github.com/sonneveld/allegro/tree/sdl2
https://github.com/sonneveld/agscommunity/tree/sdl2-port

Set environment variables to point to source directories:

    ALLEGRO_SRC=...
    AGS_SRC=...
    NUM_JOBS=$(sysctl -n hw.ncpu)  # 8 on my machine
    CMAKE_BUILD_TYPE=Debug   # or Release!
    #CMAKE_BUILD_TYPE=Release

Install dependencies:

    brew install cmake

Download SDL2 [ https://www.libsdl.org/release/SDL2-2.0.8.dmg ] and drag the SDL2.framework into /Library/Frameworks

Build OSX Libs (check out OSX/buildlibs/README.md for more details)

    cd $ALLEGRO_SRC
    cd OSX/buildlibs
    make libs install

Build allegro:

    cd $ALLEGRO_SRC

    mkdir build-release
    pushd build-release
    cmake -DCMAKE_INSTALL_PREFIX=$AGS_SRC/OSX -D SHARED=off -DCMAKE_BUILD_TYPE=Release ..
    make
    make install
    popd

    mkdir build-debug
    pushd build-debug
    cmake -DCMAKE_INSTALL_PREFIX=$AGS_SRC/OSX -D SHARED=off -DCMAKE_BUILD_TYPE=Debug ..
    make
    make install
    popd
    
    
Copy game files into $AGS_SRC/game_files in this format:

    ac2game.dat
    acsetup.cfg
    audio.vox
    speech.vox

Build AGS

    cd $AGS_SRC
    mkdir build-sdl2-$CMAKE_BUILD_TYPE
    cd build-sdl2-$CMAKE_BUILD_TYPE
    cmake .. -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE
    make -j$NUM_JOBS

After building, there should be an "AGS.app" bundle in your build directory. You can run this like a normal app.



*or* Build AGS for xcode if you want to develop and debug within

    cd $AGS_SRC
    mkdir build-sdl2-xcode-$CMAKE_BUILD_TYPE
    cd build-sdl2-$CMAKE_BUILD_TYPE
    cmake .. -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE -GXcode
    open AdventureGameStudio.xcodeproj



# Adventure Game Studio

Adventure Game Studio (AGS) - is the IDE and the engine meant for creating and running videogames of adventure (aka "quest") genre. It has potential, although limited, support for other genres as well.

Originally created by Chris Jones back in 1999, AGS was opensourced in 2011 and since continued to be developed by contributors.

An official homepage of AGS is: http://www.adventuregamestudio.co.uk

Both Editor and Engine are licensed under the Artistic License 2.0; for more details see [License.txt](License.txt).


## Engine instructions

To get started building the AGS engine, see the platform specific instructions or forum threads:

-    [Linux](debian/README.md) ([Forum thread](http://www.adventuregamestudio.co.uk/forums/index.php?topic=46152.0))
-    [Windows](Windows/README.md) ([Forum thread](http://www.adventuregamestudio.co.uk/forums/index.php?topic=46847.0)) ([wiki](http://www.adventuregamestudio.co.uk/wiki/Compiling_AGS))
-    [Android](Android/README.md) ([Forum thread](http://www.adventuregamestudio.co.uk/forums/index.php?topic=44768.0))
-    [iOS](iOS/README.md) ([Forum thread](http://www.adventuregamestudio.co.uk/forums/index.php?topic=46040.0))
-    [PSP](PSP/README.md) ([Forum thread](http://www.adventuregamestudio.co.uk/forums/index.php?topic=43998.0))
-    [Mac OS X](OSX/README.md) ([Forum thread](http://www.adventuregamestudio.co.uk/forums/index.php?topic=47264.0))

When running the engine on you may configure it by modifying acsetup.cfg or using several command line arguments.
On Windows you may also invoke a setup dialog by running executable with "--setup" argument, or executing winsetup.exe, if it is present.
For the list of available config options and command line arguments, please refer to [OPTIONS.md](OPTIONS.md).


## Issue tracker

Please report bugs and feature requests at the [AGS Issue Tracker](http://www.adventuregamestudio.co.uk/forums/index.php?action=projects)!


## Contributing

We welcome any contributor who wishes to help the project.

The usual workflow is this: you fork our repository (unless you already did that), create a feauture/fix branch, commit your changes to that branch, and then create a pull request. We will review your commits, and sometimes may ask to change something before merging into ours.

For bug fixing and general code improvements that may be enough, however, for significant changes, especially completely new features, it is advised to first open an issue in the tracker and discuss it with development team, to make sure it does not break anything, nor contradict to existing program behavior or concepts.

The [master](https://github.com/adventuregamestudio/ags/tree/master) branch should be kept in a working state and always compilable on all targeted platforms.
Larger changes that potentially break things temporarily should first be made in other branches or in personal forks.

We have a coding convention, please check it before writing the code: http://www.adventuregamestudio.co.uk/wiki/AGS_Engine_Coding_Conventions


## AGS game compatibility:

This runtime engine port is not compatible with all AGS games. There are the
following restrictions:

-   Supported AGS versions right now are all starting from 2.50; games between 2.5 and
    3.2 are supported in theory, but may have yet unknown compatibility issues.
    You can check the version of AGS a game was made with by examining the properties
    of the game executable.
    If you try to run a game made with a newer or older version of AGS, you will
    receive an error when trying to launch the game.
-   Savegames are compatible between the different platforms if they are created
    with the same engine version.
-   Games that depend on plugins for which there is no platform-independent
    replacement will not load.

	
## Changes from Chris Jones' version of AGS

This version of AGS contains changes from the version published by Chris Jones.
The run-time engine was ported to Android, iOS, Linux, Mac OS X and PSP and a refactoring effort is under way.
A detailed documentation of the changes is provided in the form of the git log of this git repository
(https://github.com/adventuregamestudio/ags).


## Credits

[Link](Credits.txt)
