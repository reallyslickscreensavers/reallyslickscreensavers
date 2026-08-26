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

# saver directory | source stem | clamped settings | typed registry reads
#
# "clamped" counts dX = <ns>::clampToRange(val, <ns>::kX) sites. "reads" counts
# rssaver::readRegistryDWORD calls, which is the clamped count plus the
# FrameRateLimit read, plus any setting read through something other than a
# Range - today only cyclone's two normalizeFlag checkboxes.
set(SAVERS
    "cyclone|cyclone|5|8"        "euphoria|euphoria|10|11"
    "fieldlines|fieldlines|7|8"  "flocks|flocks|10|11"
    "flux|flux|12|13"            "helios|helios|8|9"
    "hyperspace|hyperspace|9|10" "lattice|lattice|11|12"
    "microcosm|microcosm|11|12"  "plasma|plasma|4|5"
    "skyrocket|skyrocket|13|14"  "solarwinds|solarWinds|9|10"
    "starfield|starfield|3|4")
set(EXPECTED_TOTAL 112)

# cyclone reached the shared clamp from the other direction, in PR #62: its
# settings header also drives the dialog, so its two checkboxes go through
# normalizeFlag rather than a {0,1} Range, and its frame rate keeps the
# dialog's "0 means unlimited" semantics instead of rsWin32Saver's plain bound.
set(CYCLONE_FRL_FORM "dFrameRateLimit *= *cycloneSettings::clampFrameRate[(]val[)];")
set(SHARED_FRL_FORM "dFrameRateLimit *= *rsWin32Saver::clampFrameRateLimit[(]val[)];")

set(FAILURES "")
set(RUNNING_TOTAL 0)

foreach(entry ${SAVERS})
    string(REPLACE "|" ";" parts "${entry}")
    list(GET parts 0 dir)
    list(GET parts 1 stem)
    list(GET parts 2 expected_count)
    list(GET parts 3 expected_reads)

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

    # --- Rule 6: exactly one whole-statement dFrameRateLimit clamp, in the
    # form this saver uses. Whole-statement, so a bare clampFrameRateLimit(val);
    # that drops the assignment cannot pass. No test can reach these lines - no
    # test may write to the developer's real HKCU key - so this is the only
    # check the frame rate limit gets anywhere.
    if(stem STREQUAL "cyclone")
        set(frl_form "${CYCLONE_FRL_FORM}")
    else()
        set(frl_form "${SHARED_FRL_FORM}")
    endif()
    set(frl_count 0)
    foreach(line ${source_lines})
        if(line MATCHES "${frl_form}")
            math(EXPR frl_count "${frl_count} + 1")
        endif()
    endforeach()
    if(NOT frl_count EQUAL 1)
        list(APPEND FAILURES "${source}: found ${frl_count} dFrameRateLimit clamp statements matching '${frl_form}', expected 1")
    endif()

    # --- Rule 7: every settings read is typed.
    #
    # rssaver::readRegistryDWORD checks REG_DWORD and the size before reporting
    # success, so a "Speed" stored as REG_SZ leaves the default in place instead
    # of arriving as four bytes of text reinterpreted as a DWORD. A bare
    # RegQueryValueEx here would silently reopen that hole.
    set(read_count 0)
    set(line_no 0)
    foreach(line ${source_lines})
        math(EXPR line_no "${line_no} + 1")
        if(line MATCHES "rssaver::readRegistryDWORD[(]skey,")
            math(EXPR read_count "${read_count} + 1")
        endif()
        if(line MATCHES "RegQueryValueEx[(]")
            list(APPEND FAILURES "${source}:${line_no}: raw RegQueryValueEx - settings reads must go through rssaver::readRegistryDWORD")
        endif()
    endforeach()
    if(NOT read_count EQUAL expected_reads)
        list(APPEND FAILURES "${source}: found ${read_count} typed registry reads, expected ${expected_reads}")
    endif()
endforeach()

if(NOT RUNNING_TOTAL EQUAL EXPECTED_TOTAL)
    list(APPEND FAILURES "total clamped settings across all savers is ${RUNNING_TOTAL}, expected ${EXPECTED_TOTAL}")
endif()

# --- Rule 8: readRegistryDWORD is defined exactly once, in the shared header.
# Thirteen private copies is the duplication the shared headers exist to avoid.
file(GLOB_RECURSE SAVER_SOURCES "${REPO_ROOT}/src/*.cpp")
foreach(f ${SAVER_SOURCES})
    file(STRINGS "${f}" dup_lines REGEX "(static|inline) +bool +readRegistryDWORD")
    if(dup_lines)
        list(APPEND FAILURES "${f}: defines its own readRegistryDWORD - use src/common/saverRegistry.h")
    endif()
endforeach()

# --- Rule 9: no file under src/ names kSingleBackground.
# microcosm is the one saver whose registry key name and global name diverge
# ("SingleBackground" into dBackground), so its constant follows the global.
# A constant named after the key would satisfy rule 2 while pairing the wrong
# slider to the wrong value.
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
message(STATUS "SettingsClampWiring: ${RUNNING_TOTAL} clamped settings across ${saver_count} savers, all typed and wired correctly")
