add_library(alogg)

set_target_properties(alogg PROPERTIES
    C_STANDARD 11
    C_EXTENSIONS NO
)

target_include_directories(alogg PUBLIC . )

target_sources(alogg 
    PRIVATE
    alogg.c
)

target_link_libraries(alogg PUBLIC Allegro::Allegro Vorbis::Vorbis Vorbis::VorbisFile)

# for consider for future tremor support:
# if (ALOGG_USE_TREMOR)
# target_link_libraries(alogg PUBLIC Tremor::Tremor)
# target_compile_definitions(alogg PRIVATE USE_TREMOR)
# endif()

if (WIN32 AND BUILD_SHARED_LIBS)
    target_compile_definitions(alogg INTERFACE ALOGG_DLL)
endif()

add_library(AlOgg::AlOgg ALIAS alogg)
