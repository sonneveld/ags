add_library(pe)

target_sources(pe
    PRIVATE
    pe_jni.c
)

find_library(log-lib log)

target_link_libraries(pe engine ${log-lib})
