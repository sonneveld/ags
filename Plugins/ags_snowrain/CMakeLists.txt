add_library(ags_snowrain)

set_target_properties(ags_snowrain PROPERTIES
    CXX_STANDARD 11
    CXX_EXTENSIONS NO
    C_STANDARD 11
    C_EXTENSIONS NO
)

target_include_directories(ags_snowrain PUBLIC .)

target_sources(ags_snowrain
    PRIVATE
    agsplugin.h

    # ags_snowrain.h
    ags_snowrain.cpp
)