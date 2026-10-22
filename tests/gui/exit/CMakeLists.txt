# The exit suites of Milestone 2 (plan section 9.5): the designs of OpenRocket opened in QtRocket's GUI
# (OpenDesignsExitGolden), in alphabetical order (ignoring case). Their suites carry "Golden" in their names, so
# that tests/gui/CMakeLists.txt gives them the labels gui and golden.
target_sources(QtRocket_gui_tests PRIVATE
    open_designs_exit_tests.cpp
)
