# The tests of the GUI area widgets (gui/src/widgets): XTests.cpp for the class X, lower_snake_case_tests.cpp
# for tests about no one class, in alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    BindingScopeTests.cpp
    BoolCheckBoxTests.cpp
    ConfigurationComboBoxTests.cpp
    DescriptionAreaTests.cpp
    DropdownButtonTests.cpp
    EnumComboBoxTests.cpp
    FieldEnablingTests.cpp
    FlowLayoutTests.cpp
    HtmlLabelTests.cpp
    IconButtonTests.cpp
    IconToggleButtonTests.cpp
    IntSpinBoxTests.cpp
    MaterialComboBoxTests.cpp
    number_field_tests.cpp
    RangeSliderTests.cpp
    ribbon_golden_tests.cpp
    RibbonTestSupport.cpp
    SelectionKeyFilterTests.cpp
    SingleRowScrollAreaTests.cpp
    SpinBoxEditingTests.cpp
    StageSelectorTests.cpp
    StyledLabelTests.cpp
    TextCommitterTests.cpp
    UnitSelectorTests.cpp
    UnitSpinBoxTests.cpp
    ValueSliderTests.cpp
    WidgetTestSupport.cpp
)
