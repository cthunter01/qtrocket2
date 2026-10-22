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
    MaterialPanelTests.cpp
    MotorConfigTests.cpp
    PlacementPanelTests.cpp
    RingComponentConfigTests.cpp
    RingPanelTestSupport.cpp
    RocketComponentConfigTests.cpp
    ShoulderPanelTests.cpp
    ThicknessRingComponentConfigTests.cpp
)
