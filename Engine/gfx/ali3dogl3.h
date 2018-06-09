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
//
// OpenGL graphics factory
//
//=============================================================================

#ifndef __AGS_EE_GFX__ALI3DOGL3_H
#define __AGS_EE_GFX__ALI3DOGL3_H

#include "util/stdtr1compat.h"
#include TR1INCLUDE(memory)
#include "gfx/bitmap.h"
#include "gfx/ddb.h"
#include "gfx/gfxdriverfactorybase.h"
#include "gfx/gfxdriverbase.h"
#include "util/string.h"
#include "util/version.h"

#include "ogl_headers.h"

namespace AGS {
namespace Engine {

using Common::Bitmap;
using Common::String;
using Common::Version;


class OGL3DisplayModeList final : public IGfxModeList
{
public:
    ~OGL3DisplayModeList() override;
    int  GetModeCount() const override;
    bool GetMode(int index, DisplayMode &mode) const override;
};




IGfxDriverFactory *GetOGL3Factory();

} // namespace Engine
} // namespace AGS

#endif 
