# GUI area 5, plot: the plots (rocketplot's PlotWidget and what QtRocket configures on it: colours, theme,
# gestures, export, the thrust curves, the simulation plots). The only area that includes rocketplot's headers
# (<rocketplot/X.h>, lint.layers); the areas after it use rocketplot through its classes.
# Each source with its header (${gui_include_dir}/plot/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    DataPlot.cpp ${gui_include_dir}/plot/DataPlot.h
    PlotColors.cpp ${gui_include_dir}/plot/PlotColors.h
    PlotEventGraphics.cpp ${gui_include_dir}/plot/PlotEventGraphics.h
    PlotExport.cpp ${gui_include_dir}/plot/PlotExport.h
    PlotHomeRange.cpp ${gui_include_dir}/plot/PlotHomeRange.h
    PlotInputBindings.cpp ${gui_include_dir}/plot/PlotInputBindings.h
    PlotText.cpp ${gui_include_dir}/plot/PlotText.h
    PlotTheme.cpp ${gui_include_dir}/plot/PlotTheme.h
    SimulationPlot.cpp ${gui_include_dir}/plot/SimulationPlot.h
    ThrustCurvePlot.cpp ${gui_include_dir}/plot/ThrustCurvePlot.h
    ThrustCurvePreview.cpp ${gui_include_dir}/plot/ThrustCurvePreview.h
)
