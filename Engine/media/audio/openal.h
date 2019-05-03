#ifndef AC_MEDIA_AUDIO_OPENAL_H__
#define AC_MEDIA_AUDIO_OPENAL_H__

#include "core/platform.h"

#if AGS_PLATFORM_OS_MACOS
#include <OpenAL/OpenAL.h>
#else
#include "al.h"
#include "alc.h"
// disabled until I add extension detection
// #include "alext.h"
#endif

#endif