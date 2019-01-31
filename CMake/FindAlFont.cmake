include(FetchContent)

FetchContent_Declare(
    alfontproject
    #SOURCE_DIR "/Users/nick.sonneveld/othernick/src/ags-paid/lib-alfont-sdl2"
    GIT_REPOSITORY "https://github.com/sonneveld/lib-alfont.git"
    GIT_TAG "alfont-1.9.1-ags-sdl2"
)
FetchContent_GetProperties(alfontproject)
if(NOT alfontproject_POPULATED)
    FetchContent_Populate(alfontproject)
    add_subdirectory(${alfontproject_SOURCE_DIR} ${alfontproject_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()