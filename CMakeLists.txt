cmake_minimum_required(VERSION 3.24)
project(limestone VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LIMESTONE_WARNINGS -Wall -Wextra -Wpedantic)

add_library(limestone_lib
    src/csv.cpp
    src/feed.cpp
    src/interner.cpp
    src/journey.cpp
    src/loader.cpp
    src/raptor.cpp
    src/time.cpp
    src/timetable.cpp
    src/transfers.cpp
    src/version.cpp
)
target_include_directories(limestone_lib PUBLIC include)
target_compile_options(limestone_lib PRIVATE ${LIMESTONE_WARNINGS})

add_executable(limestone tools/main.cpp)
target_link_libraries(limestone PRIVATE limestone_lib)
target_compile_options(limestone PRIVATE ${LIMESTONE_WARNINGS})

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(limestone_tests
    tests/test_csv.cpp
    tests/test_feed.cpp
    tests/test_loader.cpp
    tests/test_raptor.cpp
    tests/test_time.cpp
    tests/test_transfers.cpp
    tests/test_version.cpp
)
target_link_libraries(limestone_tests PRIVATE limestone_lib GTest::gtest_main)
target_compile_options(limestone_tests PRIVATE ${LIMESTONE_WARNINGS})
target_compile_definitions(limestone_tests PRIVATE LIMESTONE_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")

include(GoogleTest)
gtest_discover_tests(limestone_tests)