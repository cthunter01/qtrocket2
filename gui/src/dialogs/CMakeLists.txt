# GUI area 8, dialogs: the other dialogs (errors, warnings, storage options, preferences, ...).
# Each source with its header (${gui_include_dir}/dialogs/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    DetailDialog.cpp ${gui_include_dir}/dialogs/DetailDialog.h
    InternalErrorDialog.cpp ${gui_include_dir}/dialogs/InternalErrorDialog.h
    SaveDesignInfoDialog.cpp ${gui_include_dir}/dialogs/SaveDesignInfoDialog.h
)
