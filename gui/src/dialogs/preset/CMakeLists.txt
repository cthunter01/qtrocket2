# GUI area 9, dialogs: the parts library (OpenRocket's swing gui/dialogs/preset), a subdirectory of the area.
# Each source with its header (${gui_include_dir}/dialogs/preset/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    ComponentPresetChooserDialog.cpp ${gui_include_dir}/dialogs/preset/ComponentPresetChooserDialog.h
    ComponentPresetFilterModel.cpp ${gui_include_dir}/dialogs/preset/ComponentPresetFilterModel.h
    ComponentPresetTable.cpp ${gui_include_dir}/dialogs/preset/ComponentPresetTable.h
    ComponentPresetTableModel.cpp ${gui_include_dir}/dialogs/preset/ComponentPresetTableModel.h
)
