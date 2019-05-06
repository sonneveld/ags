//=============================================================================
//
// Adventure Game Studio (AGS)
//
// Copyright (C) 1999-2011 Chris Jones and 2011-20xx others
// The full list of copyright holders can be found in the Copyright.txt
// file, which is part of this source code distribution.
//
// The AGS source code is provided under the Artistic License 2.0.
// A copy of this license can be found in the file License.txt and at
// http://www.opensource.org/licenses/artistic-license-2.0.php
//
//=============================================================================

#ifndef __AC_AUDIOCLIPTYPE_H
#define __AC_AUDIOCLIPTYPE_H

#include <memory>

// Forward declaration
namespace AGS { namespace Common { class Stream; } }
using namespace AGS; // FIXME later

#define AUCL_BUNDLE_EXE 1
#define AUCL_BUNDLE_VOX 2

#define AUDIO_CLIP_TYPE_SOUND 1
struct AudioClipType {
    int id;
    int reservedChannels;
    int volume_reduction_while_speech_playing;
    int crossfadeSpeed;
    int reservedForFuture;

    void ReadFromFile(std::shared_ptr<AGS::Common::Stream> in);
    void WriteToFile(std::shared_ptr<AGS::Common::Stream> out);
    void ReadFromSavegame(std::shared_ptr<AGS::Common::Stream> in);
    void WriteToSavegame(std::shared_ptr<AGS::Common::Stream> out) const;
};

#endif // __AC_AUDIOCLIPTYPE_H