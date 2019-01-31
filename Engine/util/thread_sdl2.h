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
#include "debug/out.h"

namespace AGS
{
    namespace Engine
    {
        
        class SDL2Thread : public BaseThread
        {
        public:
            SDL2Thread()
            {
                _thread = NULL;
            }
            
            ~SDL2Thread()
            {
                Stop();
            }
            
            inline bool Create(AGSThreadEntry entryPoint, bool looping)
            {
                _looping = looping;
                _entry = entryPoint;
                // Thread creation is delayed till the thread is started
                return true;
            }
            
            inline bool Start()
            {
                if (_thread != NULL) { return false; }
                
                _thread = SDL_CreateThread(_thread_start, "AGS Thread", (void *)this);
                if (_thread == NULL) {
                    AGS::Common::Debug::Printf("SDL_CreateThread returned: %s", SDL_GetError());
                }
                
                return _thread != NULL;
            }
            
            bool Stop()
            {
                if (_thread == NULL) { return false; }
                
                _looping = false;
                int threadResult = -1;
                SDL_WaitThread(_thread, &threadResult);
                _thread = NULL;
                
                return threadResult == 0;
            }
            
        private:
            SDL_Thread *_thread;
            bool   _looping;
            
            AGSThreadEntry _entry;
            
            static int _thread_start(void *arg)
            {
                AGSThreadEntry entry = ((SDL2Thread *)arg)->_entry;
                bool *looping = &((SDL2Thread *)arg)->_looping;
                
                do {
                    entry();
                } while (*looping);
                
                return 0;
            }
        };
        
        
        typedef SDL2Thread Thread;
        
        
    } // namespace Engine
} // namespace AGS

#endif // __AGS_EE_PLATFORM__THREAD_SDL2_H
