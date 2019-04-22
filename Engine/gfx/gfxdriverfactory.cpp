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

#include "gfx/gfxdriverfactory.h"
#include "ee_gfx.h"

#if defined(WINDOWS_VERSION) || defined(ANDROID_VERSION) || defined(IOS_VERSION) || defined(LINUX_VERSION)
#endif

#if defined(WINDOWS_VERSION)
#include "ee_platform.h"
#endif

#include "ee_main.h"

namespace AGS
{
namespace Engine
{

void GetGfxDriverFactoryNames(StringV &ids)
{
#ifdef WINDOWS_VERSION
    ids.push_back("D3D9");
#endif
#if defined (ANDROID_VERSION) || defined (IOS_VERSION) || defined (WINDOWS_VERSION) || defined(LINUX_VERSION)
    ids.push_back("OGL");
#endif
    ids.push_back("Software");
}

IGfxDriverFactory *GetGfxDriverFactory(const String id)
{
#ifdef WINDOWS_VERSION
    if (id.CompareNoCase("D3D9") == 0)
        return D3D::D3DGraphicsFactory::GetFactory();
#endif
#if defined (ANDROID_VERSION) || defined (IOS_VERSION)|| defined (WINDOWS_VERSION) || defined (LINUX_VERSION)
    if (id.CompareNoCase("OGL") == 0)
        return OGL::OGLGraphicsFactory::GetFactory();
#endif
    if (id.CompareNoCase("Software") == 0)
        return ALSW::ALSWGraphicsFactory::GetFactory();
    set_allegro_error("No graphics factory with such id: %s", id.GetCStr());
    return nullptr;
}

} // namespace Engine
} // namespace AGS
