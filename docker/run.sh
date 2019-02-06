#!/bin/bash
docker run -v $HOME:$HOME -w $PWD -ti --rm ags-build:ubuntu-18.04 /bin/bash
