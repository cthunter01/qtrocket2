# The tests of the simulations' dialogs (gui/src/dialogs/simulation), a subdirectory of the GUI area dialogs:
# XTests.cpp for the class X, lower_snake_case_tests.cpp for tests about no one class, and their helpers, in
# alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    AerodynamicLookupDialogTests.cpp
    CsvImportSettingsDialogTests.cpp
    CsvOptionsDialogTests.cpp
    MultiLevelWindEditDialogTests.cpp
    MultiLevelWindTableModelTests.cpp
    MultiLevelWindTableViewTests.cpp
    RandomSeedControlsTests.cpp
    RecoveryWarningsPanelTests.cpp
    simulation_dialog_pin_checks_tests.cpp
    simulation_export_golden_tests.cpp
    SimulationConditionsPanelTests.cpp
    SimulationConfigDialogTests.cpp
    SimulationDialogPinChecks.cpp
    SimulationDialogPins.cpp
    SimulationDialogWalk.cpp
    SimulationExportPanelTests.cpp
    SimulationExtensionListTests.cpp
    SimulationOptionsPanelTests.cpp
    SimulationRunDialogTests.cpp
    SimulationWarningsPanelTests.cpp
    TurbulenceIntensityTextTests.cpp
    wind_levels_import_tests.cpp
    WindEditorTranscript.cpp
    WindLevelSummaryTests.cpp
)
