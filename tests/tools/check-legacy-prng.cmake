# Reject active uses of the C rand/srand generator in parent-project sources.
# Randomness is provided by rsMath's self-seeded, thread-local std::mt19937.

if(NOT DEFINED REPO_ROOT)
    message(FATAL_ERROR "REPO_ROOT was not passed to check-legacy-prng.cmake")
endif()

file(GLOB_RECURSE SOURCE_FILES
    "${REPO_ROOT}/src/*.cpp"
    "${REPO_ROOT}/src/*.h")

set(FAILURES "")
set(CHECKED_FILES 0)

foreach(source_file ${SOURCE_FILES})
    math(EXPR CHECKED_FILES "${CHECKED_FILES} + 1")
    file(STRINGS "${source_file}" source_lines)
    set(line_number 0)

    foreach(source_line ${source_lines})
        math(EXPR line_number "${line_number} + 1")
        string(STRIP "${source_line}" stripped_line)

        # Ignore ordinary line comments and block-comment lines. This keeps the
        # historical commented-out srand(0) example in mirrorBox.cpp harmless.
        if(stripped_line MATCHES "^(//|/[*]|[*])")
            continue()
        endif()

        # Drop an inline // comment, then reject rand( or srand( as a complete
        # identifier anywhere in the remaining code. This also catches
        # std::rand and assignments such as `const int value = rand();`.
        string(REGEX REPLACE "//.*$" "" code_line "${source_line}")
        if(code_line MATCHES "(^|[^A-Za-z0-9_])(srand|rand)[ \t]*[(]")
            list(APPEND FAILURES "${source_file}:${line_number}: ${stripped_line}")
        endif()
    endforeach()
endforeach()

list(LENGTH FAILURES failure_count)
if(failure_count GREATER 0)
    string(REPLACE ";" "\n  " failure_report "${FAILURES}")
    message(FATAL_ERROR
        "Found ${failure_count} active C PRNG call(s); use rsRandi/rsRandf from rsMath.h:\n  ${failure_report}")
endif()

message(STATUS "NoLegacyCPrngCalls: checked ${CHECKED_FILES} source files")
