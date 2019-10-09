cmake_minimum_required(VERSION 3.10)
project(vorbisidec LANGUAGES C)

include(CheckIncludeFile)

add_library(vorbisidec)

target_include_directories(vorbisidec
    PUBLIC include
    PRIVATE src include/tremor
)

target_sources(vorbisidec PRIVATE
    include/tremor/config_types.h
    include/tremor/ivorbiscodec.h
    include/tremor/ivorbisfile.h
    src/asm_arm.h
    src/backends.h
    src/block.c
    src/block.h
    src/codebook.c
    src/codebook.h
    src/codec_internal.h
    src/floor0.c
    src/floor1.c
    src/info.c
    src/lsp_lookup.h
    src/mapping0.c
    src/mdct_lookup.h
    src/mdct.c
    src/mdct.h
    src/misc.h
    src/os.h
    src/registry.c
    src/registry.h
    src/res012.c
    src/sharedbook.c
    src/synthesis.c
    src/vorbisfile.c
    src/window_lookup.h
    src/window.c
    src/window.h
)

target_compile_options(vorbisidec PRIVATE -fsigned-char)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
    target_compile_definitions(vorbisidec PRIVATE _ARM_ASSEM_)
endif()

# enable 32 bit only multiply operations
# target_compile_definitions(vorbisidec PRIVATE _LOW_ACCURACY_)

CHECK_INCLUDE_FILE(alloca.h HAVE_ALLOCA_H)
if(HAVE_ALLOCA_H)
    target_compile_definitions(vorbisidec PRIVATE HAVE_ALLOCA_H)
endif()
CHECK_INCLUDE_FILE(memory.h USE_MEMORY_H)
if(USE_MEMORY_H)
    target_compile_definitions(vorbisidec PRIVATE USE_MEMORY_H)
endif()

target_link_libraries(vorbisidec ogg)

add_library(Vorbis::Tremor ALIAS vorbisidec)
