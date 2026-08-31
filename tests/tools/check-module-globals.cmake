# Source-text ratchet on the savers' namespace-scope mutables (Task 6 in
# docs/MAINTENANCE.md, SonarCloud cpp:S5421).
#
# Two jobs. It pins the count each saver is down to, so a migrated saver cannot
# quietly regrow a global, and it fails a saver that still names a migrated
# variable outside the state struct - which is the only way to catch a missed
# reference inside an #ifdef RS_XSCREENSAVER block, since nothing in this
# repository compiles those.
#
# Detecting a namespace-scope variable does not need a C++ parser here, because
# these files are uniformly formatted: globals sit at column 0 and end in ";",
# function definitions at column 0 end in "{", and prototypes end in ");" with
# no "=". If a saver's formatting ever stops fitting that shape, adjust its
# entry in the table below rather than loosening the pattern for everyone.
#
# REPO_ROOT is passed in with -DREPO_ROOT=... by the add_test() invocation.

# CMake lists drop empty elements, so iterating file(STRINGS) output loses every
# blank line and each reported line number comes out short by the number of
# blanks above it. Giving each blank line a single space first keeps the
# numbering honest.
function(read_numbered_lines path out_var)
    file(READ "${path}" text)
    string(REPLACE ";" "\;" text "${text}")
    string(REPLACE "\r" "" text "${text}")
    set(previous "")
    while(NOT previous STREQUAL text)
        set(previous "${text}")
        string(REPLACE "\n\n" "\n \n" text "${text}")
    endwhile()
    string(REPLACE "\n" ";" lines "${text}")
    set(${out_var} "${lines}" PARENT_SCOPE)
endfunction()

if(NOT DEFINED REPO_ROOT)
    message(FATAL_ERROR "REPO_ROOT was not passed to check-module-globals.cmake")
endif()

# saver directory | namespace-scope mutables its .cpp files may still define
#
# A migrated saver is 1: registryPath, which rsWin32Saver.h declares extern and
# the framework reads back, so it cannot move into the state struct. It drops to
# 0 only when the rslibs half of that change lands. implicitDemo has no
# registryPath and is not a saver, so its floor is 0.
#
# Every number above 1 is a saver still waiting for Task 6. Lower them as each
# migration lands; never raise one.
set(SAVERS
    "cyclone|17"      "euphoria|29"     "fieldlines|18"
    "flocks|23"       "flux|27"         "helios|26"
    "hyperspace|56"   "lattice|26"      "microcosm|79"
    "plasma|1"        "skyrocket|50"    "solarwinds|1"
    "starfield|1"     "implicitDemo|8")

# Savers already migrated, and the variable names their state struct now owns.
# Naming one of these at column 0, or unqualified anywhere in the module, means
# a reference was missed.
set(MIGRATED "starfield" "plasma" "solarwinds")
set(starfield_MOVED
    hglrc hdc readyToDraw frameTime aspectRatio
    starX starY starZ starV textwriter
    dNumStars dSpeed dStarSize)
set(solarwinds_MOVED
    hglrc hdc aspectRatio frameTime readyToDraw
    winds lightTexture textwriter
    dWinds dEmitters dParticles dGeometry dSize
    dParticlespeed dEmitterspeed dWindspeed dBlur)
set(plasma_MOVED
    hglrc hdc readyToDraw frameTime aspectRatio
    wide high position plasmamap plasmasize textwriter
    c ct cv tex plasma
    dZoom dFocus dSpeed dResolution)

set(FAILURES "")

foreach(entry ${SAVERS})
    string(REPLACE "|" ";" parts "${entry}")
    list(GET parts 0 dir)
    list(GET parts 1 expected)

    file(GLOB sources "${REPO_ROOT}/src/${dir}/*.cpp")
    if(NOT sources)
        list(APPEND FAILURES "src/${dir}: no .cpp files found")
        continue()
    endif()

    set(found 0)
    foreach(source ${sources})
        read_numbered_lines("${source}" source_lines)
        set(line_no 0)
        foreach(line ${source_lines})
            math(EXPR line_no "${line_no} + 1")

            # Column 0, ends in a semicolon.
            if(NOT line MATCHES "^[A-Za-z_].*;$")
                continue()
            endif()
            # Declarations and definitions that are not variables of this module.
            if(line MATCHES "^(extern|typedef|using|struct|class|namespace|template|const|return|else|delete) ")
                continue()
            endif()
            # A prototype has no initialiser and ends in ");". A definition that
            # ends the same way - "int x = f();" - carries an "=".
            if(NOT line MATCHES "=" AND line MATCHES "[)];$")
                continue()
            endif()

            math(EXPR found "${found} + 1")

            # For a migrated saver, say which one it is rather than only that
            # the count moved.
            list(FIND MIGRATED "${dir}" migrated_index)
            if(NOT migrated_index EQUAL -1)
                foreach(name ${${dir}_MOVED})
                    if(line MATCHES "[^A-Za-z0-9_]${name}[^A-Za-z0-9_]" OR line MATCHES "[^A-Za-z0-9_]${name}$")
                        list(APPEND FAILURES
                             "${source}:${line_no}: defines ${name} at namespace scope again - it belongs to ${dir}State::State")
                    endif()
                endforeach()
            endif()
        endforeach()
    endforeach()

    if(found GREATER expected)
        list(APPEND FAILURES
             "src/${dir}: ${found} namespace-scope mutables, expected at most ${expected} - a new global was added")
    elseif(found LESS expected)
        list(APPEND FAILURES
             "src/${dir}: ${found} namespace-scope mutables, expected ${expected} - the count improved, lower the number in this file")
    endif()
endforeach()

# A migrated saver must reach its state through the accessor. An unqualified use
# of a moved name is a reference the migration missed; the RS_XSCREENSAVER
# blocks are the only place that can hide, because nothing compiles them.
foreach(dir ${MIGRATED})
    file(GLOB sources "${REPO_ROOT}/src/${dir}/*.cpp")
    foreach(source ${sources})
        read_numbered_lines("${source}" source_lines)
        set(line_no 0)
        foreach(line ${source_lines})
            math(EXPR line_no "${line_no} + 1")
            if(line MATCHES "^[ \t]*//")
                continue()
            endif()
            foreach(name ${${dir}_MOVED})
                # Matched only when not already reached through "s." or
                # "state()." and not part of a longer identifier.
                if(line MATCHES "(^|[^A-Za-z0-9_.])${name}([^A-Za-z0-9_]|$)")
                    list(APPEND FAILURES
                         "${source}:${line_no}: ${name} used without the state accessor")
                endif()
            endforeach()
        endforeach()
    endforeach()
endforeach()

list(LENGTH FAILURES failure_count)
if(failure_count GREATER 0)
    string(REPLACE ";" "\n  " failure_report "${FAILURES}")
    message(FATAL_ERROR "ModuleGlobalsEncapsulated found ${failure_count} problem(s):\n  ${failure_report}")
endif()

list(LENGTH SAVERS saver_count)
message(STATUS "ModuleGlobalsEncapsulated: ${saver_count} modules at or below their pinned namespace-scope counts")
