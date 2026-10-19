# GUI area 4, widgets: the spin boxes, selectors and other widgets the dialogs are made of.
# Each source with its header (${gui_include_dir}/widgets/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    DescriptionArea.cpp ${gui_include_dir}/widgets/DescriptionArea.h
    FlowLayout.cpp ${gui_include_dir}/widgets/FlowLayout.h
    HtmlLabel.cpp ${gui_include_dir}/widgets/HtmlLabel.h
    IconButton.cpp ${gui_include_dir}/widgets/IconButton.h
    IconToggleButton.cpp ${gui_include_dir}/widgets/IconToggleButton.h
    StyledLabel.cpp ${gui_include_dir}/widgets/StyledLabel.h
)
