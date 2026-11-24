# GUI area 9, dialogs: the simulations' dialogs (OpenRocket's swing gui/simulation/: the simulation settings dialog
# and its panels, the wind editor and its CSV import, the aerodynamic lookup, the run dialog, the export panel), a
# subdirectory of the area. It comes before the area simulation (the tab), which it never includes.
# Each source with its header (${gui_include_dir}/dialogs/simulation/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    CsvImportSettingsDialog.cpp ${gui_include_dir}/dialogs/simulation/CsvImportSettingsDialog.h
    MultiLevelWindEditDialog.cpp ${gui_include_dir}/dialogs/simulation/MultiLevelWindEditDialog.h
    MultiLevelWindTableModel.cpp ${gui_include_dir}/dialogs/simulation/MultiLevelWindTableModel.h
    MultiLevelWindTableView.cpp ${gui_include_dir}/dialogs/simulation/MultiLevelWindTableView.h
    SimulationConditionsPanel.cpp ${gui_include_dir}/dialogs/simulation/SimulationConditionsPanel.h
    TurbulenceIntensityText.cpp ${gui_include_dir}/dialogs/simulation/TurbulenceIntensityText.h
    WindLevelSummary.cpp ${gui_include_dir}/dialogs/simulation/WindLevelSummary.h
)
