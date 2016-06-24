#!/bin/bash

set -e

LIBSRCDIR=../../libsrc

pushd ${LIBSRCDIR}
./download.sh
popd

if [[ $('xcode-select' --print-path) != "/Applications/Xcode.app/Contents/Developer" ]]; then
    echo "xcode-select path must be set to \"/Applications/Xcode.app/Contents/Developer\""
    echo "try \"sudo xcode-select --reset\""
    exit 1
fi

for bin in curl pkg-config autoconf automake aclocal glibtoolize cmake xcodebuild; do
 path_to_executable=$(which $bin)
 if [ ! -x "$path_to_executable" ] ; then
    echo "error: require $bin"
    echo "try \"brew install curl pkg-config autoconf automake libtool\""
    exit 1
 fi
done

PREFIX=$(mktemp -d)

export PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig
PLAT_FLAGS="-mmacosx-version-min=10.7 -arch i386 -arch x86_64"

# lua
tar xf ../../libsrc/lua-5.1.5.tar.gz 
pushd lua-5.1.5
patch -p1 < ../../patches/lua-5.1.5.patch
make macosx INSTALL_TOP="$PREFIX" PLAT_LDFLAGS="$PLAT_FLAGS" PLAT_CFLAGS="$PLAT_FLAGS" 
make install INSTALL_TOP="$PREFIX"
popd


# freetype
tar xf ${LIBSRCDIR}/freetype-2.4.12.tar.bz2
pushd freetype-2.4.12
./configure CFLAGS="$PLAT_FLAGS" LDFLAGS="$PLAT_FLAGS" \
    --prefix=$PREFIX \
    --enable-static --disable-shared \
    --enable-biarch-config
make
make install
popd


# ogg
tar xf ${LIBSRCDIR}/libogg-1.3.2.tar.gz
pushd libogg-1.3.2
./configure CFLAGS="$PLAT_FLAGS" LDFLAGS="$PLAT_FLAGS" \
    --prefix=$PREFIX \
    --enable-static --disable-shared
make
make install
popd


#vorbis
tar xf ${LIBSRCDIR}/libvorbis-1.3.5.tar.gz
pushd libvorbis-1.3.5
./configure CFLAGS="$PLAT_FLAGS" LDFLAGS="$PLAT_FLAGS" \
    --prefix=$PREFIX \
    --enable-static --disable-shared \
    --disable-docs --disable-examples --disable-oggtest
make
make install
popd


#theora
tar xf ${LIBSRCDIR}/libtheora-20160525-g50df933.tar.bz2
pushd libtheora-20160525-g50df933
./autogen.sh CFLAGS="$PLAT_FLAGS" LDFLAGS="$PLAT_FLAGS"  \
    --prefix=$PREFIX \
    --enable-static --disable-shared \
    --with-ogg=${PREFIX} --with-vorbis=${PREFIX} \
    --disable-examples \
    --disable-asm \
    --disable-vorbistest \
    --disable-oggtest \
    --disable-doc   \
    --disable-spec  
make
make install
popd


#allegro
tar xf ${LIBSRCDIR}/allegro-4.4.2.tar.gz 
pushd allegro-4.4.2
patch -p1 < ../../patches/allegro-4.4.2.patch

rm -rf build
mkdir build
pushd build

#-D CMAKE_BUILD_TYPE=Debug \
cmake \
'-DCMAKE_OSX_ARCHITECTURES=x86_64;i386' \
-D CMAKE_OSX_DEPLOYMENT_TARGET=10.7 \
-D SHARED=off \
-D WANT_ALLEGROGL=off -D WANT_LOADPNG=off -D WANT_LOGG=off -D WANT_JPGALLEG=off -D WANT_EXAMPLES=off -D WANT_TOOLS=off -D WANT_TESTS=off \
-D WANT_DOCS=off \
-D CMAKE_INSTALL_PREFIX=${PREFIX} \
-G Xcode ..

#xcodebuild -project ALLEGRO.xcodeproj clean
xcodebuild -project ALLEGRO.xcodeproj -target ALL_BUILD -configuration Release
xcodebuild -project ALLEGRO.xcodeproj -target install -configuration Release 

popd # build
popd # src


#dumb
tar xf ${LIBSRCDIR}/dumb-0.9.3.tar.gz
pushd dumb-0.9.3
patch -p1 < ../../../OSX/patches/dumb-0.9.3.patch
cat > make/config.txt <<EOT
include make/unix.inc
ALL_TARGETS := core core-headers
ALL_TARGETS += allegro allegro-headers
PREFIX := ${PREFIX}
PLAT_CFLAGS := -mmacosx-version-min=10.7 -arch i386 -arch x86_64 -I\$(PREFIX)/include
EOT
make
make install
popd


# install
cp ${PREFIX}/lib/*.a ../lib
rm -rf ../include
cp -a ${PREFIX}/include ../include

rm -rf ${PREFIX}
