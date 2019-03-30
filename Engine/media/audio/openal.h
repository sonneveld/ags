#ifndef AC_MEDIA_AUDIO_OPENAL_H__
#define AC_MEDIA_AUDIO_OPENAL_H__

#ifdef MAC_VERSION
#include <OpenAL/OpenAL.h>
#else
#include "AL/al.h"
#include "AL/alc.h"
#include "AL/alext.h"
#endif

#endif