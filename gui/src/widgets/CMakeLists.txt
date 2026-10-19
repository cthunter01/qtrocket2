# GUI area 4, widgets: the spin boxes, selectors and other widgets the dialogs are made of.
# Each source with its header (${gui_include_dir}/widgets/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    ConfigurationComboBox.cpp ${gui_include_dir}/widgets/ConfigurationComboBox.h
    DescriptionArea.cpp ${gui_include_dir}/widgets/DescriptionArea.h
    FlowLayout.cpp ${gui_include_dir}/widgets/FlowLayout.h
    HtmlLabel.cpp ${gui_include_dir}/widgets/HtmlLabel.h
    IconToggleButton.cpp ${gui_include_dir}/widgets/IconToggleButton.h
    SingleRowScrollArea.cpp ${gui_include_dir}/widgets/SingleRowScrollArea.h
    StageSelector.cpp ${gui_include_dir}/widgets/StageSelector.h
    StyledLabel.cpp ${gui_include_dir}/widgets/StyledLabel.h
)
