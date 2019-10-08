add_library(agspalrender)

set_target_properties(agspalrender PROPERTIES
    CXX_STANDARD 11
    CXX_EXTENSIONS NO
    C_STANDARD 11
    C_EXTENSIONS NO
)

target_include_directories(agspalrender PUBLIC .)

target_sources(agspalrender
    PRIVATE
    agsplugin.h

    palrender.h
    # agspalrender.h
    ags_palrender.cpp
    raycast.cpp
    raycast.h
)