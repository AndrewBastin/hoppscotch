# Test suite for Hoppscotch C Core

# Test sources
set(TEST_SOURCES
    test_basic.c
)

# Create test executable
add_executable(test_hoppscotch ${TEST_SOURCES})

# Link against the core library
target_link_libraries(test_hoppscotch
    PRIVATE
    hoppscotch_core
)

# Add include directories
target_include_directories(test_hoppscotch
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Disable treating warnings as errors for tests since asserts use variables
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_hoppscotch PRIVATE -Wno-error)
endif()

# Add test
add_test(NAME test_hoppscotch COMMAND test_hoppscotch)
