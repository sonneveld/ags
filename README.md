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

The following instructions are for Ubuntu 18.04. Other distributions may require slightly different instructions.

Check out the sdl port of AGS:
https://github.com/sonneveld/ags/tree/ags3-sdl2

Set environment variables to point to source directories:

    AGS_SRC=...
    CMAKE_BUILD_TYPE=Debug   # or Release!
    #CMAKE_BUILD_TYPE=Release

Install dependencies:

    apt-get -y update
    apt-get -y --no-install-recommends install build-essential pkg-config 
    apt-get -y --no-install-recommends install libsdl2-2.0 libsdl2-dev 
    apt-get -y --no-install-recommends install curl ca-certificates
    apt-get -y --no-install-recommends install git

    curl -O -L https://github.com/Kitware/CMake/releases/download/v3.13.4/cmake-3.13.4-Linux-x86_64.tar.gz
    tar -C /opt -xf cmake-3.13.4-Linux-x86_64.tar.gz
    ENV PATH="/opt/cmake-3.13.4-Linux-x86_64/bin:${PATH}"

Build AGS:

    cd $AGS_SRC  # where you checked-out the source
    mkdir build
    cmake ..
    make -j


## Building AGS/SDL2 on macOS

Check out the sdl ports of AGS and Allegro:
https://github.com/sonneveld/ags/tree/ags3-sdl2

Set environment variables to point to source directories:

    AGS_SRC=...
    CMAKE_BUILD_TYPE=Debug   # or Release!
    #CMAKE_BUILD_TYPE=Release

Install dependencies:

    # Check out https://brew.sh/ for details on installing homebrew
    brew install cmake

Download SDL2 [ https://www.libsdl.org/release/SDL2-2.0.8.dmg ] and drag the SDL2.framework into /Library/Frameworks

Build AGS

    cd $AGS_SRC
    mkdir build-sdl2-$CMAKE_BUILD_TYPE
    cd build-sdl2-$CMAKE_BUILD_TYPE
    cmake .. -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE
    make -j

After building, there should be an "AGS.app" bundle in your build directory. You can run this like a normal app.

Bundling SDL2 framework with the app:

    Copy SDL2.framework into AGS.app/Contents/Frameworks

    # Tell the AGS binary to also search for @rpath based libraries within the bundle relative to itself.
    install_name_tool -add_rpath "@executable_path/../Frameworks" AGS.app/Contents/MacOS/AGS






*or* Build AGS for xcode if you want to develop and debug within

    cd $AGS_SRC
    mkdir build-sdl2-xcode-$CMAKE_BUILD_TYPE
    cd build-sdl2-$CMAKE_BUILD_TYPE
    cmake .. -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE -GXcode
    open AdventureGameStudio.xcodeproj


## Building AGS/SDL2 on Windows

These instructions have similiar requiements to the [original Windows build instructions](Windows/README.md), so it might be helpful to also read that documentation for further details.

Download and install Visual Studio Community 2017 https://www.visualstudio.com/downloads/

Download and install cmake https://cmake.org/download/

Download SDL2 development libraries for MSVC https://www.libsdl.org/download-2.0.php . Install in AGS Source/SDL2-2.0.8

Download OpenAL SDK and installer here https://www.openal.org/downloads/ 

Build engine:

- export SDL2DIR=C:/Users/sonneveld/source/repos/SDL2-2.0.9
- or set SDL2_ROOT_DIR as a cmake variable
- export OPENALDIR="C:/Program Files (x86)/OpenAL 1.1 SDK"


- mkdir build
- cd build
- cmake ..
- Open AGS.sln
- Build AGS (for debug or release)


# Adventure Game Studio

Adventure Game Studio (AGS) - is the IDE and the engine meant for creating and running videogames of adventure (aka "quest") genre. It has potential, although limited, support for other genres as well.

Originally created by Chris Jones back in 1999, AGS was opensourced in 2011 and since continued to be developed by contributors.

An official homepage of AGS is: http://www.adventuregamestudio.co.uk

Both Editor and Engine are licensed under the Artistic License 2.0; for more details see [License.txt](License.txt).


## Engine instructions

To get started building the AGS engine, see the platform specific instructions or forum threads:

-    [Linux](debian/README.md) ([Forum thread](http://www.adventuregamestudio.co.uk/forums/index.php?topic=46152.0))
-    [Windows](Windows/README.md)
-    [Android](Android/README.md) ([Forum thread](http://www.adventuregamestudio.co.uk/forums/index.php?topic=44768.0))
-    [iOS](iOS/README.md) ([Forum thread](http://www.adventuregamestudio.co.uk/forums/index.php?topic=46040.0))
-    [PSP](PSP/README.md) ([Forum thread](http://www.adventuregamestudio.co.uk/forums/index.php?topic=43998.0))
-    [Mac OS X](OSX/README.md) ([Forum thread](http://www.adventuregamestudio.co.uk/forums/index.php?topic=47264.0))

When running the engine on your platform of choice you may configure it by modifying acsetup.cfg or using several command line arguments.
On Windows you may also invoke a setup dialog by running executable with "--setup" argument, or executing winsetup.exe, if it is present.
For the list of available config options and command line arguments, please refer to [OPTIONS.md](OPTIONS.md).

## Contributing

We welcome any contributor who wishes to help the project.

The usual workflow is this: you fork our repository (unless you already did that), create a feature/fix branch, commit your changes to that branch, and then create a pull request. We will review your commits and sometimes may ask you to alter your code before merging it into our repository.

For bug fixing and general code improvements that may be enough, however, for significant changes, completely new features or changes in the program design, we ask you to first open an issue in the tracker and discuss it with the development team, to make sure it does not break anything, nor is in conflict with existing program behavior or concepts.

The [master](https://github.com/adventuregamestudio/ags/tree/master) branch should be kept in a working state and compilable on all targeted platforms.
The "release-X.X.X" branch is created to prepare the code for respective release and continue making patches to that release. If you've found a critical issue in the latest release it should be fixed in the release-X.X.X branch when possible (it will be merged to master later).
Because of the low number of active developers we only maintain one latest release branch. If bugs are found in one of the much older versions, we advise you to update to the latest version first.

We have a coding convention, please check it before writing the code: http://www.adventuregamestudio.co.uk/wiki/AGS_Engine_Coding_Conventions


## AGS game compatibility:

This runtime engine port is still not compatible with all of the AGS games. There are the following restrictions:
- Supports (imports into editor and runs by the engine) all versions of AGS games starting from games made with AGS 2.50 up to games made with the latest 3.x release, but there may be unknown compatibility issues with very old games.
- If you try to run an unsupported game, you will receive an error message, reporting original version of AGS it was made in, and data format index, which may be used for reference.
- Savegames are compatible between the different platforms if they are created with the same engine version. Engine should normally read savegames made by version 3.2.0 of runtime and above, but that has not been tested for a while.
- Games that depend on plugins for which there is no platform-independent replacement will not load.


## Changes from Chris Jones' version of AGS

This version of AGS contains changes from the version published by Chris Jones.
The run-time engine was ported to Android, iOS, Linux, Mac OS X and PSP and a refactoring effort is under way.
A detailed documentation of the changes is provided in the form of the git log of this git repository
(https://github.com/adventuregamestudio/ags).


## Credits

[Link](Copyright.txt)
