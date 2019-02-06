# Linux Build Docker Image

This build image should allow building on Linux with the minimum dependencies.
It includes SDL2, ogg, vorbis, cmake and git.

However, uou won't be able to run AGS because there won't be a display available.

## Usage

    # build docker image
    ./build.sh

    # run docker
    ./run.sh

    # you should now be in the same path as before, but mounted through docker

    cd ..
    mkdir build-linux
    cd build-linux
    cmake ..
    make -j
