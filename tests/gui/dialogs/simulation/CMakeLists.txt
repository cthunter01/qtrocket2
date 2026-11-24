# The tests of the simulations' dialogs (gui/src/dialogs/simulation), a subdirectory of the GUI area dialogs:
# XTests.cpp for the class X, lower_snake_case_tests.cpp for tests about no one class, and their helpers, in
# alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    MultiLevelWindEditDialogTests.cpp
    MultiLevelWindTableModelTests.cpp
    MultiLevelWindTableViewTests.cpp
    SimulationConditionsPanelTests.cpp
    SimulationDialogWalk.cpp
    TurbulenceIntensityTextTests.cpp
    WindEditorTranscript.cpp
    WindLevelSummaryTests.cpp
)
