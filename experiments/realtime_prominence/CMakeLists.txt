cmake_minimum_required(VERSION 3.10)
project(realtime_prominence VERSION 0.1.0 LANGUAGES C)

# C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include directories
include_directories(include)

# Find syllable library from parent project
set(SYLLABLE_ROOT "${CMAKE_SOURCE_DIR}/../..")
set(SYLLABLE_BUILD "${SYLLABLE_ROOT}/build")

# Include parent's header directory
include_directories("${SYLLABLE_ROOT}/include")

# Link directories
if(WIN32)
    link_directories("${SYLLABLE_BUILD}/Release")
    link_directories("${SYLLABLE_BUILD}/Debug")
    link_directories("${CMAKE_SOURCE_DIR}/lib")
else()
    link_directories("${SYLLABLE_BUILD}")
    link_directories("${CMAKE_SOURCE_DIR}/lib")
endif()

set(SYLLABLE_LIB syllable)

# Real-time simulator
add_executable(realtime_sim src/realtime_sim.c)
target_link_libraries(realtime_sim ${SYLLABLE_LIB})

if(WIN32)
    # Copy DLL to output directory after build
    add_custom_command(TARGET realtime_sim POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SYLLABLE_BUILD}/Release/syllable.dll"
        $<TARGET_FILE_DIR:realtime_sim>
        COMMENT "Copying syllable.dll..."
    )
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(realtime_sim PRIVATE /W4)
else()
    target_compile_options(realtime_sim PRIVATE -Wall -Wextra)
endif()
