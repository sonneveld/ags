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

#ifndef __AGS_EE_UTIL__LIBRARY_H
#define __AGS_EE_UTIL__LIBRARY_H

#include "core/platform.h"
#include "util/string.h"

namespace AGS
{
namespace Engine
{


class BaseLibrary
{
public:
  BaseLibrary() = default;

  virtual ~BaseLibrary() = default;

  virtual AGS::Common::String GetFilenameForLib(AGS::Common::String libraryName) = 0;

  virtual bool Load(AGS::Common::String libraryName) = 0;

  virtual bool Unload() = 0;

  virtual void *GetFunctionAddress(AGS::Common::String functionName) = 0;
};


} // namespace Engine
} // namespace AGS

#include "library_sdl2.h"

#endif // __AGS_EE_UTIL__MUTEX_H
