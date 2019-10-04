ARG FROM_DEBIAN=debian:latest
FROM $FROM_DEBIAN

# Take default debconf options
ENV DEBIAN_FRONTEND noninteractive

# Configure apt defaults
ARG APT_CONF_LOCAL=99local
RUN mkdir -p /etc/apt/apt.conf.d && \
  printf 'APT::Get::Assume-Yes "true";\n\
APT::Get::Install-Recommends "false";\n\
APT::Get::Install-Suggests "false";\n' > /etc/apt/apt.conf.d/$APT_CONF_LOCAL

# Upgrade existing packages
RUN apt-get update && apt-get upgrade

# Get build dependencies
RUN apt-get install \
  build-essential \
  cmake \
  git \
  libasound2-dev \
  libjack-dev \
  libsdl2-dev
