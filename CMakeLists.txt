cmake_minimum_required(VERSION 3.21)
project(midip VERSION 0.0.1)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)  # Group CMake targets inside a folder
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)         # Generate compile_commands.json for language servers

include(FetchContent)
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        10.1.0
)
list(APPEND FETCH_CONTENTS fmt)
FetchContent_MakeAvailable(${FETCH_CONTENTS})

# Compiler specific options
if (NOT MSVC)
    set(BASE_OPTIONS
        "-Wall"
        "-Wextra"
        "-Wconversion"
        "-Wpedantic"
        "-Wshadow"
        "-Werror"
        # fmt warnings
        "-Wno-unknown-attributes"
    )
else()
    set(BASE_OPTIONS
        "/W4"
        "/WX"
        "/utf-8"
        "/Zc:__cplusplus"
        #"/fsanitize=address"  # Doesn't work without Visual Studio
    )
endif()

set(HEADERS "")
set(SOURCES midip.cpp)
add_executable(midip ${HEADERS} ${SOURCES})
target_include_directories(midip PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_features(midip PRIVATE cxx_std_23)
target_compile_options(midip PRIVATE ${BASE_OPTIONS})
target_link_libraries(midip PRIVATE fmt)
source_group(TREE "${CMAKE_CURRENT_LIST_DIR}" FILES ${HEADERS} ${SOURCES})
