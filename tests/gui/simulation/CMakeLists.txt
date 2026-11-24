# The tests of the GUI area simulation (gui/src/simulation): XTests.cpp for the class X, lower_snake_case_tests.cpp
# for tests about no one class, in alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    SimulationColumnsTests.cpp
    SimulationColumnVisibilityTests.cpp
    SimulationTableModelTests.cpp
    SimulationTablePins.cpp
    SimulationTableTestSupport.cpp
    SimulationTableViewTests.cpp
    SimulationWarningsDelegateTests.cpp
)
