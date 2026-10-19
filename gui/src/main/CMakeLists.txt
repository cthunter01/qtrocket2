# GUI area 9, main: the main windows, their menus, actions and undo, the file operations.
# Each source with its header (${gui_include_dir}/main/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    ComponentEditing.cpp ${gui_include_dir}/main/ComponentEditing.h
    MainMenus.cpp ${gui_include_dir}/main/MainMenus.h
    MainWindow.cpp ${gui_include_dir}/main/MainWindow.h
    OpenRocketClipboard.cpp ${gui_include_dir}/main/OpenRocketClipboard.h
    UndoRedoActions.cpp ${gui_include_dir}/main/UndoRedoActions.h
    WidgetUserPrompts.cpp ${gui_include_dir}/main/WidgetUserPrompts.h
    WindowManager.cpp ${gui_include_dir}/main/WindowManager.h
)
