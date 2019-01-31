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

#ifndef __AGS_EE_PLATFORM__THREAD_SDL2_H
#define __AGS_EE_PLATFORM__THREAD_SDL2_H

#include "SDL.h"

namespace AGS
{
namespace Engine
{

class SDL2Thread : public BaseThread
{
public:
  SDL2Thread() : thread_(nullptr), entry_(nullptr), looping_(false)
  {
  }

  ~SDL2Thread() override
  {
    Stop();
  }

  SDL2Thread &operator=(const SDL2Thread &) = delete;
  SDL2Thread(const SDL2Thread &) = delete;

  bool Create(AGSThreadEntry entryPoint, bool looping) override
  {
    if (!entryPoint) { return false; }

    entry_ = entryPoint;
    looping_ = looping;
    return true;
  }

  bool Start() override
  {
    if (!entry_) { return false; }

    thread_ = SDL_CreateThread(thread_start_, "SDL2 Thread", this);
    return thread_ != nullptr;
  }

  bool Stop() override
  {
    looping_ = false; // signal thread to stop
    SDL_WaitThread(thread_, nullptr);
    return true;
  }


private:
  SDL_Thread *thread_;
  AGSThreadEntry entry_;
  bool looping_;

  static int thread_start_(void *data)
  {
    auto self = static_cast<SDL2Thread*>(data);
    auto entry = self->entry_;
    for (;;)
    {
      entry();
      if (!self->looping_) { break; }
      SDL_Delay(1);
    }
    return 0;
  }
};

typedef SDL2Thread Thread;

} // namespace Engine
} // namespace AGS

#endif // __AGS_EE_PLATFORM__THREAD_SDL2_H
