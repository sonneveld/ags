#!/bin/bash

set -e

pushd ../../libsrc
./download.sh
popd

# android-21 is the minimum we can go with current Android SDK for 64-bits
PLATFORM=android-21

# standalone toolchains cannot share same directory
export NDK_STANDALONE=$NDK_HOME/platforms/$PLATFORM

# building lua 5.1 for arm64-v8a requires changes, so it's removed here
for arch in arm64-v8a x86_64 
do
	rm -rf ../nativelibs/$arch
	mkdir -p ../nativelibs/$arch
	pushd $arch
	chmod +x *.sh
#	./lua.sh	
	./ogg.sh	
	./tremor.sh  # ("vorbis") requires ogg
	./theora.sh  # requires ogg, "vorbis"
	./allegro.sh # requires ogg, "vorbis", theora
	./dumb.sh    # requires allegro
	popd
done

# android-14 is the minimum we can go with current Android SDK
PLATFORM=android-14

# standalone toolchains cannot share same directory
export NDK_STANDALONE=$NDK_HOME/platforms/$PLATFORM

for arch in armeabi armeabi-v7a x86 mips 
do
	rm -rf ../nativelibs/$arch
	mkdir -p ../nativelibs/$arch
	pushd $arch
	chmod +x *.sh
	./lua.sh	
	./ogg.sh	
	./tremor.sh  # ("vorbis") requires ogg
	./theora.sh  # requires ogg, "vorbis"
	./allegro.sh # requires ogg, "vorbis", theora
	./dumb.sh    # requires allegro
	popd
done


