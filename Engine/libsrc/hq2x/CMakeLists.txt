add_library(hq2x)

set_target_properties(hq2x PROPERTIES
    CXX_STANDARD 11
    CXX_EXTENSIONS NO
)

target_include_directories(hq2x PUBLIC . )

target_sources(hq2x 
    PRIVATE
    hq2x3x.cpp
    hq2x3x.h
)

add_library(HQ2X::hq2x ALIAS hq2x)
