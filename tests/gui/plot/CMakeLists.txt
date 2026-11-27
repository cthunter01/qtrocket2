# The tests of the GUI area plot (gui/src/plot): XTests.cpp for the class X, lower_snake_case_tests.cpp
# for tests about no one class, in alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    DataPlotTests.cpp
    PlotColorsTests.cpp
    PlotEventGraphicsTests.cpp
    PlotExportTests.cpp
    PlotInputBindingsTests.cpp
    PlotReadback.cpp
    PlotReadbackTests.cpp
    PlotThemeTests.cpp
    rocketplot_build_tests.cpp
    simulation_plot_chart_golden_tests.cpp
    SimulationPlotGoldenChecks.cpp
    SimulationPlotTests.cpp
    thrust_curve_plot_golden_tests.cpp
    ThrustCurvePlotTests.cpp
    ThrustCurvePreviewTests.cpp
    WindProfilePlotTests.cpp
)
