# QtRocket_cli run as a program: one ctest test per case, each through RunCli.cmake (cmake -P), which
# checks the exact exit code (ctest alone tells only 0 from the others), patterns of the standard
# output and error, and the files written. The tests in this process (CliTests.cpp,
# cli_golden_tests.cpp) cover the commands in full; these cover what only the executable does: its
# arguments, its data directory (the build tree's copy of the motor database, which
# QtRocket_cli_data makes, found beside the program as in an installed archive), its exit status and
# its standard streams. Every exit code is pinned but 5, an internal error, which no input gives
# (CliTests.cpp provokes it in process). Label "cli"; the milestone run also "golden".

# The tests run the program: building the test target builds it too (the coverage preset builds the
# test target alone).
add_dependencies(QtRocket_core_tests QtRocket_cli)

set(cli_example "${PROJECT_SOURCE_DIR}/data/examples/A simple model rocket.ork")
set(cli_fixtures "${PROJECT_SOURCE_DIR}/tests/data/cli")
string(REPLACE "." "\\." cli_version_pattern "${PROJECT_VERSION}")

# qtrocket_cli_test(<case> EXIT <code> [ARGS <argument>...] [STDOUT <pattern>...]
#                   [STDERR <pattern>...] [EMPTY_STDERR] [LF_ONLY] [FILE <name> [CONTENT <pattern>...]]
#                   [NO_FILE <name>] [LABELS <label>...])
# Adds the test cli.<case>, which runs QtRocket_cli with the arguments in a work directory of its own
# and expects what RunCli.cmake describes.
function(qtrocket_cli_test name)
    cmake_parse_arguments(PARSE_ARGV 1 cli "EMPTY_STDERR;LF_ONLY" "EXIT;FILE;NO_FILE"
        "ARGS;STDOUT;STDERR;CONTENT;LABELS")
    list(JOIN cli_ARGS "|" arguments)
    list(JOIN cli_STDOUT "|" stdout_patterns)
    list(JOIN cli_STDERR "|" stderr_patterns)
    list(JOIN cli_CONTENT "|" content_patterns)
    add_test(NAME cli.${name}
        COMMAND ${CMAKE_COMMAND}
            "-DCLI=$<TARGET_FILE:QtRocket_cli>"
            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/work/${name}"
            "-DEXPECT_EXIT=${cli_EXIT}"
            "-DARGS=${arguments}"
            "-DEXPECT_STDOUT=${stdout_patterns}"
            "-DEXPECT_STDERR=${stderr_patterns}"
            "-DEXPECT_EMPTY_STDERR=${cli_EMPTY_STDERR}"
            "-DEXPECT_LF_ONLY=${cli_LF_ONLY}"
            "-DEXPECT_FILE=${cli_FILE}"
            "-DEXPECT_FILE_CONTENT=${content_patterns}"
            "-DEXPECT_NO_FILE=${cli_NO_FILE}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/RunCli.cmake")
    set(labels cli ${cli_LABELS})
    set_tests_properties(cli.${name} PROPERTIES LABELS "${labels}" TIMEOUT 300)
endfunction()

# 0: the version and the help, every line ending with '\n' alone (decision E7: on Windows too).
qtrocket_cli_test(version EXIT 0 ARGS --version
    STDOUT "^QtRocket ${cli_version_pattern}\\n$" EMPTY_STDERR LF_ONLY)
qtrocket_cli_test(help EXIT 0 ARGS --help
    STDOUT "^Usage: QtRocket_cli <command> \\[options\\]\\n" "\\nExit codes:\\n" "  5  internal error\\n$"
    EMPTY_STDERR LF_ONLY)

# 1: the command line.
qtrocket_cli_test(no_arguments EXIT 1
    STDERR "^QtRocket_cli: no command given\\nRun 'QtRocket_cli --help' for the usage\\.\\n$")
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

# 0: the exit check of Milestone 1 (plan section 7) as the plan writes it, with a relative CSV path;
# cli_golden_tests.cpp compares the apogee with its golden (50.482784669989435 m) within 1e-4.
qtrocket_cli_test(milestone EXIT 0
    ARGS simulate "${cli_example}" --calm --time-step 0.01 --csv out.csv
    STDOUT "^Simulation 1: Simulation 1\\n" "\\n  apogee: 50\\.48[0-9]* m\\n" "\\n  CSV: out\\.csv\\n"
    EMPTY_STDERR
    FILE out.csv CONTENT "^# Simulation 1 \\(Up to date\\)\\n" "\\n# Time \\(s\\),Altitude \\(m\\),"
        "\\n# Event APOGEE occurred at t="
    LABELS golden)

# 3: a simulation aborted (an ordinary abort: no motors), its CSV written all the same.
qtrocket_cli_test(aborted_simulation EXIT 3 ARGS simulate "${cli_fixtures}/no-motors.ork" --csv aborted.csv
    STDOUT " s: SIM_ABORT: No motors defined in the simulation\\n"
    STDERR "QtRocket_cli: simulation 1 'Without a motor' was aborted: No motors defined in the simulation\\n"
    FILE aborted.csv CONTENT "\\n# Event SIM_ABORT occurred at t=0 seconds\\n")

# 4: a CSV that cannot be written, and a motor database that cannot be read.
qtrocket_cli_test(csv_into_missing_directory EXIT 4
    ARGS simulate "${cli_example}" --csv missing/out.csv
    STDERR "^QtRocket_cli: cannot open '.*out\\.csv' for writing\\n" NO_FILE missing)
qtrocket_cli_test(unreadable_motor_database EXIT 4 ARGS motors --db "${cli_fixtures}/no-motors.ork"
    STDERR "^QtRocket_cli: cannot read the motor database '")
