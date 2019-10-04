# The name of the target operating system.
set(CMAKE_SYSTEM_NAME Generic)

execute_process(COMMAND xcrun --sdk ${ALLEGRO_IOS_SDK} --show-sdk-path
                OUTPUT_VARIABLE SDK_PATH
                OUTPUT_STRIP_TRAILING_WHITESPACE)
set(IOS_ADDITIONAL_LIBRARY_PATH "$(pwd)/../../nativelibs/${ALLEGRO_IOS_ARCH}")
set(IOS_PLATFORM_INCLUDE "${SDK_PATH}/usr/include")
set(IOS_PLATFORM_LIB "${SDK_PATH}/usr/lib")

# Location of target environment.

set(CMAKE_SYSTEM_INCLUDE_PATH "${IOS_ADDITIONAL_LIBRARY_PATH}/include" "${IOS_PLATFORM_INCLUDE}")
set(CMAKE_SYSTEM_LIBRARY_PATH "${IOS_ADDITIONAL_LIBRARY_PATH}/lib" "${IOS_PLATFORM_LIB}")

# Which compilers to use for C and C++.
execute_process(COMMAND xcrun --sdk ${ALLEGRO_IOS_SDK} --find gcc
                OUTPUT_VARIABLE CMAKE_C_COMPILER
                OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND xcrun --sdk ${ALLEGRO_IOS_SDK} --find g++
                OUTPUT_VARIABLE CMAKE_CXX_COMPILER
                OUTPUT_STRIP_TRAILING_WHITESPACE)

# Needed to pass the compiler tests.
set(LINK_DIRECTORIES ${NDK_PLATFORM_LIB})
set(LINK_LIBRARIES "")

set(CMAKE_EXE_LINKER_FLAGS "")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")

# Adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search
# programs in the host environment.
set(CMAKE_FIND_ROOT_PATH "")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(IOS 1)
