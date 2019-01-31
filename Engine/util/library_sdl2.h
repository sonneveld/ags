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

#ifndef __AGS_EE_UTIL__LIBRARY_SDL2_H
#define __AGS_EE_UTIL__LIBRARY_SDL2_H

#include "core/platform.h"
#include "SDL.h"
#include "util/string.h"
#include "debug/out.h"

// FIXME: Replace with a unified way to get the directory which contains the engine binary
#if AGS_PLATFORM_OS_ANDROID
extern char android_app_directory[256];
#else
extern AGS::Common::String appDirectory;
#endif

namespace AGS
{
    namespace Engine
    {
        
        class SDL2Library : public BaseLibrary
        {
        public:
            SDL2Library()
            : _library(NULL)
            {
            };
            
            virtual ~SDL2Library()
            {
                Unload();
            };
            
            AGS::Common::String GetFilenameForLib(AGS::Common::String libraryName) 
            {
                auto fname = AGS::Common::String();

                #if ! AGS_PLATFORM_OS_WINDOWS
                fname.Append("lib");
                #endif
                fname.Append(libraryName);
                                
                #if AGS_PLATFORM_OS_WINDOWS
                    fname.Append(".dll");
                #elif AGS_PLATFORM_OS_MACOS
                    fname.Append(".dylib");
                #else
                    fname.Append(".so");
                #endif

                return fname;
            }

            AGS::Common::String BuildPath(const char *path, AGS::Common::String libraryName)
            {
                auto resolvedPath = AGS::Common::String();

                if (path) {
                    resolvedPath = path;
                    resolvedPath.Append("/");
                }

                resolvedPath.Append(GetFilenameForLib(libraryName));
                
                AGS::Common::Debug::Printf("Built library path: %s", resolvedPath.GetCStr());
                return resolvedPath;
            }
            
            bool Load(AGS::Common::String libraryName)
            {
                Unload();
                
                // Try rpath first
                _library = SDL_LoadObject(BuildPath(NULL, libraryName).GetCStr());
                if (_library != NULL) { return true; }
                AGS::Common::Debug::Printf("SDL_LoadObject returned: %s", SDL_GetError());
                
                // Try current path
                _library = SDL_LoadObject(BuildPath(".", libraryName).GetCStr());
                if (_library != NULL) { return true; }
                AGS::Common::Debug::Printf("SDL_LoadObject returned: %s", SDL_GetError());
                
                // Try the engine directory
#if AGS_PLATFORM_OS_ANDROID
                char buffer[200];
                sprintf(buffer, "%s%s", android_app_directory, "/lib");
                _library = SDL_LoadObject(BuildPath(buffer, libraryName).GetCStr());
                if (_library != NULL) { return true; }
                AGS::Common::Debug::Printf("SDL_LoadObject returned: %s", SDL_GetError());
#else
                _library = SDL_LoadObject(BuildPath(appDirectory.GetCStr(), libraryName).GetCStr());
                if (_library != NULL) { return true; }
                AGS::Common::Debug::Printf("SDL_LoadObject returned: %s", SDL_GetError());
#endif
                
                return false;
            }
            
            bool Unload()
            {
                if (!_library) { return false; }
                SDL_UnloadObject(_library);
                return true;
            }
            
            void *GetFunctionAddress(AGS::Common::String functionName)
            {
                if (!_library) { return NULL; }
                
                void *result = SDL_LoadFunction(_library, functionName.GetCStr());
                if (result == NULL) {
                    AGS::Common::Debug::Printf("SDL_LoadFunction returned: %s", SDL_GetError());
                }
                return result;
            }
            
        private:
            void *_library;
        };
        
        
        typedef SDL2Library Library;
        
    } // namespace Engine
} // namespace AGS



#endif // __AGS_EE_UTIL__LIBRARY_SDL2_H
