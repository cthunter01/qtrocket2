# QtRocket_gui_tests: every test of the GUI (gui/), one GoogleTest executable with its own main()
# (GuiTestMain.cpp: one QApplication, offscreen). QtTest is linked for its helpers only (QTest::mouseClick,
# QTest::keyClicks, QSignalSpy, QTest::qWaitFor, QAbstractItemModelTester in its Warning mode); its assertion macros
# pass silently outside QtTest's own runner, so lint.no-qttest-macros rejects them here. The test support of the
# core (QtRocket_test_support: the test rockets and documents, the golden data, ...) is linked too. The tests of an
# area are in <area>/, added by <area>/CMakeLists.txt; the test support of the GUI tests is here.
add_executable(QtRocket_gui_tests
    ExpectedQtMessage.cpp
    ExpectedQtMessageTests.cpp
    gui_test_environment_tests.cpp
    GuiTestMain.cpp
    GuiTestSupport.cpp
    GuiTestSupportTests.cpp
    NextModalDialog.cpp
    RecordingGuardHook.cpp
    ScopedApplicationPalette.cpp
    ScriptedUserPrompts.cpp
    ScriptedUserPromptsTests.cpp
    TestGuiServices.cpp
)
add_subdirectory(util)
add_subdirectory(settings)
add_subdirectory(binding)
add_subdirectory(widgets)
add_subdirectory(tree)
add_subdirectory(figure)
add_subdirectory(configdialog)
add_subdirectory(dialogs)
add_subdirectory(main)
add_subdirectory(app)
target_include_directories(QtRocket_gui_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# GTest::gtest, not gtest_main: main() is GuiTestMain.cpp. GTest::gmock for GoogleMock's matchers.
target_link_libraries(QtRocket_gui_tests PRIVATE QtRocket::gui QtRocket_test_support Qt6::Test GTest::gtest
    GTest::gmock)
add_dependencies(QtRocket_gui_tests QtRocket_data)
QtRocket_configure_qt_target(QtRocket_gui_tests)
# The area lists above add the tests: AUTOMOC must notice when one of them changes.
QtRocket_automoc_follows_sources(QtRocket_gui_tests)

include(GoogleTest)
# As tests/core: listed when ctest runs (PRE_TEST), a generous listing timeout, and the label golden on the suites
# whose name contains "Golden" (the comparisons with OpenRocket's reference). Every test also gets the label gui and
# the offscreen platform (main() picks it as well when the variable is unset: the listing run gets no ENVIRONMENT).
gtest_discover_tests(QtRocket_gui_tests DISCOVERY_MODE PRE_TEST DISCOVERY_TIMEOUT 60 TEST_FILTER "-*Golden*.*"
    PROPERTIES LABELS gui ENVIRONMENT QT_QPA_PLATFORM=offscreen)
gtest_discover_tests(QtRocket_gui_tests DISCOVERY_MODE PRE_TEST DISCOVERY_TIMEOUT 60 TEST_FILTER "*Golden*.*"
    PROPERTIES LABELS "gui;golden" ENVIRONMENT QT_QPA_PLATFORM=offscreen)

# The application run as a program (a smoke test, labels gui and lint): QtRocket --version prints its version and
# exits 0, on the offscreen platform (the QApplication comes before the command line is read). The runner is the
# one of the command-line tool's tests (tests/core/cli/RunCli.cmake).
string(REPLACE "." "\\." gui_version_pattern "${PROJECT_VERSION}")
add_test(NAME gui.version
    COMMAND ${CMAKE_COMMAND}
        "-DCLI=$<TARGET_FILE:QtRocket>"
        "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/work/version"
        "-DEXPECT_EXIT=0"
        "-DARGS=--version"
        "-DEXPECT_STDOUT=^QtRocket ${gui_version_pattern}\\n$"
        "-DEXPECT_EMPTY_STDERR=ON"
        -P "${PROJECT_SOURCE_DIR}/tests/core/cli/RunCli.cmake")
set_tests_properties(gui.version PROPERTIES LABELS "gui;lint" ENVIRONMENT QT_QPA_PLATFORM=offscreen TIMEOUT 300)
# QtRocket --help names the options and the design files to open.
add_test(NAME gui.help
    COMMAND ${CMAKE_COMMAND}
        "-DCLI=$<TARGET_FILE:QtRocket>"
        "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/work/help"
        "-DEXPECT_EXIT=0"
        "-DARGS=--help"
        "-DEXPECT_STDOUT=\\[file\\.\\.\\.\\]|--version|Design files to open\\."
        "-DEXPECT_EMPTY_STDERR=ON"
        -P "${PROJECT_SOURCE_DIR}/tests/core/cli/RunCli.cmake")
set_tests_properties(gui.help PROPERTIES LABELS "gui;lint" ENVIRONMENT QT_QPA_PLATFORM=offscreen TIMEOUT 300)
# The application started and quit, through the two test hooks of the program (gui/main.cpp), offscreen: it makes
# its services, opens the window of a New design, quits as File > Quit does as soon as its event loop runs, and exits
# with 0 and nothing on the standard error (under the asan preset: no report of the sanitizers either). Its
# preferences go into the test's work directory, never into the user's, and hold what every start stores (the default
# component materials).
set(gui_start_directory ${CMAKE_CURRENT_BINARY_DIR}/work/start)
add_test(NAME gui.start
    COMMAND ${CMAKE_COMMAND}
        "-DCLI=$<TARGET_FILE:QtRocket>"
        "-DWORK_DIR=${gui_start_directory}"
        "-DEXPECT_EXIT=0"
        "-DEXPECT_EMPTY_STDERR=ON"
        "-DEXPECT_FILE=settings/QtRocket/QtRocket.ini"
        "-DEXPECT_FILE_CONTENT=\\[componentMaterials\\]|FinSet=BULK.Balsa|NoseCone=BULK.Polystyrene"
        -P "${PROJECT_SOURCE_DIR}/tests/core/cli/RunCli.cmake")
set_tests_properties(gui.start PROPERTIES LABELS "gui;lint" TIMEOUT 300
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTROCKET_TEST_QUIT_AFTER_START=1;QTROCKET_TEST_SETTINGS_DIRECTORY=${gui_start_directory}/settings")
# The test runs the program: building the test target builds it too (the coverage preset builds the test target
# alone).
add_dependencies(QtRocket_gui_tests QtRocket)
