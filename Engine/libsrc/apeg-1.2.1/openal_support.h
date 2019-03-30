#ifndef APEG_OPENAL_SUPPORT_H__
#define APEG_OPENAL_SUPPORT_H__

#ifdef __APPLE__
#include <OpenAL/OpenAL.h>
#else
#include "al.h"
#include "alc.h"
// disabled until I add extension detection
// #include "alext.h"
#endif

#endif
