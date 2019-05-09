add_library(mojoal)

set_target_properties(mojoal PROPERTIES
    C_STANDARD 11
    C_EXTENSIONS NO
)

target_include_directories(mojoal PUBLIC . )

target_sources(mojoal 
    PRIVATE
    mojoal.c                       
    AL/al.h                        
    AL/alc.h                       
)

target_link_libraries(mojoal PUBLIC SDL2::SDL2)

add_library(OpenAL::OpenAL ALIAS mojoal)
