# GUI area 9, dialogs: the simulations' dialogs (OpenRocket's swing gui/simulation/: the simulation settings dialog
# and its panels, the wind editor and its CSV import, the aerodynamic lookup, the run dialog, the export panel), a
# subdirectory of the area. It comes before the area simulation (the tab), which it never includes.
# Each source with its header (${gui_include_dir}/dialogs/simulation/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    SimulationConditionsPanel.cpp ${gui_include_dir}/dialogs/simulation/SimulationConditionsPanel.h
    TurbulenceIntensityText.cpp ${gui_include_dir}/dialogs/simulation/TurbulenceIntensityText.h
    WindLevelSummary.cpp ${gui_include_dir}/dialogs/simulation/WindLevelSummary.h
)
