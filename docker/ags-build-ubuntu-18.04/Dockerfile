FROM ubuntu:bionic

# Make no effort to shrink this image as it is only for local development and
# we want to be able to exploit layer caching

RUN apt-get -y update
RUN apt-get -y --no-install-recommends install build-essential pkg-config 
RUN apt-get -y --no-install-recommends install libsdl2-2.0 libsdl2-dev 
RUN apt-get -y --no-install-recommends install libogg-dev libvorbis-dev libtheora-dev 
RUN apt-get -y --no-install-recommends install curl ca-certificates
RUN apt-get -y --no-install-recommends install git

RUN curl -O -L https://github.com/Kitware/CMake/releases/download/v3.13.4/cmake-3.13.4-Linux-x86_64.tar.gz
RUN tar -C /opt -xf cmake-3.13.4-Linux-x86_64.tar.gz
ENV PATH="/opt/cmake-3.13.4-Linux-x86_64/bin:${PATH}"

CMD /bin/bash