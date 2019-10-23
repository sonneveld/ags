# Using the engine


## Adding games to the game list

By default games have to be placed into the external storage device. This is
usually the SD-card, but this can vary.

Place the game into the directory

    <EXTERN>/ags/<GAME NAME>/

<GAME NAME> is what will be displayed in the game list.



## Game options

Global options can be configured by pressing the MENU button on the game list
and choosing the "Preferences" item. These settings apply to all games unless
they have their own custom preferences set.

By performing a longclick on a game entry in the list, a menu opens that lets
you choose custom preferences specifically for that game.

In the same menu you can also choose "Continue" to resume the game from
your last savegame.



## Controls

-   Finger movement: Moving the mouse cursor
-   Single finger tap: Perform a left click
-   Tap with two fingers: Perform a right click
-   Longclick: Hold down the left mouse button until tapping the screen again
-   MENU button: Opens a menu for key input and quitting the game
-   MENU button longpress: Opens and closes the onscreen keyboard
-   BACK button: Sends ESC key command to the game
-   BACK button longpress: Opens a menu for key input and quitting the game




## Downloading prebuilt engine packages

Unsigned APKs suitable for running any compatible AGS game may be found linked in the
corresponding release posts on [AGS forum board](http://www.adventuregamestudio.co.uk/forums/index.php?board=28.0).



## MIDI playback

For midi music playback, you have to download GUS patches. We recommend
"Richard Sanders's GUS patches" from this address:

http://alleg.sourceforge.net/digmid.html

A direct link is here:

http://www.eglebbk.dds.nl/program/download/digmid.dat

This 'digmid.dat' is, in fact, a **bzip2** archive, containing actual data file,
which should be about 25 MB large. Extract that file, rename it to **patches.dat**
and place it into the  **ags** directory alongside your games.



# Building the engine

The Android app consists of three parts, each with different requirements:

- **Java app**: needs the Android SDK for Windows, Linux or Mac. If you build from command-line as opposed to Android Studio you also need Apache Ant. Requires built native engine library.
- **Native engine library**: needs the Android NDK for Windows, Linux or Mac. Requires built native 3rd party libraries.
- **Native 3rd party libraries**: need the Android NDK for Linux and number of tools (see full list in the corresponding section below).

All components are built using Android Studio.

## Native 3rd party libraries

These are backend and utility libraries necessary for the AGS engine. 

## Native engine library

This is the main AGS engine code. 

## Java app

There are two parts to the Java app, one is the engine library in `<SOURCE>/Android/library` and the other one is the launcher app. The default launcher which displays a list of games from the SD-card is in `<SOURCE>/Android/launcher_list`.

## Android Studio

Requirements:
* Android Studio
* SDK Platform: Android SDK 29 (Android 10 Q)
* SDK Tools: Android SDK Build-Tools/Platform-Tools/Tools, LLDB, NDK, CMake, Android Emulator (and accelerator if possible)
* External installation of Cmake (minimum 3.13) and Ninja

Requirements can be installed in Android Studio under "Preferences/Appearance & Behaviour/System Settings/AndroidSDK".

You may need to modify the version of cmake in `Android2/AGSGameLauncher/app/build.gradle` to be the specific external version you have installed.

**IMPORTANT:** Android port integrates number of plugins as a part of the engine. Some of the plugin sources
may be included as submodules, so make sure to initialize submodules before compiling it, e.g. from the
root <SOURCE> directory:

    $ git submodule update --init --recursive

The native code is built for all current Android architecture, that is armeabi-v7a, arm64-v8a, x86, and x86_64.

Open the Android project in `Android2/AGSGameLauncher`, and build using `Build/Make Project`

This will download, patch, build and properly install the required libraries.


# Links

Android Studio: https://developer.android.com/studio

Android thread on the AGS forum: http://www.adventuregamestudio.co.uk/yabb/index.php?topic=44768.0
