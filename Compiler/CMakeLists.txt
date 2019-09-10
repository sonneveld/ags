add_library(compiler)

set_target_properties(compiler PROPERTIES
    CXX_STANDARD 11
    CXX_EXTENSIONS NO
)

target_include_directories(compiler PUBLIC .)

target_sources(compiler 
    PRIVATE
    fmem.cpp
    script/cc_compiledscript.cpp
    script/cc_internallist.cpp
    script/cc_macrotable.cpp
    script/cc_symboltable.cpp
    script/cc_treemap.cpp
    script/cs_compiler.cpp
    script/cs_parser.cpp
    script/cs_parser_common.cpp
    script/cs_prepro.cpp
)

target_link_libraries(compiler PUBLIC AGS::Common)

get_target_property(COMMON_SOURCES compiler SOURCES)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${COMMON_SOURCES})

add_library(AGS::Compiler ALIAS compiler)



add_executable(agscc agscc.cpp)
target_link_libraries(agscc PUBLIC AGS::Compiler)
