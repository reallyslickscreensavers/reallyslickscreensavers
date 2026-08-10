cmake_minimum_required(VERSION 3.14)
project(rssavers_tests LANGUAGES CXX)

# GoogleTest requires C++17 (target_compile_features ... PUBLIC cxx_std_17),
# so declare it here rather than inheriting it transitively from the dependency.
set(CMAKE_CXX_STANDARD 17)
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

# Test executable for header-only settings logic that needs no saver body
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

# ---------------------------------------------------------------------------
# Saver test harness
# ---------------------------------------------------------------------------
#
# Recording OpenGL stub. _GDI32_ makes wingdi.h declare the gl* functions
# without __declspec(dllimport) so the stub's definitions can satisfy them;
# opengl32.lib and glu32.lib are deliberately never linked.
add_library(gl_stub STATIC support/gl_stub.cpp)
target_include_directories(gl_stub PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(gl_stub PUBLIC WIN32 _WINDOWS _GDI32_)

# The globals rsWin32Saver would normally own.
add_library(saver_shim STATIC support/saver_shim.cpp)
target_include_directories(saver_shim PUBLIC ${REPO_ROOT}/libs)
target_compile_definitions(saver_shim PUBLIC WIN32 _WINDOWS _GDI32_)
# winmm for rsTimer's timeGetTime/timeBeginPeriod, comctl32 for InitCommonControls.
target_link_libraries(saver_shim PUBLIC winmm comctl32)

# One executable per saver: every module defines draw(), idleProc(),
# setDefaults() and friends at global scope, so two savers in one binary would
# collide.
function(add_saver_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
    add_executable(test_${name} test_${name}.cpp ${ARG_SOURCES})
    target_include_directories(test_${name} PRIVATE
        ${REPO_ROOT}/src/${name}   # the module's own resource.h
        ${REPO_ROOT}/libs          # <rsWin32Saver/...>, <rsText/...>, <rsMath/...>
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    # WIN32 is a project define, not a compiler builtin: MSVC only predefines
    # _WIN32. Without it every saver body sits inside #ifdef WIN32 and compiles
    # away to nothing, silently reporting zero coverage.
    target_compile_definitions(test_${name} PRIVATE WIN32 _WINDOWS _GDI32_)
    target_link_libraries(test_${name} PRIVATE gl_stub saver_shim GTest::gtest_main)
    gtest_discover_tests(test_${name} DISCOVERY_MODE PRE_TEST)
endfunction()

add_saver_test(plasma SOURCES
    ${REPO_ROOT}/src/plasma/plasma.cpp
    ${REPO_ROOT}/libs/rsText/rsText.cpp
)
