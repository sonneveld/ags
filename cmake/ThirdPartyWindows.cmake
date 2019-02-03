set(SDL2_ROOT_DIR "SDL2-2.0.8")
find_package(SDL2 REQUIRED)

add_library(Ogg::Ogg STATIC IMPORTED)
set_property(TARGET Ogg::Ogg PROPERTY IMPORTED_LOCATION ${OGG_PREFIX}/lib/libogg_static.lib)
set_property(TARGET Ogg::Ogg PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${OGG_PREFIX}/include)

add_library(Theora::Theora STATIC IMPORTED)
set_property(TARGET Theora::Theora PROPERTY IMPORTED_LOCATION ${THEORA_PREFIX}/lib/libtheora_static.lib)
set_property(TARGET Theora::Theora PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${THEORA_PREFIX}/include)

add_library(Vorbis::Vorbis STATIC IMPORTED)
set_property(TARGET Vorbis::Vorbis PROPERTY IMPORTED_LOCATION ${VORBIS_PREFIX}/lib/libvorbis_static.lib)
set_property(TARGET Vorbis::Vorbis PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${VORBIS_PREFIX}/include)

add_library(Vorbis::VorbisFile STATIC IMPORTED)
set_property(TARGET Vorbis::VorbisFile PROPERTY IMPORTED_LOCATION ${VORBIS_PREFIX}/lib/libvorbisfile_static.lib)
set_property(TARGET Vorbis::VorbisFile PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${VORBIS_PREFIX}/include)
