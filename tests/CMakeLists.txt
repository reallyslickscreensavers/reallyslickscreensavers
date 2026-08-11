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

# OpenAL stub, linked only by skyrocket. AL_BUILD_LIBRARY is the exact analogue
# of _GDI32_ above: without it al.h declares every entry point dllimport and the
# stub's definitions satisfy nothing. OpenAL32.lib is never linked.
add_library(al_stub STATIC support/al_stub.cpp)
target_include_directories(al_stub PUBLIC ${REPO_ROOT}/3rdparty/openal/include)
target_compile_definitions(al_stub PUBLIC AL_BUILD_LIBRARY)

# The globals rsWin32Saver would normally own.
add_library(saver_shim STATIC support/saver_shim.cpp support/test_window.cpp)
target_include_directories(saver_shim PUBLIC ${REPO_ROOT}/libs)
target_compile_definitions(saver_shim PUBLIC WIN32 _WINDOWS _GDI32_)
# winmm for rsTimer's timeGetTime/timeBeginPeriod, comctl32 for InitCommonControls.
target_link_libraries(saver_shim PUBLIC winmm comctl32)

# One executable per saver: every module defines draw(), idleProc(),
# setDefaults() and friends at global scope, so two savers in one binary would
# collide.
function(add_saver_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBRARIES" ${ARGN})
    add_executable(test_${name} test_${name}.cpp ${ARG_SOURCES})
    target_include_directories(test_${name} PRIVATE
        ${REPO_ROOT}/src/${name}   # the module's own resource.h
        ${REPO_ROOT}/libs          # <rsWin32Saver/...>, <rsText/...>, <rsMath/...>
        ${REPO_ROOT}/3rdparty      # <GL/glext.h>, as the .vcxproj files do
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    # WIN32 is a project define, not a compiler builtin: MSVC only predefines
    # _WIN32. Without it every saver body sits inside #ifdef WIN32 and compiles
    # away to nothing, silently reporting zero coverage.
    target_compile_definitions(test_${name} PRIVATE WIN32 _WINDOWS _GDI32_)
    target_link_libraries(test_${name} PRIVATE
        gl_stub saver_shim GTest::gtest_main ${ARG_LIBRARIES})
    gtest_discover_tests(test_${name} DISCOVERY_MODE PRE_TEST)
endfunction()

# The stub's own maths. Everything the saver suites assert about flare
# positions and culling is computed by the matrix stack, so it is checked
# directly rather than only through the savers that consume it.
add_executable(test_gl_stub test_gl_stub.cpp)
target_compile_definitions(test_gl_stub PRIVATE WIN32 _WINDOWS _GDI32_)
target_link_libraries(test_gl_stub PRIVATE gl_stub GTest::gtest_main)
gtest_discover_tests(test_gl_stub DISCOVERY_MODE PRE_TEST)

add_saver_test(plasma SOURCES
    ${REPO_ROOT}/src/plasma/plasma.cpp
    ${REPO_ROOT}/libs/rsText/rsText.cpp
)

add_saver_test(flocks SOURCES
    ${REPO_ROOT}/src/flocks/flocks.cpp
    ${REPO_ROOT}/libs/rsText/rsText.cpp
    ${REPO_ROOT}/libs/Rgbhsl/Rgbhsl.cpp   # bug::update colours bugs through hsl2rgb
)

add_saver_test(cyclone SOURCES
    ${REPO_ROOT}/src/cyclone/cyclone.cpp
    ${REPO_ROOT}/libs/rsText/rsText.cpp
    ${REPO_ROOT}/libs/Rgbhsl/Rgbhsl.cpp
)

add_saver_test(fieldlines SOURCES
    ${REPO_ROOT}/src/fieldlines/fieldlines.cpp
    ${REPO_ROOT}/libs/rsText/rsText.cpp
)

# The source file is solarWinds.cpp but the module directory is solarwinds, so
# the test target name follows the directory.
add_saver_test(solarwinds SOURCES
    ${REPO_ROOT}/src/solarwinds/solarWinds.cpp
    ${REPO_ROOT}/libs/rsText/rsText.cpp
)

add_saver_test(starfield SOURCES
    ${REPO_ROOT}/src/starfield/starfield.cpp
    ${REPO_ROOT}/libs/rsText/rsText.cpp
)

add_saver_test(flux SOURCES
    ${REPO_ROOT}/src/flux/flux.cpp
    ${REPO_ROOT}/libs/rsText/rsText.cpp
    ${REPO_ROOT}/libs/Rgbhsl/Rgbhsl.cpp   # particle::update colours through hsl2rgb
)

add_saver_test(euphoria SOURCES
    ${REPO_ROOT}/src/euphoria/euphoria.cpp
    ${REPO_ROOT}/libs/rsText/rsText.cpp
    ${REPO_ROOT}/libs/rsMath/rsVec.cpp    # wisp::update works in rsVec
    ${REPO_ROOT}/libs/Rgbhsl/Rgbhsl.cpp   # and colours through hsl2rgb
)

# Sources shared by the savers that build implicit surfaces - helios today,
# microcosm later. Listed here rather than in each add_saver_test call because
# the set is the whole Implicit library plus the rsMath types it works in.
set(IMPLICIT_SOURCES
    ${REPO_ROOT}/libs/Implicit/impCubeTables.cpp
    ${REPO_ROOT}/libs/Implicit/impCubeVolume.cpp
    ${REPO_ROOT}/libs/Implicit/impShape.cpp
    ${REPO_ROOT}/libs/Implicit/impSphere.cpp
    ${REPO_ROOT}/libs/Implicit/impSurface.cpp
)

# helios builds a marching-cubes shell around its emitters and steers its
# camera with quaternions.
add_saver_test(helios SOURCES
    ${REPO_ROOT}/src/helios/helios.cpp
    ${REPO_ROOT}/libs/rsText/rsText.cpp
    ${REPO_ROOT}/libs/rsMath/rsVec.cpp
    ${REPO_ROOT}/libs/rsMath/rsMatrix.cpp
    ${REPO_ROOT}/libs/rsMath/rsQuat.cpp
    ${REPO_ROOT}/libs/Rgbhsl/Rgbhsl.cpp
    ${IMPLICIT_SOURCES}
)
