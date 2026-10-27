# The tests of the parts library (gui/src/dialogs/preset): XTests.cpp for the class X, lower_snake_case_tests.cpp
# for tests about no one class, in alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    ComponentPresetChooserDialogTests.cpp
    ComponentPresetFilterModelTests.cpp
    ComponentPresetTableModelTests.cpp
    ComponentPresetTableTests.cpp
    preset_chooser_golden_tests.cpp
)
