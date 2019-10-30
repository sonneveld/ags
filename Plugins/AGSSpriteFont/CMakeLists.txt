add_library(agsspritefont)

set_target_properties(agsspritefont PROPERTIES
    CXX_STANDARD 11
    CXX_EXTENSIONS NO
    C_STANDARD 11
    C_EXTENSIONS NO
)

target_include_directories(agsspritefont PUBLIC AGSSpriteFont)

target_sources(agsspritefont
    PRIVATE
    AGSSpriteFont/agsplugin.h

    AGSSpriteFont/AGSSpriteFont.cpp
    AGSSpriteFont/CharacterEntry.cpp
    AGSSpriteFont/CharacterEntry.h
    AGSSpriteFont/color.cpp
    AGSSpriteFont/color.h
    AGSSpriteFont/SpriteFont.cpp
    AGSSpriteFont/SpriteFont.h
    AGSSpriteFont/SpriteFontRenderer.cpp
    AGSSpriteFont/SpriteFontRenderer.h
    AGSSpriteFont/VariableWidthFont.cpp
    AGSSpriteFont/VariableWidthFont.h
    AGSSpriteFont/VariableWidthSpriteFont.cpp
    AGSSpriteFont/VariableWidthSpriteFont.h
)