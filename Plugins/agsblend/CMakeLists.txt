add_library(agsblend)

set_target_properties(agsblend PROPERTIES
    CXX_STANDARD 11
    CXX_EXTENSIONS NO
    C_STANDARD 11
    C_EXTENSIONS NO
)

target_include_directories(agsblend PUBLIC .)

target_sources(agsblend
    PRIVATE
    agsplugin.h

    AGSBlend.cpp
    # agsblend.h
)