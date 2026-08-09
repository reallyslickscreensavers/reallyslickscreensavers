cmake_minimum_required(VERSION 3.14)
project(rssavers_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.17.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Repo root, so saver headers can be reached by directory
set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

# Test executable
add_executable(rssavers_tests
    test_starfieldSettings.cpp
)

target_include_directories(rssavers_tests PRIVATE
    ${REPO_ROOT}/src/starfield    # for "starfieldSettings.h"
)

target_link_libraries(rssavers_tests PRIVATE GTest::gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(rssavers_tests DISCOVERY_MODE PRE_TEST)
