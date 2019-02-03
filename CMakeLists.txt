# TODO:
# options for plugins - all, some, etc
# enable steam
# bundle game dir

# https://gitlab.kitware.com/cmake/community/wikis/doc/cmake/Life-Cycle-Considerations
cmake_minimum_required (VERSION 3.11)

project(AdventureGameStudio
    VERSION 3.4.1.13
    LANGUAGES CXX C)

if(WIN32)
    set (THIRDPARTY_PREFIX ${AdventureGameStudio_SOURCE_DIR}/Windows)
elseif(APPLE)
    set (THIRDPARTY_PREFIX ${AdventureGameStudio_SOURCE_DIR}/OSX)
else()
    set (THIRDPARTY_PREFIX /usr/local)
endif()

set (ALLEGRO_PREFIX ${THIRDPARTY_PREFIX})
set (ALFONT_PREFIX ${THIRDPARTY_PREFIX})
set (LUA_PREFIX ${THIRDPARTY_PREFIX})
set (OGG_PREFIX ${THIRDPARTY_PREFIX})
set (THEORA_PREFIX ${THIRDPARTY_PREFIX})
set (VORBIS_PREFIX ${THIRDPARTY_PREFIX})

option(NO_MP3_PLAYER "Disable MP3 decoding" OFF)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${AdventureGameStudio_SOURCE_DIR}/cmake")

include(TestBigEndian)

include(FileList)

if(WIN32)
    include(ThirdPartyWindows)
elseif(APPLE)
    include(ThirdParty)
else()
    include(ThirdPartyLinux)
endif()

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

find_package(SDL2 REQUIRED)
find_package(Allegro REQUIRED)
find_package(alfont REQUIRED)

# GLAD
# ------------------------------------------------------------

add_library(glad STATIC libsrc/glad/src/glad.c)
target_include_directories(glad PRIVATE PUBLIC "libsrc/glad/include")
if(UNIX)
    set_property(TARGET glad PROPERTY INTERFACE_LINK_LIBRARIES dl)
endif()

# AGS Common
# ------------------------------------------------------------

add_library(AGSCommon STATIC
    ${common_sources}
)

target_include_directories(AGSCommon PRIVATE
    PUBLIC
    "${AdventureGameStudio_SOURCE_DIR}/Common"
    "${AdventureGameStudio_SOURCE_DIR}/Common/libinclude"
)

# common specific
target_compile_definitions(AGSCommon PRIVATE
    $<$<PLATFORM_ID:Linux>:AGS_CASE_SENSITIVE_FILESYSTEM> # common
)

target_link_libraries(AGSCommon
    SDL2::SDL2
    allegro
)


# AGS
# ------------------------------------------------------------

add_executable(AGS "")

if (APPLE)
    set_target_properties(AGS PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER uk.co.adventuregamestudio.AGS
        RESOURCE "${resources}"  # *must* be part of target sources too
    )
endif (APPLE)

if (WIN32)
    set_target_properties(AGS PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

target_sources(AGS PRIVATE
    ${engine_sources}
    $<$<PLATFORM_ID:Windows>:
        ${windows_sources}
    >
    $<$<PLATFORM_ID:Darwin>:
        ${macos_sources}
        ${plugin_sources}
    >
    $<$<PLATFORM_ID:Linux>:
        ${linux_sources}
        ${plugin_sources}
    >
    ${lib_sources}
    ${test_sources}
    ${resources}
    )

target_include_directories(AGS PRIVATE
    "${AdventureGameStudio_SOURCE_DIR}/Engine"
    "${AdventureGameStudio_SOURCE_DIR}/Engine/plugin" # for code that includes 'agsplugin.h' directly
    "${AdventureGameStudio_SOURCE_DIR}/Windows/include"
)

target_compile_definitions(AGS PRIVATE
    $<$<PLATFORM_ID:Linux>:BUILTIN_PLUGINS>
    $<$<PLATFORM_ID:Darwin>:BUILTIN_PLUGINS>

    $<$<PLATFORM_ID:Windows>:AGS_HAS_CD_AUDIO>
    $<$<PLATFORM_ID:Linux>:AGS_HAS_CD_AUDIO>

    $<$<PLATFORM_ID:Linux>:RTLD_NEXT>

    $<$<CONFIG:Debug>:SOUND_CACHE_DEBUG>
    $<$<CONFIG:Debug>:DEBUG_MANAGED_OBJECTS>
    #$<$<CONFIG:Debug>:DEBUG_PATHFINDER>

    $<$<PLATFORM_ID:Windows>:AGS_ENABLE_OPENGL_DRIVER>
    $<$<PLATFORM_ID:Linux>:AGS_ENABLE_OPENGL_DRIVER>
    # $<$<PLATFORM_ID:Android>:AGS_ENABLE_OPENGL_DRIVER>
    # $<$<PLATFORM_ID:IOS>:AGS_ENABLE_OPENGL_DRIVER>
    $<$<PLATFORM_ID:Darwin>:AGS_ENABLE_OPENGL_DRIVER>
)

if (APPLE)
    set_target_properties(AGS PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER uk.co.adventuregamestudio.AGS
        RESOURCE "${resources}"
    )
endif()

target_link_libraries(AGS
    AGSCommon
    SDL2::SDL2main
    allegro
    alfont
    # Lua::Lua
    Ogg::Ogg
    Theora::Theora
    Vorbis::Vorbis
    Vorbis::VorbisFile
    glad

    $<$<PLATFORM_ID:Windows>:Shlwapi>   # for some shell functions
    $<$<PLATFORM_ID:Windows>:Winmm>     # for cd playing
)


# Common to both targets
# ------------------------------------------------------------

TEST_BIG_ENDIAN(AGS_BIG_ENDIAN)

foreach(target_var AGS AGSCommon)

    target_compile_definitions(${target_var} PRIVATE
        $<$<PLATFORM_ID:Windows>:WINDOWS_VERSION>
        $<$<PLATFORM_ID:Linux>:LINUX_VERSION>
        $<$<PLATFORM_ID:Darwin>:MAC_VERSION>

        $<$<CONFIG:Debug>:DEBUG>
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<CONFIG:Debug>:DEBUG_SPRITECACHE>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>

        $<$<BOOL:${NO_MP3_PLAYER}>:NO_MP3_PLAYER>

        $<$<BOOL:${AGS_BIG_ENDIAN}>:AGS_BIG_ENDIAN>

        # $<$<PLATFORM_ID:Windows>:WIN32> # predefined on windows
        # $<$<PLATFORM_ID:Windows>:_WINDOWS> # predefined on windows
        $<$<PLATFORM_ID:Windows>:VS2005>
        $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>

        $<$<PLATFORM_ID:Linux>:_GLIBCXX_USE_CXX11_ABI>

        DISABLE_MPEG_AUDIO # apeg-1.2.1
        ALLEGRO_STATICLINK # used by allegro (if statically linked)
    )

endforeach(target_var)

foreach(target_var AGS AGSCommon glad)

    set_target_properties(${target_var} PROPERTIES
        C_STANDARD 11
        CXX_STANDARD 11
    )

    if(WIN32)
        target_compile_options(${target_var} PRIVATE
            $<$<CONFIG:Debug>:
                /Od
                /Zi    # debug info format
                /MDd   # runtime lib
            >
            $<$<CONFIG:Release>:
                /O2
                /MD   # runtime lib
            >
            /W2     # warning level
            /MP     # multi processor builds
            /GR     # run time type information
        )
    else()

        # Refer to https://developers.redhat.com/blog/2018/03/21/compiler-and-linker-flags-gcc/

        target_compile_options(${target_var} PRIVATE
            $<$<CONFIG:Debug>: -Og >
            $<$<CONFIG:Release>: -O2 >

            -fsigned-char
            -fno-strict-aliasing
            -fwrapv
            # -fwrapv-pointer
            -fPIC

            -Wall
            -Wextra

            -Wendif-labels
            -Wfloat-equal
            -Winit-self
            -Winline
            -Wmissing-noreturn
            -Wpointer-arith
            -Wshadow
            -Wundef
            -Wwrite-strings

            # probably need fixing but disable until we have time
            -Wno-unknown-pragmas
            -Wno-deprecated-declarations
            -Wno-unused-parameter
            -Wno-sign-compare
            -Wno-cast-align
            -Wno-cast-qual
            -Wno-missing-declarations
            -Wno-switch-enum
            # -Wlarger-than-4096
            -Wno-redundant-decls

            -Werror=write-strings
            -Werror=format-security
            -Werror=implicit-function-declaration

            $<$<COMPILE_LANGUAGE:CXX>:
                -Wno-old-style-cast
                -Werror=delete-non-virtual-dtor
            >

            $<$<COMPILE_LANGUAGE:C>:
                -Wbad-function-cast
                -Wdeclaration-after-statement
                -Wno-missing-prototypes
                -Wold-style-definition
                -Wstrict-prototypes
            >
        )
    endif()

endforeach(target_var)


# Source Groups for IDEs
# ------------------------------------------------------------

# Tree source groups are only available in 3.8 and above, but their main benefit is making
# project files look tidier in IDEs so can consider this optional.
if (NOT (${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} LESS 3.8))

    source_group(TREE ${AdventureGameStudio_SOURCE_DIR} PREFIX Sources FILES
        ${common_sources}
        ${engine_sources}
        $<$<PLATFORM_ID:Windows>:${windows_sources}>
        $<$<PLATFORM_ID:Darwin>:
            ${macos_sources}
            ${plugin_sources}
        >
        $<$<PLATFORM_ID:Linux>:
            ${linux_sources}
            ${plugin_sources}
        >
        ${lib_sources}
        ${test_sources}
    )

    source_group(TREE ${AdventureGameStudio_SOURCE_DIR} PREFIX Resources FILES
        ${resources}
    )

endif()
