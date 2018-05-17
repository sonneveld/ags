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

#ifndef __AGS_EE_UTIL__SDL2_MUTEX_H
#define __AGS_EE_UTIL__SDL2_MUTEX_H

#include <stdexcept>
#include "SDL.h"
#include "debug/out.h"

namespace AGS
{
    namespace Engine
    {
        
        class SDL2Mutex : public BaseMutex
        {
        public:
            SDL2Mutex()
            {
                _mutex = SDL_CreateMutex();
                if (_mutex == NULL) {
                    AGS::Common::Debug::Printf("SDL_CreateMutex returned: %s", SDL_GetError());
                    throw std::runtime_error(SDL_GetError());
                }
            }
            
            virtual ~SDL2Mutex()
            {
                SDL_DestroyMutex(_mutex);
                _mutex = NULL;
            }
            
            inline void Lock()
            {
                int result = SDL_LockMutex(_mutex);
                if (result != 0) {
                    AGS::Common::Debug::Printf("SDL_LockMutex returned: %s", SDL_GetError());
                }
                SDL_assert(result == 0);
            }
            
            inline void Unlock()
            {
                int result = SDL_UnlockMutex(_mutex);
                if (result != 0) {
                    AGS::Common::Debug::Printf("SDL_UnlockMutex returned: %s", SDL_GetError());
                }
                SDL_assert(result == 0);
            }
            
        private:
            SDL_mutex *_mutex;
        };
        
        typedef SDL2Mutex Mutex;
        
    } // namespace Engine
} // namespace AGS

#endif // __AGS_EE_UTIL__SDL2_MUTEX_H
