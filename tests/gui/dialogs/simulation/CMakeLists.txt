# The tests of the simulations' dialogs (gui/src/dialogs/simulation), a subdirectory of the GUI area dialogs:
# XTests.cpp for the class X, lower_snake_case_tests.cpp for tests about no one class, and their helpers, in
# alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    AerodynamicLookupDialogTests.cpp
    CsvImportSettingsDialogTests.cpp
    MultiLevelWindEditDialogTests.cpp
    MultiLevelWindTableModelTests.cpp
    MultiLevelWindTableViewTests.cpp
    RandomSeedControlsTests.cpp
    RecoveryWarningsPanelTests.cpp
    SimulationConditionsPanelTests.cpp
    SimulationDialogWalk.cpp
    SimulationExtensionListTests.cpp
    SimulationOptionsPanelTests.cpp
    SimulationWarningsPanelTests.cpp
    TurbulenceIntensityTextTests.cpp
    WindEditorTranscript.cpp
    wind_levels_import_tests.cpp
    WindLevelSummaryTests.cpp
)
