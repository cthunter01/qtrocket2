# The tests of the GUI area binding (gui/src/binding): XTests.cpp for the class X, lower_snake_case_tests.cpp
# for tests about no one class, in alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    BindingTestSupport.cpp
    ComponentSelectionTests.cpp
    DoubleBindingTests.cpp
    multi_edit_tests.cpp
    PropertySourceTests.cpp
    SliderMappingTests.cpp
)
