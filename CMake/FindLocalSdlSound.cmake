set(SDLSOUND_BUILD_SHARED off CACHE BOOL "no shared")
set(SDLSOUND_BUILD_TEST off CACHE BOOL "no tests")
set(SDLSOUND_BUILD_STATIC on CACHE BOOL "static")
add_subdirectory(libsrc/SDL_sound-9262f9205898-20180720 EXCLUDE_FROM_ALL)

add_library(sdl2_sound-interface INTERFACE)
target_link_libraries(sdl2_sound-interface INTERFACE SDL2_sound-static)

add_library(SDL2_sound::SDL2_sound ALIAS sdl2_sound-interface)
