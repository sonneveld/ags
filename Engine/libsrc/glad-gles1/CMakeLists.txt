add_library(glad-gles1)

set_target_properties(glad-gles1 PROPERTIES
    C_STANDARD          11
    C_EXTENSIONS        NO
)

target_include_directories(glad-gles1 PUBLIC include)

target_sources(glad-gles1 
    PRIVATE
        include/KHR/khrplatform.h
        include/glad/glad.h
        src/glad.c
)

target_link_libraries(glad-gles1 PUBLIC ${CMAKE_DL_LIBS})

add_library(Glad::GladGLES1 ALIAS glad-gles1)
