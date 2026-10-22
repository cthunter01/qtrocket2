# GUI area 8, dialogs: the other dialogs (errors, warnings, storage options, preferences, ...).
# Each source with its header (${gui_include_dir}/dialogs/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    ${gui_include_dir}/dialogs/ModalDialog.h
    DecalNotFoundDialog.cpp ${gui_include_dir}/dialogs/DecalNotFoundDialog.h
    DefaultUnitSelector.cpp ${gui_include_dir}/dialogs/DefaultUnitSelector.h
    DesignPreferencesPanel.cpp ${gui_include_dir}/dialogs/DesignPreferencesPanel.h
    DetailDialog.cpp ${gui_include_dir}/dialogs/DetailDialog.h
    ErrorWarningDialog.cpp ${gui_include_dir}/dialogs/ErrorWarningDialog.h
    GeneralPreferencesPanel.cpp ${gui_include_dir}/dialogs/GeneralPreferencesPanel.h
    InternalErrorDialog.cpp ${gui_include_dir}/dialogs/InternalErrorDialog.h
    MessageList.cpp ${gui_include_dir}/dialogs/MessageList.h
    OptionDialog.cpp ${gui_include_dir}/dialogs/OptionDialog.h
    PrefChoiceSelector.cpp ${gui_include_dir}/dialogs/PrefChoiceSelector.h
    PreferencesDialog.cpp ${gui_include_dir}/dialogs/PreferencesDialog.h
    PreferencesStore.cpp ${gui_include_dir}/dialogs/PreferencesStore.h
    SaveDesignInfoDialog.cpp ${gui_include_dir}/dialogs/SaveDesignInfoDialog.h
    StorageOptionsDialog.cpp ${gui_include_dir}/dialogs/StorageOptionsDialog.h
    UnitsPreferencesPanel.cpp ${gui_include_dir}/dialogs/UnitsPreferencesPanel.h
    WarningDialog.cpp ${gui_include_dir}/dialogs/WarningDialog.h
)
