# GUI area 6, tree: the component tree (its model, view, selection, drag and drop).
# Each source with its header (${gui_include_dir}/tree/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    ComponentDrop.cpp ${gui_include_dir}/tree/ComponentDrop.h
    ComponentLimits.cpp ${gui_include_dir}/tree/ComponentLimits.h
    ComponentMimeData.cpp ${gui_include_dir}/tree/ComponentMimeData.h
    ComponentToolTip.cpp ${gui_include_dir}/tree/ComponentToolTip.h
    ComponentTreeDelegate.cpp ${gui_include_dir}/tree/ComponentTreeDelegate.h
    ComponentTreeModel.cpp ${gui_include_dir}/tree/ComponentTreeModel.h
    ComponentTreeView.cpp ${gui_include_dir}/tree/ComponentTreeView.h
    DocumentSelectionModel.cpp ${gui_include_dir}/tree/DocumentSelectionModel.h
)
