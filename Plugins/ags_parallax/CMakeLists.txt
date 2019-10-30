add_library(ags_parallax)

set_target_properties(ags_parallax PROPERTIES
    CXX_STANDARD 11
    CXX_EXTENSIONS NO
    C_STANDARD 11
    C_EXTENSIONS NO
)

target_include_directories(ags_parallax PUBLIC .)

target_sources(ags_parallax
    PRIVATE
    agsplugin.h

    # ags_parallax.h
    ags_parallax.cpp
)