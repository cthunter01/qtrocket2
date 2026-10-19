# The tests of the GUI area widgets (gui/src/widgets): XTests.cpp for the class X, lower_snake_case_tests.cpp
# for tests about no one class, in alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    DescriptionAreaTests.cpp
    FlowLayoutTests.cpp
    HtmlLabelTests.cpp
    IconToggleButtonTests.cpp
    IntSpinBoxTests.cpp
    number_field_tests.cpp
    SelectionKeyFilterTests.cpp
    SpinBoxEditingTests.cpp
    StyledLabelTests.cpp
    UnitSelectorTests.cpp
    UnitSpinBoxTests.cpp
    ValueSliderTests.cpp
    WidgetTestSupport.cpp
)
