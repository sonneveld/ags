add_library(agstouch)

set_target_properties(agstouch PROPERTIES
    CXX_STANDARD 11
    CXX_EXTENSIONS NO
    C_STANDARD 11
    C_EXTENSIONS NO
)

target_include_directories(agstouch PUBLIC .)

target_sources(agstouch
    PRIVATE
    agsplugin.h

    agstouch.cpp
    # agstouch.h 
)