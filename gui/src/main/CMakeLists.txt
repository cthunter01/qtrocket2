# GUI area 9, main: the main windows, their menus, actions and undo, the file operations.
# Each source with its header (${gui_include_dir}/main/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    ComponentAddButtons.cpp ${gui_include_dir}/main/ComponentAddButtons.h
    ComponentButton.cpp ${gui_include_dir}/main/ComponentButton.h
    ComponentEditing.cpp ${gui_include_dir}/main/ComponentEditing.h
    DesignPanel.cpp ${gui_include_dir}/main/DesignPanel.h
    DocumentFileOperations.cpp ${gui_include_dir}/main/DocumentFileOperations.h
    ExampleDesigns.cpp ${gui_include_dir}/main/ExampleDesigns.h
    ExampleDesignsMenu.cpp ${gui_include_dir}/main/ExampleDesignsMenu.h
    MainMenus.cpp ${gui_include_dir}/main/MainMenus.h
    MainWindow.cpp ${gui_include_dir}/main/MainWindow.h
    OpenRocketClipboard.cpp ${gui_include_dir}/main/OpenRocketClipboard.h
    RecentFiles.cpp ${gui_include_dir}/main/RecentFiles.h
    RecentFilesMenu.cpp ${gui_include_dir}/main/RecentFilesMenu.h
    RocketActions.cpp ${gui_include_dir}/main/RocketActions.h
    SplitPane.cpp ${gui_include_dir}/main/SplitPane.h
    UndoRedoActions.cpp ${gui_include_dir}/main/UndoRedoActions.h
    WidgetUserPrompts.cpp ${gui_include_dir}/main/WidgetUserPrompts.h
    WindowManager.cpp ${gui_include_dir}/main/WindowManager.h
)
