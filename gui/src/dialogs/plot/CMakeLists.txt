# GUI area 9, dialogs: the plot dialog and the plot tab's panels (OpenRocket's swing gui/plot/), a subdirectory of the
# area. The plots themselves (rocketplot's widgets) are the area plot/'s.
# Each source with its header (${gui_include_dir}/dialogs/plot/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    FlightEventTableModel.cpp ${gui_include_dir}/dialogs/plot/FlightEventTableModel.h
    PlotDialog.cpp ${gui_include_dir}/dialogs/plot/PlotDialog.h
    PlotPanel.cpp ${gui_include_dir}/dialogs/plot/PlotPanel.h
    PlotTypeSelector.cpp ${gui_include_dir}/dialogs/plot/PlotTypeSelector.h
    SimulationPlotDialog.cpp ${gui_include_dir}/dialogs/plot/SimulationPlotDialog.h
    SimulationPlotPanel.cpp ${gui_include_dir}/dialogs/plot/SimulationPlotPanel.h
    SimulationPlotTypeSelector.cpp ${gui_include_dir}/dialogs/plot/SimulationPlotTypeSelector.h
)
