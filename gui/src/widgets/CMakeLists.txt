# GUI area 4, widgets: the spin boxes, selectors and other widgets the dialogs are made of.
# Each source with its header (${gui_include_dir}/widgets/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    DescriptionArea.cpp ${gui_include_dir}/widgets/DescriptionArea.h
    FlowLayout.cpp ${gui_include_dir}/widgets/FlowLayout.h
    HtmlLabel.cpp ${gui_include_dir}/widgets/HtmlLabel.h
    IconToggleButton.cpp ${gui_include_dir}/widgets/IconToggleButton.h
    IntSpinBox.cpp ${gui_include_dir}/widgets/IntSpinBox.h
    SelectionKeyFilter.cpp ${gui_include_dir}/widgets/SelectionKeyFilter.h
    SpinBoxEditing.cpp ${gui_include_dir}/widgets/SpinBoxEditing.h
    StyledLabel.cpp ${gui_include_dir}/widgets/StyledLabel.h
    UnitSelector.cpp ${gui_include_dir}/widgets/UnitSelector.h
    UnitSpinBox.cpp ${gui_include_dir}/widgets/UnitSpinBox.h
    ValueSlider.cpp ${gui_include_dir}/widgets/ValueSlider.h
)
