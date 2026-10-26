# GUI area 2, settings: the preferences stored with QSettings and the GUI's own preference keys.
# Each source with its header (${gui_include_dir}/settings/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    ${gui_include_dir}/settings/GuiPreferenceKeys.h
    SettingsPreferences.cpp ${gui_include_dir}/settings/SettingsPreferences.h
    TableUiPreferences.cpp ${gui_include_dir}/settings/TableUiPreferences.h
    WindowGeometry.cpp ${gui_include_dir}/settings/WindowGeometry.h
)
