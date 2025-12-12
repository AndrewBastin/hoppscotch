cmake_minimum_required(VERSION 3.15)
project(hoppscotch-c-core VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Build options
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Compiler warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wno-unused-parameter
    )
endif()

# Find required dependencies
find_package(CURL REQUIRED)

# cJSON dependency (try to find system version, fallback to FetchContent if needed)
find_package(cJSON QUIET)
if(NOT cJSON_FOUND)
    message(STATUS "cJSON not found, will use FetchContent if needed")
    # For now, we'll document manual installation
    # In the future, we can add FetchContent here
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CURL_INCLUDE_DIRS}
)

# Core library source files
set(HOPPSCOTCH_LIB_SOURCES
    src/hopp_core.c
)

# Create a static library for the core functionality
add_library(hoppscotch_core STATIC ${HOPPSCOTCH_LIB_SOURCES})

# Link libraries to core library
target_link_libraries(hoppscotch_core
    PUBLIC
    ${CURL_LIBRARIES}
)

# If cJSON is found, link it
if(cJSON_FOUND)
    target_link_libraries(hoppscotch_core PUBLIC cjson)
endif()

# Main executable
add_executable(hoppscotch-c src/main.c)

# Link executable with core library
target_link_libraries(hoppscotch-c
    PRIVATE
    hoppscotch_core
)

# Installation rules
install(TARGETS hoppscotch-c
    RUNTIME DESTINATION bin
)

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
