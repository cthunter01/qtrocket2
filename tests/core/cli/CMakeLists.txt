# QtRocket_cli run as a program: one ctest test per case, each through RunCli.cmake (cmake -P), which
# checks the exact exit code (ctest alone tells only 0 from the others), patterns of the standard
# output and error, the bytes of their line ends, and the files written. The tests in this process
# (CliTests.cpp, cli_golden_tests.cpp) cover the commands in full; these cover what only the
# executable does: its arguments (UTF-8 on Windows too), its data directory (the build tree's copy of
# the motor database and of the component presets, which QtRocket_data of the top-level
# CMakeLists.txt makes, found beside the program as in an installed archive), its exit status and
# its standard streams (binary mode on Windows, a standard output that cannot be written). Every
# exit code is pinned but 5, an internal error, which no input gives (CliTests.cpp provokes it in
# process). Label "cli"; the milestone run also "golden".

# The tests run the program: building the test target builds it too (the coverage preset builds the
# test target alone).
add_dependencies(QtRocket_core_tests QtRocket_cli)

set(cli_example "${PROJECT_SOURCE_DIR}/data/examples/A simple model rocket.ork")
set(cli_fixtures "${PROJECT_SOURCE_DIR}/tests/data/cli")
string(REPLACE "." "\\." cli_version_pattern "${PROJECT_VERSION}")

# qtrocket_cli_test(<case> EXIT <code> [ARGS <argument>...] [STDOUT_TO <file>] [STDOUT <pattern>...]
#                   [STDERR <pattern>...] [EMPTY_STDERR] [LF_ONLY]
#                   [FILE <name> [CONTENT <pattern>...] [FILE_LF_ONLY]] [NO_FILE <name>]
#                   [PROGRAM <path>] [FAILS_WITH <regex>] [LABELS <label>...])
# Adds the test cli.<case>, which runs QtRocket_cli (or PROGRAM) with the arguments in a work
# directory of its own and expects what RunCli.cmake describes. With FAILS_WITH the test passes
# when RunCli.cmake reports a failure that matches the regular expression: a test of the runner.
function(qtrocket_cli_test name)
    cmake_parse_arguments(PARSE_ARGV 1 cli "EMPTY_STDERR;LF_ONLY;FILE_LF_ONLY"
        "EXIT;STDOUT_TO;FILE;NO_FILE;PROGRAM;FAILS_WITH" "ARGS;STDOUT;STDERR;CONTENT;LABELS")
    list(JOIN cli_ARGS "|" arguments)
    list(JOIN cli_STDOUT "|" stdout_patterns)
    list(JOIN cli_STDERR "|" stderr_patterns)
    list(JOIN cli_CONTENT "|" content_patterns)
    set(program "$<TARGET_FILE:QtRocket_cli>")
    if(DEFINED cli_PROGRAM)
        set(program "${cli_PROGRAM}")
    endif()
    add_test(NAME cli.${name}
        COMMAND ${CMAKE_COMMAND}
            "-DCLI=${program}"
            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/work/${name}"
            "-DEXPECT_EXIT=${cli_EXIT}"
            "-DARGS=${arguments}"
            "-DSTDOUT_TO=${cli_STDOUT_TO}"
            "-DEXPECT_STDOUT=${stdout_patterns}"
            "-DEXPECT_STDERR=${stderr_patterns}"
            "-DEXPECT_EMPTY_STDERR=${cli_EMPTY_STDERR}"
            "-DEXPECT_LF_ONLY=${cli_LF_ONLY}"
            "-DEXPECT_FILE=${cli_FILE}"
            "-DEXPECT_FILE_CONTENT=${content_patterns}"
            "-DEXPECT_FILE_LF_ONLY=${cli_FILE_LF_ONLY}"
            "-DEXPECT_NO_FILE=${cli_NO_FILE}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/RunCli.cmake")
    set(labels cli ${cli_LABELS})
    set_tests_properties(cli.${name} PROPERTIES LABELS "${labels}" TIMEOUT 300)
    if(DEFINED cli_FAILS_WITH)
        set_tests_properties(cli.${name} PROPERTIES PASS_REGULAR_EXPRESSION "${cli_FAILS_WITH}")
    endif()
endfunction()

# The runner sees a carriage return: in the standard output (cmake -E cat of a fixture whose lines
# end with "\r\n", tests/data/cli/crlf-lines.txt) and in a file written (cmake -E copy of it). CMake
# drops the '\r' of "\r\n" when it reads text, so a check of the text could never fail; these two
# keep LF_ONLY and FILE_LF_ONLY from becoming such a check again.
set(cli_crlf_lines "${cli_fixtures}/crlf-lines.txt")
qtrocket_cli_test(runner_sees_a_carriage_return_in_the_output EXIT 0
    PROGRAM "${CMAKE_COMMAND}" ARGS -E cat "${cli_crlf_lines}" LF_ONLY
    FAILS_WITH "the standard output holds a carriage return")
qtrocket_cli_test(runner_sees_a_carriage_return_in_a_file EXIT 0
    PROGRAM "${CMAKE_COMMAND}" ARGS -E copy "${cli_crlf_lines}" copy.txt FILE copy.txt FILE_LF_ONLY
    FAILS_WITH "copy\\.txt holds a carriage return")

# 0: the version and the help, every line ending with '\n' alone (decision E7: on Windows too).
qtrocket_cli_test(version EXIT 0 ARGS --version
    STDOUT "^QtRocket ${cli_version_pattern}\\n$" EMPTY_STDERR LF_ONLY)
qtrocket_cli_test(help EXIT 0 ARGS --help
    STDOUT "^Usage: QtRocket_cli <command> \\[options\\]\\n" "\\nExit codes:\\n" "  5  internal error\\n$"
    EMPTY_STDERR LF_ONLY)

# 1: the command line.
qtrocket_cli_test(no_arguments EXIT 1
    STDERR "^QtRocket_cli: no command given\\nRun 'QtRocket_cli --help' for the usage\\.\\n$" LF_ONLY)
qtrocket_cli_test(unknown_option EXIT 1 ARGS info "${cli_example}" --calm
    STDERR "^QtRocket_cli: unknown option '--calm' for 'info'\\n")
qtrocket_cli_test(zero_time_step EXIT 1 ARGS simulate "${cli_example}" --time-step 0
    STDERR "^QtRocket_cli: the time step must be a number from 0\\.0001 to 10 s, not '0'\\n")
qtrocket_cli_test(unknown_simulation EXIT 1 ARGS simulate "${cli_example}" --sim 6
    STDERR "has no simulation '6'")

# 2: a design that cannot be loaded.
qtrocket_cli_test(missing_design EXIT 2 ARGS info "${cli_fixtures}/no such design.ork"
    STDERR "^QtRocket_cli: cannot load '.*no such design\\.ork'")
qtrocket_cli_test(malformed_design EXIT 2 ARGS info "${cli_fixtures}/malformed.ork"
    STDERR "^QtRocket_cli: cannot load '.*malformed\\.ork': ")

# 0: 'info', with the motor database of the data directory.
qtrocket_cli_test(info EXIT 0 ARGS info "${cli_example}"
    STDOUT "\\nRocket: A simple model rocket\\n" "\\nFlight configurations: 6\\n"
        "\\nConfiguration 0: \\[No motors\\] \\(default\\)\\n" "\\n  CG: 0\\.2055[0-9]* m\\n"
        "\\n  CP at Mach 0\\.3, angle of attack 0: 0\\.3305[0-9]* m\\n"
    EMPTY_STDERR LF_ONLY)

# 0: 'info' of a design that names three component presets: the program reads the bundled presets
# of its data directory (components/, the build tree's copy of data/components), which hold the
# three, so the standard error stays empty (without them each would give OpenRocket's warning "No
# matching ComponentPreset ...").
qtrocket_cli_test(info_with_presets EXIT 0
    ARGS info "${PROJECT_SOURCE_DIR}/data/examples/Pods--powered with recovery deployment.ork"
    STDOUT "\\nRocket: Pods With Recovery\\n" "\\nFlight configurations: 2\\n"
    EMPTY_STDERR LF_ONLY)

# 0: the exit check of Milestone 1 (plan section 7) as the plan writes it, with a relative CSV path;
# cli_golden_tests.cpp compares the apogee with its golden (50.482784669989435 m) within 1e-4.
qtrocket_cli_test(milestone EXIT 0
    ARGS simulate "${cli_example}" --calm --time-step 0.01 --csv out.csv
    STDOUT "^Simulation 1: Simulation 1\\n" "\\n  apogee: 50\\.48[0-9]* m\\n" "\\n  CSV: out\\.csv\\n"
    EMPTY_STDERR LF_ONLY
    FILE out.csv CONTENT "^# Simulation 1 \\(Up to date\\)\\n" "\\n# Time \\(s\\),Altitude \\(m\\),"
        "\\n# Event APOGEE occurred at t="
    FILE_LF_ONLY
    LABELS golden)

# 3: a simulation aborted (an ordinary abort: no motors), its CSV written all the same.
qtrocket_cli_test(aborted_simulation EXIT 3 ARGS simulate "${cli_fixtures}/no-motors.ork" --csv aborted.csv
    STDOUT " s: SIM_ABORT: No motors defined in the simulation\\n"
    STDERR "QtRocket_cli: simulation 1 'Without a motor' was aborted: No motors defined in the simulation\\n"
    LF_ONLY
    FILE aborted.csv CONTENT "\\n# Event SIM_ABORT occurred at t=0 seconds\\n" FILE_LF_ONLY)

# 3: an argument beyond ASCII reaches the program as UTF-8 (on Windows through GetCommandLineW()),
# and the file it names is written under that name.
qtrocket_cli_test(non_ascii_csv EXIT 3 ARGS simulate "${cli_fixtures}/no-motors.ork" --csv "Flug-ü.csv"
    STDOUT "\\n  CSV: Flug-ü\\.csv\\n"
    FILE "Flug-ü.csv" CONTENT "^# Without a motor\\n")

# 4: a CSV that cannot be written, found before any simulation runs (nothing on the standard
# output), and a motor database that cannot be read.
qtrocket_cli_test(csv_into_missing_directory EXIT 4
    ARGS simulate "${cli_example}" --csv missing/out.csv
    STDOUT "^$"
    STDERR "^QtRocket_cli: cannot write 'missing/out\\.csv': the directory 'missing' does not exist\\n$"
    NO_FILE missing)
qtrocket_cli_test(unreadable_motor_database EXIT 4 ARGS motors --db "${cli_fixtures}/no-motors.ork"
    STDERR "^QtRocket_cli: cannot read the motor database '")

# 4: a standard output that cannot be written (a full disk), where the system has a device for it.
if(EXISTS /dev/full)
    qtrocket_cli_test(standard_output_full EXIT 4 ARGS --version STDOUT_TO /dev/full
        STDERR "^QtRocket_cli: cannot write the standard output\\n$")
endif()
