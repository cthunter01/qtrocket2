# QtRocket_gui_tests: every test of the GUI (gui/), one GoogleTest executable with its own main()
# (GuiTestMain.cpp: one QApplication, offscreen). QtTest is linked for its helpers only (QTest::mouseClick,
# QTest::keyClicks, QSignalSpy, QTest::qWaitFor, QAbstractItemModelTester in its Warning mode); its assertion macros
# pass silently outside QtTest's own runner, so lint.no-qttest-macros rejects them here. The test support of the
# core (QtRocket_test_support: the test rockets and documents, the golden data, ...) is linked too. The tests of an
# area are in <area>/, added by <area>/CMakeLists.txt, the exit suites of Milestone 2 (plan section 9.5) in exit/;
# the test support of the GUI tests is here.
add_executable(QtRocket_gui_tests
    ExpectedQtMessage.cpp
    ExpectedQtMessageTests.cpp
    ForwardingUserPromptsTests.cpp
    gui_test_environment_tests.cpp
    GuiTestMain.cpp
    GuiTestSupport.cpp
    GuiTestSupportTests.cpp
    HookedUserPrompts.cpp
    HookedUserPromptsTests.cpp
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
add_subdirectory(plot)
add_subdirectory(tree)
add_subdirectory(figure)
add_subdirectory(configdialog)
add_subdirectory(dialogs)
add_subdirectory(flightconfig)
add_subdirectory(main)
add_subdirectory(app)
add_subdirectory(exit)
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
# The label gui is this directory's (the LABELS directory property, which ctest gives every test of the directory,
# the discovered ones included): a list given to gtest_discover_tests' PROPERTIES loses its escaped ";" on the way
# into the generated test file, so that LABELS "gui;golden" became the label gui alone and a property named golden.
set_property(DIRECTORY PROPERTY LABELS gui)
gtest_discover_tests(QtRocket_gui_tests DISCOVERY_MODE PRE_TEST DISCOVERY_TIMEOUT 60
    TEST_FILTER "-*Golden*.*:WidgetFileDialogTest.*" PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
gtest_discover_tests(QtRocket_gui_tests DISCOVERY_MODE PRE_TEST DISCOVERY_TIMEOUT 60 TEST_FILTER "*Golden*.*"
    PROPERTIES LABELS golden ENVIRONMENT QT_QPA_PLATFORM=offscreen)
# The tests that show Qt's own file dialog run one at a time. QFileDialog keeps its history and last directory in the
# native settings (QSettings(UserScope, "QtProject")), which QSettings::setPath() redirects to main()'s temporary
# directory only where the native format is an INI file: on Windows (the registry) and macOS (CFPreferences) one test
# process's dialog watches the temporary directory of another, and Windows warns when that one removes it
# ("QFileSystemWatcher: FindNextChangeNotification failed ... Access is denied", CI 40317068427).
gtest_discover_tests(QtRocket_gui_tests DISCOVERY_MODE PRE_TEST DISCOVERY_TIMEOUT 60
    TEST_FILTER "WidgetFileDialogTest.*"
    PROPERTIES RESOURCE_LOCK qt_file_dialog_settings ENVIRONMENT QT_QPA_PLATFORM=offscreen)

# The application run as a program (smoke tests, labels gui and lint), through the runner of the command-line tool's
# tests (tests/core/cli/RunCli.cmake). Nothing may reach the standard error: QT_FORCE_STDERR_LOGGING sends Qt's
# messages there on every platform (without it they go to the system's log where there is no terminal: journald on
# Linux, the debugger's output on Windows), so a Qt warning of the program fails these tests.
set(gui_program_environment QT_FORCE_STDERR_LOGGING=1)
# QtRocket --version prints its version and exits 0, before any platform is loaded: the platform named here does
# not exist, and loading it would end the program (so the options also work without a display).
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
set_tests_properties(gui.version PROPERTIES LABELS "gui;lint" TIMEOUT 300
    ENVIRONMENT "${gui_program_environment};QT_QPA_PLATFORM=no-such-platform")
# QtRocket --help names the options and the design files to open, before any platform is loaded too.
add_test(NAME gui.help
    COMMAND ${CMAKE_COMMAND}
        "-DCLI=$<TARGET_FILE:QtRocket>"
        "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/work/help"
        "-DEXPECT_EXIT=0"
        "-DARGS=--help"
        "-DEXPECT_STDOUT=\\[file\\.\\.\\.\\]|--version|Design files to open\\."
        "-DEXPECT_EMPTY_STDERR=ON"
        -P "${PROJECT_SOURCE_DIR}/tests/core/cli/RunCli.cmake")
set_tests_properties(gui.help PROPERTIES LABELS "gui;lint" TIMEOUT 300
    ENVIRONMENT "${gui_program_environment};QT_QPA_PLATFORM=no-such-platform")
# The application started and quit, through the three test hooks of the program (gui/main.cpp), offscreen: it makes
# its services, opens the window of a New design, quits as File > Quit does as soon as its event loop runs, and exits
# with 0 and nothing on the standard error (under the asan preset: no report of the sanitizers either). Its
# preferences go into the test's work directory, never into the user's, and hold what every start stores (the default
# component materials); so does the user's data directory, where the start makes the default directories of the
# user's thrust curves and component presets (user/ThrustCurves, user/Components). As GuiTestMain.cpp sets them up for the GUI's tests: Qt paints on the GUI thread only
# (QT_NO_GUI_THREADPOOL: ThreadSanitizer cannot see the locking of Qt's own painting threads in an uninstrumented Qt),
# and the offscreen platform's fonts: on Windows it reads them from QT_QPA_FONTDIR only (Qt's packages have no
# lib/fonts, of which it warns), and on macOS its font family "Sans Serif" does not exist, of which Qt's font
# database warns once (the category is off).
set(gui_start_directory ${CMAKE_CURRENT_BINARY_DIR}/work/start)
set(gui_start_environment ${gui_program_environment} QT_QPA_PLATFORM=offscreen QT_NO_GUI_THREADPOOL=1
    QTROCKET_TEST_QUIT_AFTER_START=1 QTROCKET_TEST_SETTINGS_DIRECTORY=${gui_start_directory}/settings
    QTROCKET_TEST_USER_DATA_DIRECTORY=${gui_start_directory}/user)
if(WIN32)
    set(gui_windows_directory "$ENV{WINDIR}")
    if(gui_windows_directory STREQUAL "")
        set(gui_windows_directory "C:/Windows")
    endif()
    file(TO_CMAKE_PATH "${gui_windows_directory}/Fonts" gui_font_directory)
    list(APPEND gui_start_environment "QT_QPA_FONTDIR=${gui_font_directory}")
elseif(APPLE)
    list(APPEND gui_start_environment "QT_LOGGING_RULES=qt.qpa.fonts.warning=false")
endif()
add_test(NAME gui.start
    COMMAND ${CMAKE_COMMAND}
        "-DCLI=$<TARGET_FILE:QtRocket>"
        "-DWORK_DIR=${gui_start_directory}"
        "-DEXPECT_EXIT=0"
        "-DEXPECT_EMPTY_STDERR=ON"
        "-DEXPECT_FILE=settings/QtRocket/QtRocket.ini"
        "-DEXPECT_FILE_CONTENT=\\[componentMaterials\\]|FinSet=BULK.Balsa|NoseCone=BULK.Polystyrene"
        -P "${PROJECT_SOURCE_DIR}/tests/core/cli/RunCli.cmake")
set_tests_properties(gui.start PROPERTIES LABELS "gui;lint" TIMEOUT 300 ENVIRONMENT "${gui_start_environment}")
# The test runs the program: building the test target builds it too (the coverage preset builds the test target
# alone).
add_dependencies(QtRocket_gui_tests QtRocket)
