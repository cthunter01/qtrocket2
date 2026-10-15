# Script mode (cmake -P): runs QtRocket_cli once and checks its exit code and what it wrote. The
# tests of the executable (tests/core/cli/CMakeLists.txt) go through this script because ctest by
# itself tells only exit code 0 from the others (WILL_FAIL), and the tool has six.
#
#   cmake -DCLI=<QtRocket_cli> -DWORK_DIR=<directory> -DEXPECT_EXIT=<code> [-DARGS=<a|b|c>]
#         [-DSTDOUT_TO=<file>] [-DEXPECT_STDOUT=<patterns>] [-DEXPECT_STDERR=<patterns>]
#         [-DEXPECT_EMPTY_STDERR=ON] [-DEXPECT_FILE=<name> [-DEXPECT_FILE_CONTENT=<patterns>]
#         [-DEXPECT_FILE_LF_ONLY=ON]] [-DEXPECT_NO_FILE=<name>] [-DEXPECT_LF_ONLY=ON]
#         -P RunCli.cmake
#
# CLI          the executable.
# WORK_DIR     emptied and made the working directory of the run, so that a relative output path
#              lands there and no test sees what an earlier run left; the standard output and
#              error are kept there too (stdout.txt, stderr.txt), byte for byte.
# EXPECT_EXIT  the exit code, compared exactly (a crash is a text such as "Segmentation fault").
# ARGS         the arguments, separated by '|': an argument may hold spaces, none may hold '|' or
#              ';'.
# STDOUT_TO    a file the standard output goes to instead of stdout.txt, such as /dev/full (a
#              standard output that cannot be written); EXPECT_STDOUT and EXPECT_LF_ONLY do not
#              look at it then.
# EXPECT_STDOUT, EXPECT_STDERR, EXPECT_FILE_CONTENT
#              regular expressions (CMake's syntax) separated by '|', so without alternation, each
#              of which must match somewhere in the standard output, the standard error or the
#              file EXPECT_FILE. In a pattern the two characters '\n' stand for a line end, '\n'
#              with an optional '\r' before it. (CMake drops the '\r' of every "\r\n" when it
#              reads a file as text, so the patterns never see one: the line ends are what
#              EXPECT_LF_ONLY and EXPECT_FILE_LF_ONLY check.)
# EXPECT_EMPTY_STDERR  the standard error is empty.
# EXPECT_FILE  a file (relative to WORK_DIR) the run must have written.
# EXPECT_FILE_LF_ONLY  that file holds no '\r' byte.
# EXPECT_NO_FILE  a path (relative to WORK_DIR) that must not exist after the run.
# EXPECT_LF_ONLY  the standard output and error hold no '\r' byte: every line ends with '\n'
#              alone, on every platform (QtRocket_cli puts both into binary mode on Windows).

foreach(var CLI WORK_DIR EXPECT_EXIT)
    if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
        message(FATAL_ERROR "RunCli.cmake requires -D${var}=...")
    endif()
endforeach()

set(failures "")

# Expects every pattern of the '|'-separated list PATTERNS to match TEXT, which is called WHAT in
# the messages; the mismatches are added to the variable failures of the caller.
function(expect_patterns what text patterns)
    if("${patterns}" STREQUAL "")
        return()
    endif()
    string(REPLACE "|" ";" pattern_list "${patterns}")
    set(found "${failures}")
    foreach(pattern IN LISTS pattern_list)
        string(REPLACE "\\n" "\r?\n" regex "${pattern}")
        if(NOT text MATCHES "${regex}")
            string(APPEND found "  ${what} does not match '${pattern}'\n")
        endif()
    endforeach()
    set(failures "${found}" PARENT_SCOPE)
endfunction()

# Expects the file PATH, which is called WHAT in the messages, to hold no '\r' byte; a failure is
# added to the variable failures of the caller. The file is read as the hex digits of its bytes:
# read as text it would have lost the '\r' of every "\r\n". The digits are split into bytes before
# the search, so that "0d" cannot match across two bytes (a0 d1); a single regular expression over
# the digits (^(..)*0d) would overflow CMake's stack on a file of some ten kilobytes.
function(expect_lf_only what path)
    file(READ "${path}" hex HEX)
    string(REGEX MATCHALL ".." bytes "${hex}")
    list(FIND bytes "0d" carriage_return)
    if(NOT carriage_return EQUAL -1)
        set(failures
            "${failures}  ${what} holds a carriage return (byte ${carriage_return}): a line ends with \"\\r\\n\", not \"\\n\"\n"
            PARENT_SCOPE)
    endif()
endfunction()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

set(arguments "")
if(NOT "${ARGS}" STREQUAL "")
    string(REPLACE "|" ";" arguments "${ARGS}")
endif()

# The program's own bytes go to the files (OUTPUT_VARIABLE would have dropped the '\r' of every
# "\r\n" before the script saw it).
set(stdout_file "${WORK_DIR}/stdout.txt")
if(NOT "${STDOUT_TO}" STREQUAL "")
    set(stdout_file "${STDOUT_TO}")
endif()
execute_process(
    COMMAND "${CLI}" ${arguments}
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE exit_code
    OUTPUT_FILE "${stdout_file}"
    ERROR_FILE "${WORK_DIR}/stderr.txt")
set(stdout "")
if("${STDOUT_TO}" STREQUAL "")
    file(READ "${WORK_DIR}/stdout.txt" stdout)
endif()
file(READ "${WORK_DIR}/stderr.txt" stderr)

if(NOT "${exit_code}" STREQUAL "${EXPECT_EXIT}")
    string(APPEND failures "  exit code ${exit_code}, expected ${EXPECT_EXIT}\n")
endif()
expect_patterns("the standard output" "${stdout}" "${EXPECT_STDOUT}")
expect_patterns("the standard error" "${stderr}" "${EXPECT_STDERR}")
if(EXPECT_EMPTY_STDERR AND NOT "${stderr}" STREQUAL "")
    string(APPEND failures "  the standard error is not empty\n")
endif()
if(EXPECT_LF_ONLY)
    if("${STDOUT_TO}" STREQUAL "")
        expect_lf_only("the standard output" "${WORK_DIR}/stdout.txt")
    endif()
    expect_lf_only("the standard error" "${WORK_DIR}/stderr.txt")
endif()
if(NOT "${EXPECT_FILE}" STREQUAL "")
    if(EXISTS "${WORK_DIR}/${EXPECT_FILE}" AND NOT IS_DIRECTORY "${WORK_DIR}/${EXPECT_FILE}")
        file(READ "${WORK_DIR}/${EXPECT_FILE}" content)
        expect_patterns("${EXPECT_FILE}" "${content}" "${EXPECT_FILE_CONTENT}")
        if(EXPECT_FILE_LF_ONLY)
            expect_lf_only("${EXPECT_FILE}" "${WORK_DIR}/${EXPECT_FILE}")
        endif()
    else()
        string(APPEND failures "  no file ${EXPECT_FILE} was written\n")
    endif()
endif()
if(NOT "${EXPECT_NO_FILE}" STREQUAL "" AND EXISTS "${WORK_DIR}/${EXPECT_NO_FILE}")
    string(APPEND failures "  ${EXPECT_NO_FILE} exists\n")
endif()

if(NOT "${failures}" STREQUAL "")
    list(JOIN arguments "' '" shown)
    message(FATAL_ERROR "QtRocket_cli '${shown}' in ${WORK_DIR}:\n${failures}"
                        "standard output:\n${stdout}\nstandard error:\n${stderr}")
endif()
