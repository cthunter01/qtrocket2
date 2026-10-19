# The tests of the GUI area widgets (gui/src/widgets): XTests.cpp for the class X, lower_snake_case_tests.cpp
# for tests about no one class, in alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    ConfigurationComboBoxTests.cpp
    DescriptionAreaTests.cpp
    FlowLayoutTests.cpp
    HtmlLabelTests.cpp
    IconToggleButtonTests.cpp
    ribbon_golden_tests.cpp
    RibbonTestSupport.cpp
    SingleRowScrollAreaTests.cpp
    StageSelectorTests.cpp
    StyledLabelTests.cpp
)
