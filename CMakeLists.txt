cmake_minimum_required (VERSION 3.8)

project(AdventureGameStudio 
    VERSION 3.4.1.13 
    LANGUAGES CXX C)

set (CMAKE_CXX_STANDARD 11)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${AdventureGameStudio_SOURCE_DIR}/cmake")

include(FileList)

add_executable(AGS 
    MACOSX_BUNDLE 
    "")

target_sources(AGS PRIVATE 
    ${sources} 
    ${macos_sources}
    ${plugin_sources}
    ${lib_sources}
    #${test_sources}
    )

source_group(TREE ${AdventureGameStudio_SOURCE_DIR} PREFIX Sources FILES 
    ${sources} 
    ${macos_sources}
    ${plugin_sources}
    ${lib_sources}
    #${test_sources}
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

target_compile_definitions(AGS PRIVATE 
    DISABLE_MPEG_AUDIO 
    MAC_VERSION 
    BUILTIN_PLUGINS 
    DEBUG    
)

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
