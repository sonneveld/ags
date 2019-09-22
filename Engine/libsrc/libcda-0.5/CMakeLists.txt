add_library(cda)

set_target_properties(cda PROPERTIES
    CXX_STANDARD 11
    CXX_EXTENSIONS NO
    C_STANDARD 11
    C_EXTENSIONS NO
)

target_include_directories(cda PUBLIC .)

target_sources(cda 
    PRIVATE
        libcda.h
        windows.c
        linux.c
)

if (WIN32)
    target_link_libraries(cda PUBLIC winmm)
endif()

add_library(Cda::Cda ALIAS cda)
