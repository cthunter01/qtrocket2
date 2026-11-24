# GUI area 4, widgets: the spin boxes, selectors and other widgets the dialogs are made of.
# Each source with its header (${gui_include_dir}/widgets/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    BindingScope.cpp ${gui_include_dir}/widgets/BindingScope.h
    BoolCheckBox.cpp ${gui_include_dir}/widgets/BoolCheckBox.h
    CollapsiblePanel.cpp ${gui_include_dir}/widgets/CollapsiblePanel.h
    ColorChooserButton.cpp ${gui_include_dir}/widgets/ColorChooserButton.h
    ConfigurationComboBox.cpp ${gui_include_dir}/widgets/ConfigurationComboBox.h
    CsvExportPanel.cpp ${gui_include_dir}/widgets/CsvExportPanel.h
    CsvOptionPanel.cpp ${gui_include_dir}/widgets/CsvOptionPanel.h
    DataTypeComboBox.cpp ${gui_include_dir}/widgets/DataTypeComboBox.h
    DescriptionArea.cpp ${gui_include_dir}/widgets/DescriptionArea.h
    DropdownButton.cpp ${gui_include_dir}/widgets/DropdownButton.h
    EnumComboBox.cpp ${gui_include_dir}/widgets/EnumComboBox.h
    FieldEnabling.cpp ${gui_include_dir}/widgets/FieldEnabling.h
    FieldSeparatorComboBox.cpp ${gui_include_dir}/widgets/FieldSeparatorComboBox.h
    FieldValidator.cpp ${gui_include_dir}/widgets/FieldValidator.h
    FlowLayout.cpp ${gui_include_dir}/widgets/FlowLayout.h
    GroupedSearchPopup.cpp ${gui_include_dir}/widgets/GroupedSearchPopup.h
    HtmlLabel.cpp ${gui_include_dir}/widgets/HtmlLabel.h
    IconButton.cpp ${gui_include_dir}/widgets/IconButton.h
    IconToggleButton.cpp ${gui_include_dir}/widgets/IconToggleButton.h
    IntSpinBox.cpp ${gui_include_dir}/widgets/IntSpinBox.h
    LineStyleComboBox.cpp ${gui_include_dir}/widgets/LineStyleComboBox.h
    MaterialComboBox.cpp ${gui_include_dir}/widgets/MaterialComboBox.h
    PresetComboBox.cpp ${gui_include_dir}/widgets/PresetComboBox.h
    RangeSlider.cpp ${gui_include_dir}/widgets/RangeSlider.h
    SelectionKeyFilter.cpp ${gui_include_dir}/widgets/SelectionKeyFilter.h
    SingleRowScrollArea.cpp ${gui_include_dir}/widgets/SingleRowScrollArea.h
    SpinBoxEditing.cpp ${gui_include_dir}/widgets/SpinBoxEditing.h
    StageSelector.cpp ${gui_include_dir}/widgets/StageSelector.h
    StyledLabel.cpp ${gui_include_dir}/widgets/StyledLabel.h
    TableRowTraversal.cpp ${gui_include_dir}/widgets/TableRowTraversal.h
    TextCommitter.cpp ${gui_include_dir}/widgets/TextCommitter.h
    UnitSelector.cpp ${gui_include_dir}/widgets/UnitSelector.h
    UnitSpinBox.cpp ${gui_include_dir}/widgets/UnitSpinBox.h
    ValueSlider.cpp ${gui_include_dir}/widgets/ValueSlider.h
)
