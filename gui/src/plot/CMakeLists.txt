# GUI area 5, plot: the plots (rocketplot's PlotWidget and what QtRocket configures on it: colours, gestures,
# export, the thrust curves). The only area that includes rocketplot's headers (<rocketplot/X.h>, lint.layers); the
# areas after it use rocketplot through its classes.
# Each source with its header (${gui_include_dir}/plot/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    PlotColors.cpp ${gui_include_dir}/plot/PlotColors.h
    PlotExport.cpp ${gui_include_dir}/plot/PlotExport.h
    PlotInputBindings.cpp ${gui_include_dir}/plot/PlotInputBindings.h
)
