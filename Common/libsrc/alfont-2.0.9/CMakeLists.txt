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

if(NOT WIN32)
    target_compile_definitions(alfont PRIVATE ALFONT_LINUX)
endif()

add_library(AlFont::AlFont ALIAS alfont)
