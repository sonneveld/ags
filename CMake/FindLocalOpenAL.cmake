# Grab system openAL or use embedded mojoAL

find_package(OpenAL)

if (OPENAL_FOUND)
    add_library(openal-interface INTERFACE)
    target_link_libraries(openal-interface INTERFACE ${OPENAL_LIBRARY})
    target_include_directories(openal-interface INTERFACE ${OPENAL_INCLUDE_DIR})
    add_library(External::OpenAL ALIAS openal-interface)
else()
    add_subdirectory(libsrc/mojoAL-47392e0ffefd EXCLUDE_FROM_ALL)

    add_library(openal-interface INTERFACE)
    target_link_libraries(openal-interface INTERFACE MojoAL::MojoAL)
    add_library(External::OpenAL ALIAS openal-interface)
endif()

# at some point I was grabbing the osx openal framework directly. I don't know why
# find_library(OPENAL_FRAMEWORK OpenAL)
# if (NOT OPENAL_FRAMEWORK_NOTFOUND)
#     add_library(openal-interface INTERFACE)
#     target_link_libraries(openal-interface INTERFACE ${OPENAL_FRAMEWORK})
#     add_library(External::OpenAL ALIAS openal-interface)
# endif()
