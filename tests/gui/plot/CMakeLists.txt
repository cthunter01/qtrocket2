# The tests of the GUI area plot (gui/src/plot): XTests.cpp for the class X, lower_snake_case_tests.cpp
# for tests about no one class, in alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    PlotColorsTests.cpp
    PlotExportTests.cpp
    PlotInputBindingsTests.cpp
    PlotReadback.cpp
    PlotReadbackTests.cpp
    rocketplot_build_tests.cpp
    thrust_curve_plot_golden_tests.cpp
    ThrustCurvePlotTests.cpp
    ThrustCurvePreviewTests.cpp
)
