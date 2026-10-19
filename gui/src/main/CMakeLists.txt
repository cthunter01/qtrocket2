# GUI area 9, main: the main windows, their menus, actions and undo, the file operations.
# Each source with its header (${gui_include_dir}/main/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    MainMenus.cpp ${gui_include_dir}/main/MainMenus.h
    MainWindow.cpp ${gui_include_dir}/main/MainWindow.h
    UndoRedoActions.cpp ${gui_include_dir}/main/UndoRedoActions.h
    WindowManager.cpp ${gui_include_dir}/main/WindowManager.h
)
