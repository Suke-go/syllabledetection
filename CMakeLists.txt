cmake_minimum_required(VERSION 3.10)
project(libsyllable VERSION 0.1.0 LANGUAGES C)

# Options
option(BUILD_SHARED_LIBS "Build shared library" ON)
option(BUILD_EXAMPLES "Build example applications" ON)
option(BUILD_TESTS "Build unit tests" ON)

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/syllable_detector.c
    src/dsp/biquad.c
    src/dsp/envelope.c
    src/dsp/zff.c
    src/peak_detector.c
)

# Library target
add_library(syllable ${SOURCES})
target_include_directories(syllable PUBLIC include)

# Compiler warnings
if(MSVC)
    target_compile_options(syllable PRIVATE /W4)
    target_compile_definitions(syllable PRIVATE EXPORT_DLL)
else()
    target_compile_options(syllable PRIVATE -Wall -Wextra -pedantic)
endif()

# Installation
install(TARGETS syllable DESTINATION lib)
install(FILES include/syllable_detector.h DESTINATION include)

# Subdirectories
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
