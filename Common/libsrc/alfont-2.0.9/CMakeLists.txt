add_library(alfont)

set_target_properties(alfont PROPERTIES
    C_STANDARD 11
    C_EXTENSIONS NO
)

target_include_directories(alfont PUBLIC . )

target_sources(alfont 
    PRIVATE
    alfont.h
    alfont.c
)

target_link_libraries(alfont PUBLIC Allegro::Allegro FreeType::FreeType)

# This is not a good idea for now, current implementation of alfont assumes utf8 if ALFONT_LINUX set
# if(NOT WIN32)
#     target_compile_definitions(alfont PRIVATE ALFONT_LINUX)
# endif()

add_library(AlFont::AlFont ALIAS alfont)
