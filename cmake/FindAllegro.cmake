include(ExternalProject)

ExternalProject_Add (
    AllegroDebugProject
    GIT_REPOSITORY "git@github.com:sonneveld/allegro.git"
    #GIT_REPOSITORY "file://$ENV{HOME}/othernick/src/ags-paid/allegro"
    GIT_TAG "sdl2-flat"
    GIT_SHALLOW 1
    INSTALL_DIR ${PROJECT_BINARY_DIR}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR> -DSHARED=off -DCMAKE_BUILD_TYPE=Debug 
)

ExternalProject_Get_Property(AllegroDebugProject INSTALL_DIR)

add_library(Allegro::Allegro STATIC IMPORTED)
set_property(TARGET Allegro::Allegro PROPERTY IMPORTED_CONFIGURATIONS "Debug")
set_property(TARGET Allegro::Allegro PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/lib/liballeg-debug.a)
file(MAKE_DIRECTORY "${INSTALL_DIR}/include") # needs to be created ahead of time
set_property(TARGET Allegro::Allegro PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include)
set_property(TARGET Allegro::Allegro PROPERTY INTERFACE_LINK_LIBRARIES SDL2::SDL2)

add_dependencies(Allegro::Allegro AllegroDebugProject)
