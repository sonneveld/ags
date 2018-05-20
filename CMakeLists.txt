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

add_library(AGSCommon STATIC
    ${common_sources}
)
target_link_libraries(AGS AGSCommon)

target_sources(AGS PRIVATE 
    ${engine_sources} 
    ${macos_sources}
    ${plugin_sources}
    ${lib_sources}
    ${test_sources}
    ${resources}
    )

source_group(TREE ${AdventureGameStudio_SOURCE_DIR} PREFIX Sources FILES 
    ${common_sources}
    ${engine_sources} 
    ${macos_sources}
    ${plugin_sources}
    ${lib_sources}
    ${test_sources}
)

source_group(TREE ${AdventureGameStudio_SOURCE_DIR} PREFIX Resources FILES 
    ${resources} 
)

target_include_directories(AGSCommon PRIVATE
    PUBLIC
    "${AdventureGameStudio_SOURCE_DIR}/Common"
    "${AdventureGameStudio_SOURCE_DIR}/Common/libinclude"
    "${AdventureGameStudio_SOURCE_DIR}/OSX/include"
    "${AdventureGameStudio_SOURCE_DIR}/OSX/include/freetype2"
) 

target_include_directories(AGS PRIVATE
    "${AdventureGameStudio_SOURCE_DIR}/Engine"
    "${AdventureGameStudio_SOURCE_DIR}/Engine/plugin"
) 

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

foreach(target_var AGS AGSCommon)
    target_compile_definitions(${target_var} PRIVATE 
        $<$<PLATFORM_ID:Windows>:WINDOWS_VERSION>
        $<$<PLATFORM_ID:Linux>:LINUX_VERSION>
        $<$<PLATFORM_ID:Darwin>:MAC_VERSION>

        $<$<CONFIG:Debug>:DEBUG>
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<CONFIG:Debug>:DEBUG_SPRITECACHE>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>

        $<$<STREQUAL:ENABLE_MP3,OFF>:NO_MP3_PLAYER>

        $<$<PLATFORM_ID:Windows>:WIN32>
        $<$<PLATFORM_ID:Windows>:_WINDOWS>
        $<$<PLATFORM_ID:Windows>:VS2005>
        $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>

        DISABLE_MPEG_AUDIO # apeg-1.2.1
        ALLEGRO_STATICLINK # used by allegro (if statically linked)
    )
endforeach(target_var)

# Engine specific
target_compile_definitions(AGS PRIVATE 
    $<$<PLATFORM_ID:Linux>:BUILTIN_PLUGINS> 
    $<$<PLATFORM_ID:Darwin>:BUILTIN_PLUGINS> 

    $<$<PLATFORM_ID:Windows>:AGS_HAS_CD_AUDIO>
    $<$<PLATFORM_ID:Linux>:AGS_HAS_CD_AUDIO>  

    $<$<PLATFORM_ID:Linux>:RTLD_NEXT> 

    $<$<CONFIG:Debug>:SOUND_CACHE_DEBUG>
    $<$<CONFIG:Debug>:DEBUG_MANAGED_OBJECTS>
    #$<$<CONFIG:Debug>:DEBUG_PATHFINDER>
)

# common specific
target_compile_definitions(AGSCommon PRIVATE 
    $<$<PLATFORM_ID:Linux>:AGS_CASE_SENSITIVE_FILESYSTEM> # common
)

if (APPLE)
    set_target_properties(AGS PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER uk.co.adventuregamestudio.AGS
        RESOURCE "${resources}"
    )
endif()

link_directories(${AdventureGameStudio_SOURCE_DIR}/OSX/lib)

target_link_libraries(AGS 

    $<$<CONFIG:Debug>:${AdventureGameStudio_SOURCE_DIR}/OSX/lib/libdumbd.a>
    $<$<NOT:$<CONFIG:Debug>>:${AdventureGameStudio_SOURCE_DIR}/OSX/lib/libdumb.a>

    $<$<CONFIG:Debug>:${AdventureGameStudio_SOURCE_DIR}/OSX/lib/libaldmd.a>
    $<$<NOT:$<CONFIG:Debug>>:${AdventureGameStudio_SOURCE_DIR}/OSX/lib/libaldmb.a>

    $<$<CONFIG:Debug>:${AdventureGameStudio_SOURCE_DIR}/OSX/lib/liballeg-debug.a>
    $<$<NOT:$<CONFIG:Debug>>:${AdventureGameStudio_SOURCE_DIR}/OSX/lib/liballeg.a>

    ${AdventureGameStudio_SOURCE_DIR}/OSX/lib/libfreetype.a

    ${AdventureGameStudio_SOURCE_DIR}/OSX/lib/liblua.a

    ${AdventureGameStudio_SOURCE_DIR}/OSX/lib/libogg.a
    ${AdventureGameStudio_SOURCE_DIR}/OSX/lib/libtheoradec.a
    ${AdventureGameStudio_SOURCE_DIR}/OSX/lib/libvorbis.a
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
