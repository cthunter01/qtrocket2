# The tests of the GUI area configdialog (gui/src/configdialog): XTests.cpp for the class X, lower_snake_case_tests.cpp
# for tests about no one class, in alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    body_panel_golden_tests.cpp
    BodyPanelTestSupport.cpp
    BodyTubeConfigTests.cpp
    CommonStringsTests.cpp
    ComponentConfigDialogTests.cpp
    ConfigDialogTestSupport.cpp
    ConfigFieldsTests.cpp
    ConfigPanelFactoryTests.cpp
    EllipticalFinSetConfigTests.cpp
    fin_panel_golden_tests.cpp
    FinPointTableModelTests.cpp
    FinSetConfigTests.cpp
    FinTabCalculatorTests.cpp
    freeform_panel_golden_tests.cpp
    FreeformFinSetConfigTests.cpp
    InstancesPanelTests.cpp
    LaunchLugConfigTests.cpp
    lug_panel_golden_tests.cpp
    MaterialPanelTests.cpp
    MotorConfigTests.cpp
    NoseConeConfigTests.cpp
    PanelProbeLines.cpp
    PanelReplay.cpp
    PlacementPanelTests.cpp
    RailButtonConfigTests.cpp
    RocketComponentConfigTests.cpp
    ShoulderPanelTests.cpp
    TransitionConfigTests.cpp
    TrapezoidFinSetConfigTests.cpp
    TubeFinSetConfigTests.cpp
)
