# The tests of the motor chooser (gui/src/dialogs/motor), a subdirectory of the GUI area dialogs: XTests.cpp for the
# class X, lower_snake_case_tests.cpp for tests about no one class, and their helpers, in alphabetical order
# (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    CheckListTests.cpp
    motor_chooser_mounts_golden_tests.cpp
    motor_information_golden_tests.cpp
    motor_table_golden_tests.cpp
    MotorChooserDialogTests.cpp
    MotorChooserTestSupport.cpp
    MotorFilterPanelTests.cpp
    MotorInformationPanelTests.cpp
    MotorTableModelTests.cpp
    ThrustCurveMotorSelectionPanelTests.cpp
)
