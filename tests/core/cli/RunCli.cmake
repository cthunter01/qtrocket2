# Script mode (cmake -P): runs QtRocket_cli once and checks its exit code and what it wrote. The
# tests of the executable (tests/core/cli/CMakeLists.txt) go through this script because ctest by
# itself tells only exit code 0 from the others (WILL_FAIL), and the tool has six.
#
#   cmake -DCLI=<QtRocket_cli> -DWORK_DIR=<directory> -DEXPECT_EXIT=<code> [-DARGS=<a|b|c>]
#         [-DEXPECT_STDOUT=<patterns>] [-DEXPECT_STDERR=<patterns>] [-DEXPECT_EMPTY_STDERR=ON]
#         [-DEXPECT_FILE=<name> [-DEXPECT_FILE_CONTENT=<patterns>]] [-DEXPECT_NO_FILE=<name>]
#         [-DEXPECT_LF_ONLY=ON] -P RunCli.cmake
#
# CLI          the executable.
# WORK_DIR     emptied and made the working directory of the run, so that a relative output path
#              lands there and no test sees what an earlier run left; the standard output and
#              error are kept there too (stdout.txt, stderr.txt).
# EXPECT_EXIT  the exit code, compared exactly (a crash is a text such as "Segmentation fault").
# ARGS         the arguments, separated by '|': an argument may hold spaces, none may hold '|' or
#              ';'.
# EXPECT_STDOUT, EXPECT_STDERR, EXPECT_FILE_CONTENT
#              regular expressions (CMake's syntax) separated by '|', so without alternation, each
#              of which must match somewhere in the standard output, the standard error or the
#              file EXPECT_FILE. In a pattern the two characters '\n' stand for a line end, '\n'
#              with an optional '\r' before it: the patterns tolerate the line ends of any platform.
# EXPECT_EMPTY_STDERR  the standard error is empty.
# EXPECT_FILE  a file (relative to WORK_DIR) the run must have written.
# EXPECT_NO_FILE  a path (relative to WORK_DIR) that must not exist after the run.
# EXPECT_LF_ONLY  the standard output and error hold no '\r': every line ends with '\n' alone, on
#              every platform (QtRocket_cli puts both into binary mode on Windows).

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

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

set(arguments "")
if(NOT "${ARGS}" STREQUAL "")
    string(REPLACE "|" ";" arguments "${ARGS}")
endif()

execute_process(
    COMMAND "${CLI}" ${arguments}
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE exit_code
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    ENCODING UTF-8)
file(WRITE "${WORK_DIR}/stdout.txt" "${stdout}")
file(WRITE "${WORK_DIR}/stderr.txt" "${stderr}")

if(NOT "${exit_code}" STREQUAL "${EXPECT_EXIT}")
    string(APPEND failures "  exit code ${exit_code}, expected ${EXPECT_EXIT}\n")
endif()
expect_patterns("the standard output" "${stdout}" "${EXPECT_STDOUT}")
expect_patterns("the standard error" "${stderr}" "${EXPECT_STDERR}")
if(EXPECT_EMPTY_STDERR AND NOT "${stderr}" STREQUAL "")
    string(APPEND failures "  the standard error is not empty\n")
endif()
if(EXPECT_LF_ONLY)
    string(FIND "${stdout}${stderr}" "\r" carriage_return)
    if(NOT carriage_return EQUAL -1)
        string(APPEND failures "  a line ends with \"\\r\\n\", not \"\\n\"\n")
    endif()
endif()
if(NOT "${EXPECT_FILE}" STREQUAL "")
    if(EXISTS "${WORK_DIR}/${EXPECT_FILE}" AND NOT IS_DIRECTORY "${WORK_DIR}/${EXPECT_FILE}")
        file(READ "${WORK_DIR}/${EXPECT_FILE}" content)
        expect_patterns("${EXPECT_FILE}" "${content}" "${EXPECT_FILE_CONTENT}")
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
