# The tests of the GUI area configdialog (gui/src/configdialog): XTests.cpp for the class X, lower_snake_case_tests.cpp
# for tests about no one class, in alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    BulkheadConfigTests.cpp
    CenteringRingConfigTests.cpp
    ClusterButtonTests.cpp
    ClusterSelectionPanelTests.cpp
    CommonStringsTests.cpp
    ComponentConfigDialogTests.cpp
    ConfigDialogTestSupport.cpp
    ConfigFieldsTests.cpp
    ConfigPanelFactoryTests.cpp
    InnerTubeConfigTests.cpp
    InstancesPanelTests.cpp
    MassComponentConfigTests.cpp
    MassObjectFieldsTests.cpp
    MassPanelTestSupport.cpp
    MaterialPanelTests.cpp
    MotorConfigTests.cpp
    ParachuteConfigTests.cpp
    PlacementPanelTests.cpp
    RadialPositionPanelTests.cpp
    RecoveryDeviceConfigTests.cpp
    RingComponentConfigTests.cpp
    RingPanelTestSupport.cpp
    RocketComponentConfigTests.cpp
    ShockCordConfigTests.cpp
    ShoulderPanelTests.cpp
    StreamerConfigTests.cpp
    ThicknessRingComponentConfigTests.cpp
)
