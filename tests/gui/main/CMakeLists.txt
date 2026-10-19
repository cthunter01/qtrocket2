# The tests of the GUI area main (gui/src/main): XTests.cpp for the class X, lower_snake_case_tests.cpp
# for tests about no one class, in alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    MainMenusTests.cpp
    MainWindowTests.cpp
    UndoRedoActionsTests.cpp
    WidgetUserPromptsTests.cpp
    window_shortcuts_tests.cpp
    WindowManagerTests.cpp
)
