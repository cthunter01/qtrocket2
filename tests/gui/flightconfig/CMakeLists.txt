# The tests of the GUI area flightconfig (gui/src/flightconfig): XTests.cpp for the class X,
# lower_snake_case_tests.cpp for tests about no one class, in alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    flight_configuration_tables_golden_tests.cpp
    FlightConfigurableTableModelTests.cpp
    FlightConfigurationCellTextTests.cpp
    MotorCellDelegateTests.cpp
    MotorMountTableModelTests.cpp
)
