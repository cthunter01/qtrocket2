# The tests of the GUI area main (gui/src/main): XTests.cpp for the class X, lower_snake_case_tests.cpp
# for tests about no one class, in alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    ComponentAddButtonsTests.cpp
    ComponentEditingTests.cpp
    ConfigDialogRequestsTests.cpp
    DesignPanelTests.cpp
    DocumentFileOperationsTests.cpp
    ExampleDesignsMenuTests.cpp
    ExampleDesignsTests.cpp
    FileMenusTestSupport.cpp
    MainMenusTests.cpp
    MainWindowTests.cpp
    OpenRocketClipboardTests.cpp
    preferences_window_tests.cpp
    RecentFilesMenuTests.cpp
    RecentFilesTests.cpp
    rocket_panel_window_tests.cpp
    RocketActionsTests.cpp
    SaveFileNamesTests.cpp
    SplitPaneTests.cpp
    UndoRedoActionsTests.cpp
    WidgetUserPromptsTests.cpp
    window_shortcuts_tests.cpp
    WindowManagerTests.cpp
)
