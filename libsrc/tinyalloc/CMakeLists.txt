add_library(tinyalloc)

set_target_properties(tinyalloc PROPERTIES
    CXX_STANDARD 11
    CXX_EXTENSIONS NO
    C_STANDARD 11
    C_EXTENSIONS NO
)

target_include_directories(tinyalloc PUBLIC .)

target_sources(tinyalloc 
    PRIVATE
    tinyalloc.c
    tinyalloc.h
    tinydebug.c
)

# target_compile_definitions(tinyalloc PRIVATE TA_DEBUG)

add_library(TinyAlloc::TinyAlloc ALIAS tinyalloc)
