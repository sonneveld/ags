add_library(agsflashlight)

set_target_properties(agsflashlight PROPERTIES
    CXX_STANDARD 11
    CXX_EXTENSIONS NO
    C_STANDARD 11
    C_EXTENSIONS NO
)

target_include_directories(agsflashlight PUBLIC .)

target_sources(agsflashlight
    PRIVATE
    agsplugin.h

    # agsflashlight.h
    agsflashlight.cpp
)