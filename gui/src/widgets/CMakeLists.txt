# GUI area 4, widgets: the spin boxes, selectors and other widgets the dialogs are made of.
# Each source with its header (${gui_include_dir}/widgets/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    BindingScope.cpp ${gui_include_dir}/widgets/BindingScope.h
    BoolCheckBox.cpp ${gui_include_dir}/widgets/BoolCheckBox.h
    DescriptionArea.cpp ${gui_include_dir}/widgets/DescriptionArea.h
    EnumComboBox.cpp ${gui_include_dir}/widgets/EnumComboBox.h
    FieldEnabling.cpp ${gui_include_dir}/widgets/FieldEnabling.h
    FlowLayout.cpp ${gui_include_dir}/widgets/FlowLayout.h
    HtmlLabel.cpp ${gui_include_dir}/widgets/HtmlLabel.h
    IconButton.cpp ${gui_include_dir}/widgets/IconButton.h
    IconToggleButton.cpp ${gui_include_dir}/widgets/IconToggleButton.h
    IntSpinBox.cpp ${gui_include_dir}/widgets/IntSpinBox.h
    MaterialComboBox.cpp ${gui_include_dir}/widgets/MaterialComboBox.h
    SelectionKeyFilter.cpp ${gui_include_dir}/widgets/SelectionKeyFilter.h
    SpinBoxEditing.cpp ${gui_include_dir}/widgets/SpinBoxEditing.h
    StyledLabel.cpp ${gui_include_dir}/widgets/StyledLabel.h
    TextCommitter.cpp ${gui_include_dir}/widgets/TextCommitter.h
    UnitSelector.cpp ${gui_include_dir}/widgets/UnitSelector.h
    UnitSpinBox.cpp ${gui_include_dir}/widgets/UnitSpinBox.h
    ValueSlider.cpp ${gui_include_dir}/widgets/ValueSlider.h
)
