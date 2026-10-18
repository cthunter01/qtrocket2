# Script mode (cmake -P), the test lint.no-qttest-macros (tests/lint/CMakeLists.txt): QtTest's assertion macros
# anywhere in tests/gui fail it, comments included. Outside QtTest's own runner they pass silently: QVERIFY(false),
# QCOMPARE(1, 2) and a timed-out QTRY_VERIFY report nothing to GoogleTest, so a test written with them can never fail.
# The GUI tests assert with GoogleTest (EXPECT_*, ASSERT_*) and use QtTest only for its helpers (QTest::mouseClick,
# QTest::keyClicks, QSignalSpy, QTest::qWaitFor, ...).
#
#   cmake -DSOURCE_DIR=<the repository> -P CheckNoQtTestMacros.cmake
#
# Banned: QVERIFY*, QCOMPARE*, QTRY_*, QFAIL, QSKIP, QEXPECT_FAIL, QFETCH*, QTEST* (QTEST, QTEST_MAIN, ...).

cmake_minimum_required(VERSION 3.28)
if(NOT DEFINED SOURCE_DIR OR NOT IS_DIRECTORY "${SOURCE_DIR}")
    message(FATAL_ERROR "CheckNoQtTestMacros.cmake requires -DSOURCE_DIR=<the repository>")
endif()
include(${CMAKE_CURRENT_LIST_DIR}/LintSupport.cmake)

set(banned "(^|[^A-Za-z0-9_])(QVERIFY[A-Z0-9_]*|QCOMPARE[A-Z0-9_]*|QTRY_[A-Z0-9_]+|QFAIL|QSKIP|QEXPECT_FAIL|QFETCH[A-Z0-9_]*|QTEST[A-Z0-9_]*)([^A-Za-z0-9_]|$)")

set(violations "")
qtrocket_lint_sources(files "${SOURCE_DIR}/tests/gui")
list(LENGTH files checked)
foreach(file IN LISTS files)
    file(RELATIVE_PATH relative "${SOURCE_DIR}" "${file}")
    qtrocket_lint_read_lines(lines "${file}")
    set(number 0)
    foreach(line IN LISTS lines)
        math(EXPR number "${number} + 1")
        if(line MATCHES "${banned}")
            set(found "${CMAKE_MATCH_2}")
            qtrocket_lint_unescape(text "${line}")
            string(APPEND violations "  ${relative}:${number}: ${found} in: ${text}\n")
        endif()
    endforeach()
endforeach()

if(NOT violations STREQUAL "")
    message("${violations}")
    message(FATAL_ERROR "lint.no-qttest-macros: QtTest's assertion macros in tests/gui (the list above); they pass "
                        "silently outside QtTest: use GoogleTest's EXPECT_* and ASSERT_*")
endif()
message(STATUS "lint.no-qttest-macros: ${checked} files checked, none uses QtTest's assertion macros")
