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

target_link_libraries(alogg PUBLIC Allegro::Allegro)

if (AGS_USE_TREMOR)
    target_link_libraries(alogg PUBLIC Vorbis::Tremor)
    target_compile_definitions(alogg PRIVATE USE_TREMOR)
else()
    target_link_libraries(alogg PUBLIC Vorbis::Vorbis Vorbis::VorbisFile)
endif()

if (WIN32 AND BUILD_SHARED_LIBS)
    target_compile_definitions(alogg INTERFACE ALOGG_DLL)
endif()

add_library(AlOgg::AlOgg ALIAS alogg)
