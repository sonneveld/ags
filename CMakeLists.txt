cmake_minimum_required (VERSION 3.8 FATAL_ERROR)

project(AdventureGameStudio 
    VERSION 3.4.1.13 
    LANGUAGES CXX C)

option(ENABLE_MP3 "Allow MP3 decoding")

# TODO:
# options for plugins - all, some, etc
# enable steam
# bundle game dir

set (CMAKE_CXX_STANDARD 11)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${AdventureGameStudio_SOURCE_DIR}/cmake")

include(FileList)

add_executable(AGS "")

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

if (APPLE)
    set_target_properties(AGS PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER uk.co.adventuregamestudio.AGS
        RESOURCE "${resources}"  # *must* be part of target sources too
    )
endif (APPLE)

if (WIN32)
    set_target_properties(AGS PROPERTIES
        WIN32 TRUE
    )
endif (WIN32)

target_sources(AGS PRIVATE 
    ${sources} 
    ${macos_sources}
    ${plugin_sources}
    ${lib_sources}
    ${test_sources}
    ${resources}
    )

source_group(TREE ${AdventureGameStudio_SOURCE_DIR} PREFIX Sources FILES 
    ${sources} 
    ${macos_sources}
    ${plugin_sources}
    ${lib_sources}
    ${test_sources}
)

source_group(TREE ${AdventureGameStudio_SOURCE_DIR} PREFIX Resources FILES 
    ${resources} 
)

include_directories(
    "${AdventureGameStudio_SOURCE_DIR}/Common"
    "${AdventureGameStudio_SOURCE_DIR}/Engine"
    "${AdventureGameStudio_SOURCE_DIR}/Engine/plugin"
    "${AdventureGameStudio_SOURCE_DIR}/Common/libinclude"
    "${AdventureGameStudio_SOURCE_DIR}/OSX/include"
    "${AdventureGameStudio_SOURCE_DIR}/OSX/include/freetype2"
) 

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

link_directories(${AdventureGameStudio_SOURCE_DIR}/OSX/lib)

if (WIN32)
    target_compile_definitions(AGS PRIVATE 
        WINDOWS_VERSION
        WIN32
        _WINDOWS
        VS2005
        _CRT_SECURE_NO_WARNINGS
        AGS_HAS_CD_AUDIO
        _LIB   # for common
        _CONSOLE   # for tests
    )
endif()

if (APPLE)
    target_compile_definitions(AGS PRIVATE 
        MAC_VERSION
        BUILTIN_PLUGINS
    )
endif()

if (LINUX)
    target_compile_definitions(AGS PRIVATE 
        LINUX_VERSION
        AGS_RUNTIME_PATCH_ALLEGRO
        AGS_HAS_CD_AUDIO
        AGS_CASE_SENSITIVE_FILESYSTEM
        BUILTIN_PLUGINS
        RTLD_NEXT
    )
endif()

target_compile_definitions(AGS PRIVATE 
    DISABLE_MPEG_AUDIO
    ALLEGRO_STATICLINK
)

if (!ENABLE_MP3)
    target_compile_definitions(AGS PRIVATE 
        NO_MP3_PLAYER
    )
endif()


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(AGS PRIVATE 
        DEBUG
        _DEBUG
        SOUND_CACHE_DEBUG
        DEBUG_SPRITECACHE
        DEBUG_MANAGED_OBJECTS
        #DEBUG_PATHFINDER  # broken
    )
else()
    target_compile_definitions(AGS PRIVATE 
        NDEBUG
    )
endif()


set_target_properties(AGS PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER uk.co.adventuregamestudio.AGS
    RESOURCE "${resources}"
)

target_link_libraries(AGS 

    # debug dumb
    ${AdventureGameStudio_SOURCE_DIR}/OSX/lib/libaldmd.a
    ${AdventureGameStudio_SOURCE_DIR}/OSX/lib/libdumbd.a

    # release dumb
    #${AdventureGameStudio_SOURCE_DIR}/OSX/lib/libaldmb.a
    #${AdventureGameStudio_SOURCE_DIR}/OSX/lib/libdumb.a

    ${AdventureGameStudio_SOURCE_DIR}/OSX/lib/liballeg-debug.a
    #${AdventureGameStudio_SOURCE_DIR}/OSX/lib/liballeg.a

    ${AdventureGameStudio_SOURCE_DIR}/OSX/lib/libfreetype.a

    ${AdventureGameStudio_SOURCE_DIR}/OSX/lib/liblua.a

    ${AdventureGameStudio_SOURCE_DIR}/OSX/lib/libogg.a
    # libtheora is deprecated
    #${AdventureGameStudio_SOURCE_DIR}/OSX/lib/libtheora.a
    ${AdventureGameStudio_SOURCE_DIR}/OSX/lib/libtheoradec.a
    #${AdventureGameStudio_SOURCE_DIR}/OSX/lib/libtheoraenc.a
    ${AdventureGameStudio_SOURCE_DIR}/OSX/lib/libvorbis.a
    #${AdventureGameStudio_SOURCE_DIR}/OSX/lib/libvorbisenc.a
    ${AdventureGameStudio_SOURCE_DIR}/OSX/lib/libvorbisfile.a
)

find_package(SDL2 REQUIRED)
if ( SDL2_FOUND )
    include_directories(${SDL2_INCLUDE_DIR})
    target_link_libraries(AGS ${SDL2_LIBRARY})
endif( SDL2_FOUND )

find_package( ZLIB REQUIRED )
if ( ZLIB_FOUND )
    include_directories( ${ZLIB_INCLUDE_DIRS} )
    target_link_libraries( AGS ${ZLIB_LIBRARIES} )
endif( ZLIB_FOUND )

find_package( BZIP2 REQUIRED )
if ( BZIP2_FOUND )
    include_directories( ${BZIP2_INCLUDE_DIRS} )
    target_link_libraries( AGS ${BZIP2_LIBRARIES} )
endif( BZIP2_FOUND )
