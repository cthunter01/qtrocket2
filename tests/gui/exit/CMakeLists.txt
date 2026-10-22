# The exit suites of Milestone 2 (plan section 9.5): a rocket designed from scratch in QtRocket's GUI
# (FromScratchExitGolden; the scenario's driver FromScratchScenario, its questions FromScratchPrompts and their tests)
# and the designs of OpenRocket opened in QtRocket's GUI (OpenDesignsExitGolden), in alphabetical order (ignoring
# case). Their suites carry "Golden" in their names, so that tests/gui/CMakeLists.txt gives them the labels gui and
# golden.
target_sources(QtRocket_gui_tests PRIVATE
    from_scratch_exit_tests.cpp
    FromScratchPrompts.cpp
    FromScratchScenario.cpp
    FromScratchScenarioTests.cpp
    open_designs_exit_tests.cpp
)
