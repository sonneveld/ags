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

#include "gfx/gfxfilter_sdl_renderer.h"

namespace AGS
{
namespace Engine
{
namespace SDLRenderer
{

using namespace Common;

const GfxFilterInfo SDLRendererGfxFilter::FilterInfo = GfxFilterInfo("StdScale", "Nearest-neighbour");

const GfxFilterInfo &SDLRendererGfxFilter::GetInfo() const
{
    return FilterInfo;
}

} // namespace ALSW
} // namespace Engine
} // namespace AGS
