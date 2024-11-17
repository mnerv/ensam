cmake_minimum_required(VERSION 3.21)
project(ensam VERSION 0.0.2)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)  # Group CMake targets inside a folder
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)         # Generate compile_commands.json for language servers

option(USE_MINIAUDIO "Use Miniaudio library" OFF)

include(FetchContent)
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        10.1.0
)
list(APPEND FETCH_CONTENTS fmt)
if(USE_MINIAUDIO)
    FetchContent_Declare(
        miniaudio
        GIT_REPOSITORY https://github.com/mononerv/miniaudio.git
        GIT_TAG        9924c01d827264e2cb2c5bd5bb995f6497231e54
    )
    list(APPEND FETCH_CONTENTS miniaudio)
endif()
FetchContent_Declare(
    utf8cpp
    GIT_REPOSITORY https://github.com/nemtrif/utfcpp.git
    GIT_TAG        v4.0.5
)
list(APPEND FETCH_CONTENTS utf8cpp)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/mononerv/asio.git
    GIT_TAG        37380337ae0cc601b5e40e4e7739ab7b0a12cb63
)
list(APPEND FETCH_CONTENTS asio)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/mononerv/stb.git
    GIT_TAG        698c6fb9889c71494b49c9187d249af5fc87b211
)
list(APPEND FETCH_CONTENTS stb)
# Turn off BUILD_TESTING globally to prevent CTest from being included in CTRE
set(BUILD_TESTING OFF CACHE BOOL "Disable testing globally" FORCE)
# Set the CTRE_BUILD_TESTS option before including the CTRE library
set(CTRE_BUILD_TESTS OFF CACHE BOOL "Build ctre Tests" FORCE)
FetchContent_Declare(
    ctre
    GIT_REPOSITORY https://github.com/hanickadot/compile-time-regular-expressions.git
    GIT_TAG        v3.9.0
)
list(APPEND FETCH_CONTENTS ctre)

FetchContent_MakeAvailable(${FETCH_CONTENTS})
find_package(Threads REQUIRED)

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

set(HEADERS
    ens/midi.hpp
    ens/utils.hpp
) 
set(SOURCES
    ens/midi.cpp
)
add_library(ens OBJECT ${HEADERS} ${SOURCES})
target_include_directories(ens PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_features(ens PRIVATE cxx_std_23)
target_compile_options(ens PRIVATE ${BASE_OPTIONS})
target_compile_definitions(ens PRIVATE $<$<BOOL:${USE_MINIAUDIO}>:"MDP_MINIAUDIO">)
target_link_libraries(ens
    PUBLIC
    fmt
    asio::asio
    utf8cpp
    ctre
    stb::stb
    Threads::Threads
    $<$<BOOL:${USE_MINIAUDIO}>:miniaudio::miniaudio>
)
source_group(TREE "${CMAKE_CURRENT_LIST_DIR}" FILES ${HEADERS} ${SOURCES})

set(HEADERS "")
set(SOURCES ensam.cpp)
add_executable(ensam ${HEADERS} ${SOURCES})
target_include_directories(ensam PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_features(ensam PRIVATE cxx_std_23)
target_compile_options(ensam PRIVATE ${BASE_OPTIONS})
target_link_libraries(ensam PRIVATE ens)
source_group(TREE "${CMAKE_CURRENT_LIST_DIR}" FILES ${HEADERS} ${SOURCES})

set(HEADERS "")
set(SOURCES maestro.cpp)
add_executable(maestro ${HEADERS} ${SOURCES})
target_include_directories(maestro PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_features(maestro PRIVATE cxx_std_23)
target_compile_options(maestro PRIVATE ${BASE_OPTIONS})
target_link_libraries(maestro PRIVATE ens)
source_group(TREE "${CMAKE_CURRENT_LIST_DIR}" FILES ${HEADERS} ${SOURCES})
