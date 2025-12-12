cmake_minimum_required(VERSION 3.20)

project(hoppscotch_c LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(HOPPSCOTCH_C_BUILD_TESTS "Build hoppscotch-c unit tests" ON)

find_package(CURL REQUIRED)

add_library(hoppscotch_http_client
  src/http/http_client.c
)

target_include_directories(hoppscotch_http_client
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(hoppscotch_http_client
  PUBLIC
    CURL::libcurl
)

target_compile_options(hoppscotch_http_client
  PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

include(CTest)

if(BUILD_TESTING AND HOPPSCOTCH_C_BUILD_TESTS)
  include(FetchContent)

  FetchContent_Declare(
    cmocka
    GIT_REPOSITORY https://git.cryptomilk.org/projects/cmocka.git
    GIT_TAG cmocka-1.1.7
  )

  set(WITH_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(WITH_TESTS OFF CACHE BOOL "" FORCE)
  set(WITH_STATIC_LIB ON CACHE BOOL "" FORCE)
  set(WITH_SHARED_LIB OFF CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(cmocka)

  find_package(Threads REQUIRED)

  add_executable(http_client_tests
    tests/test_http_client.c
  )

  target_link_libraries(http_client_tests
    PRIVATE
      hoppscotch_http_client
      Threads::Threads
      CURL::libcurl
  )

  if(TARGET cmocka::cmocka)
    target_link_libraries(http_client_tests PRIVATE cmocka::cmocka)
  elseif(TARGET cmocka)
    target_link_libraries(http_client_tests PRIVATE cmocka)
  elseif(TARGET cmocka-static)
    target_link_libraries(http_client_tests PRIVATE cmocka-static)
  else()
    message(FATAL_ERROR "Could not determine cmocka target")
  endif()

  target_compile_options(http_client_tests
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )

  add_test(NAME hoppscotch_http_client COMMAND http_client_tests)

  if(EXISTS "/usr/bin/valgrind")
    set(MEMORYCHECK_COMMAND "/usr/bin/valgrind")
    set(MEMORYCHECK_COMMAND_OPTIONS "--leak-check=full --error-exitcode=1")
  endif()
endif()
