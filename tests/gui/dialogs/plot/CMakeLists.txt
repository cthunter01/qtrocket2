# The tests of the plot dialog and the plot tab's panels (gui/src/dialogs/plot), a subdirectory of the GUI area
# dialogs: XTests.cpp for the class X, lower_snake_case_tests.cpp for tests about no one class, and their helpers, in
# alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    PlotDialogTests.cpp
    PlotDialogTestSupport.cpp
    SimulationPlotDialogTests.cpp
)
