# base will already have Chocolatey installed
FROM cirrusci/windowsservercore:2019

ARG VISUALCPP_BUILD_TOOLS_VERSION=14.0.25420.1
RUN cinst visualcpp-build-tools --version %VISUALCPP_BUILD_TOOLS_VERSION% -y
