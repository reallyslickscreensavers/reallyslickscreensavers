# Source-text gate on the settings clamps (Task 11 in docs/MAINTENANCE.md).
#
# This is the only check that catches a mis-paired range constant:
# dSize = flocksSettings::clampToRange(val, flocksSettings::kSpeed) compiles
# clean, and every clamp site sits below readRegistry's early return, so on a
# machine with no stored settings - CI included - none of those lines ever
# executes. Runs as a ctest case (see tests/CMakeLists.txt) so it fails the
# same pipeline steps the unit tests do.
#
# REPO_ROOT is passed in with -DREPO_ROOT=... by the add_test() invocation.

if(NOT DEFINED REPO_ROOT)
    message(FATAL_ERROR "REPO_ROOT was not passed to check-settings-wiring.cmake")
endif()

# saver directory | source stem | expected clamped settings
set(SAVERS
    "cyclone|cyclone|7"        "euphoria|euphoria|10"
    "fieldlines|fieldlines|7"  "flocks|flocks|10"
    "flux|flux|12"             "helios|helios|8"
    "hyperspace|hyperspace|9"  "lattice|lattice|11"
    "microcosm|microcosm|11"   "plasma|plasma|4"
    "skyrocket|skyrocket|13"   "solarwinds|solarWinds|9"
    "starfield|starfield|3")
set(EXPECTED_TOTAL 114)

set(FAILURES "")
set(RUNNING_TOTAL 0)

foreach(entry ${SAVERS})
    string(REPLACE "|" ";" parts "${entry}")
    list(GET parts 0 dir)
    list(GET parts 1 stem)
    list(GET parts 2 expected_count)

    set(header "${REPO_ROOT}/src/${dir}/${stem}Settings.h")
    set(source "${REPO_ROOT}/src/${dir}/${stem}.cpp")

    # --- Rule 1: the settings header exists and declares the right namespace.
    if(NOT EXISTS "${header}")
        list(APPEND FAILURES "${header}: does not exist")
        continue()
    endif()
    file(READ "${header}" header_text)
    string(FIND "${header_text}" "namespace ${stem}Settings" ns_pos)
    if(ns_pos EQUAL -1)
        list(APPEND FAILURES "${header}: missing 'namespace ${stem}Settings'")
    endif()

    if(NOT EXISTS "${source}")
        list(APPEND FAILURES "${source}: does not exist")
        continue()
    endif()
    file(STRINGS "${source}" source_lines)

    # --- Rules 2 & 3: strict clamp-site pairing, and the per-saver count.
    set(strict_count 0)
    set(line_no 0)
    foreach(line ${source_lines})
        math(EXPR line_no "${line_no} + 1")
        if(line MATCHES "d([A-Za-z]+) *= *([A-Za-z]+Settings)::clampToRange[(]val, ([A-Za-z]+Settings)::k([A-Za-z]+)[)];")
            set(setting_name "${CMAKE_MATCH_1}")
            set(ns_used "${CMAKE_MATCH_2}")
            set(ns_range "${CMAKE_MATCH_3}")
            set(range_name "${CMAKE_MATCH_4}")
            math(EXPR strict_count "${strict_count} + 1")
            if(NOT setting_name STREQUAL range_name)
                list(APPEND FAILURES "${source}:${line_no}: d${setting_name} clamped against k${range_name} - name mismatch")
            endif()
            if(NOT ns_used STREQUAL ns_range)
                list(APPEND FAILURES "${source}:${line_no}: clamp uses ${ns_used}::clampToRange but ${ns_range}::k${range_name} - namespace mismatch")
            endif()
            if(NOT ns_used STREQUAL "${stem}Settings")
                list(APPEND FAILURES "${source}:${line_no}: clamp uses ${ns_used}::clampToRange, expected ${stem}Settings::clampToRange")
            endif()
        endif()
    endforeach()

    if(NOT strict_count EQUAL expected_count)
        list(APPEND FAILURES "${source}: found ${strict_count} clamped settings, expected ${expected_count}")
    endif()
    math(EXPR RUNNING_TOTAL "${RUNNING_TOTAL} + ${strict_count}")

    # --- Rule 4: no clamp site exists that rule 2's stricter pattern rejected.
    set(loose_count 0)
    foreach(line ${source_lines})
        if(line MATCHES "= *[A-Za-z]+Settings::clampToRange[(]val,")
            math(EXPR loose_count "${loose_count} + 1")
        endif()
    endforeach()
    if(NOT loose_count EQUAL strict_count)
        list(APPEND FAILURES "${source}: ${loose_count} clampToRange(val, ...) call(s) but only ${strict_count} matched the strict name-pairing pattern")
    endif()

    # --- Rule 5: no raw registry assignment survives.
    set(line_no 0)
    foreach(line ${source_lines})
        math(EXPR line_no "${line_no} + 1")
        if(line MATCHES "^[ \t]*d[A-Za-z]+ *= *val;")
            list(APPEND FAILURES "${source}:${line_no}: raw 'dX = val;' assignment survives")
        endif()
    endforeach()

    # --- Rule 6: exactly one whole-statement dFrameRateLimit clamp.
    set(frl_count 0)
    foreach(line ${source_lines})
        if(line MATCHES "dFrameRateLimit *= *rsWin32Saver::clampFrameRateLimit[(]val[)];")
            math(EXPR frl_count "${frl_count} + 1")
        endif()
    endforeach()
    if(NOT frl_count EQUAL 1)
        list(APPEND FAILURES "${source}: found ${frl_count} 'dFrameRateLimit = rsWin32Saver::clampFrameRateLimit(val);' statements, expected 1")
    endif()
endforeach()

if(NOT RUNNING_TOTAL EQUAL EXPECTED_TOTAL)
    list(APPEND FAILURES "total clamped settings across all savers is ${RUNNING_TOTAL}, expected ${EXPECTED_TOTAL}")
endif()

# --- Rule 7: cyclone's dComplexity guard, both lines.
set(cyclone_source "${REPO_ROOT}/src/cyclone/cyclone.cpp")
file(STRINGS "${cyclone_source}" cyclone_lines)
set(guard_count 0)
set(stale_if_count 0)
foreach(line ${cyclone_lines})
    if(line MATCHES "dComplexity *= *cycloneSettings::clampIntToRange[(]dComplexity, cycloneSettings::kComplexity[)];")
        math(EXPR guard_count "${guard_count} + 1")
    endif()
    if(line MATCHES "if[(]dComplexity *[<>]")
        math(EXPR stale_if_count "${stale_if_count} + 1")
    endif()
endforeach()
if(NOT guard_count EQUAL 1)
    list(APPEND FAILURES "${cyclone_source}: found ${guard_count} unconditional dComplexity clampIntToRange guards, expected 1")
endif()
if(NOT stale_if_count EQUAL 0)
    list(APPEND FAILURES "${cyclone_source}: found ${stale_if_count} stale 'if(dComplexity <|>' lines, expected 0")
endif()

# --- Rule 8: no file under src/ names kSingleBackground.
file(GLOB_RECURSE ALL_SRC_FILES "${REPO_ROOT}/src/*.cpp" "${REPO_ROOT}/src/*.h")
foreach(f ${ALL_SRC_FILES})
    file(STRINGS "${f}" f_lines REGEX "kSingleBackground")
    if(f_lines)
        list(APPEND FAILURES "${f}: contains the identifier kSingleBackground")
    endif()
endforeach()

list(LENGTH FAILURES failure_count)
if(failure_count GREATER 0)
    string(REPLACE ";" "\n  " failure_report "${FAILURES}")
    message(FATAL_ERROR "SettingsClampWiring found ${failure_count} problem(s):\n  ${failure_report}")
endif()

list(LENGTH SAVERS saver_count)
message(STATUS "SettingsClampWiring: ${RUNNING_TOTAL} clamped settings across ${saver_count} savers, all wired correctly")
