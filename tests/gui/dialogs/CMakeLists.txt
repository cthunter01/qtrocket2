# The tests of the GUI area dialogs (gui/src/dialogs): XTests.cpp for the class X, lower_snake_case_tests.cpp
# for tests about no one class, in alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    DetailDialogTests.cpp
    InternalErrorDialogTests.cpp
    SaveDesignInfoDialogTests.cpp
)
