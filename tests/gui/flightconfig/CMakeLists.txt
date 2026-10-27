# The tests of the GUI area flightconfig (gui/src/flightconfig): XTests.cpp for the class X,
# lower_snake_case_tests.cpp for tests about no one class, in alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    flight_configuration_operations_golden_tests.cpp
    flight_configuration_tables_golden_tests.cpp
    FlightConfigurablePanelTests.cpp
    FlightConfigurableTableModelTests.cpp
    FlightConfigurationCellTextTests.cpp
    FlightConfigurationEditingTests.cpp
    FlightConfigurationPanelTests.cpp
    FlightConfigurationTabSupport.cpp
    MotorCellDelegateTests.cpp
    MotorConfigurationPanelTests.cpp
    MotorMountConfigurationPanelTests.cpp
    MotorMountTableModelTests.cpp
    RecoveryConfigurationPanelTests.cpp
    RowColumnSelectionModelTests.cpp
    SeparationConfigurationPanelTests.cpp
    TableTransferKeysTests.cpp
)
