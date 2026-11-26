# The exit suites of Milestone 2 (plan section 9.5): a rocket designed from scratch in QtRocket's GUI
# (FromScratchExitGolden; the scenario's driver FromScratchScenario, its questions FromScratchPrompts and their tests)
# and the designs of OpenRocket opened in QtRocket's GUI (OpenDesignsExitGolden, with its list of designs OpenDesigns
# and its windows OpenDesignWindows; Milestone 4's flight block of their design views with the estimate on,
# OpenDesignsFlightBlockExitGolden); Milestone 3's scenarios of the Motors & Configuration tab
# (FlightConfigurationsExitGolden: FlightConfigurationScenario, FlightConfigurationPrompts) and Milestone 4's of the
# Flight simulations tab (SimulationsExitGolden: SimulationsScenario, SimulationsPrompts and the driver's tests), in
# alphabetical order (ignoring case). Their suites carry "Golden" in their names, so that tests/gui/CMakeLists.txt gives
# them the labels
# gui and golden (the flight block's test on the application's threads, OpenDesignsFlightBlockThreadsTest, is one of its
# threaded suites instead). The DISABLED_ writers of the interop check's saves (the suites *InteropSave, beside their
# scenarios, and FlightDataInteropSave, the flights QtRocket's GUI runs, flight_data_interop_tests.cpp; where they
# write: InteropSaves) carry no "Golden": ctest lists them disabled.
target_sources(QtRocket_gui_tests PRIVATE
    flight_configurations_exit_tests.cpp
    flight_data_interop_tests.cpp
    FlightConfigurationPrompts.cpp
    FlightConfigurationScenario.cpp
    FlightConfigurationScenarioTests.cpp
    from_scratch_exit_tests.cpp
    FromScratchPrompts.cpp
    FromScratchScenario.cpp
    FromScratchScenarioTests.cpp
    InteropSaves.cpp
    open_designs_exit_tests.cpp
    open_designs_flight_block_exit_tests.cpp
    OpenDesigns.cpp
    OpenDesignWindows.cpp
    pasted_motor_mount_tests.cpp
    ScenarioInput.cpp
    simulations_exit_tests.cpp
    SimulationsPrompts.cpp
    SimulationsScenario.cpp
    SimulationsScenarioTests.cpp
)
