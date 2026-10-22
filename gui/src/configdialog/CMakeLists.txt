# GUI area 7, configdialog: the component configuration dialog and its panels.
# Each source with its header (${gui_include_dir}/configdialog/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    BodyComponentFields.cpp ${gui_include_dir}/configdialog/BodyComponentFields.h
    BodyTubeConfig.cpp ${gui_include_dir}/configdialog/BodyTubeConfig.h
    CommonStrings.cpp ${gui_include_dir}/configdialog/CommonStrings.h
    ComponentConfigDialog.cpp ${gui_include_dir}/configdialog/ComponentConfigDialog.h
    ConfigFields.cpp ${gui_include_dir}/configdialog/ConfigFields.h
    ConfigPanelFactory.cpp ${gui_include_dir}/configdialog/ConfigPanelFactory.h
    EllipticalFinSetConfig.cpp ${gui_include_dir}/configdialog/EllipticalFinSetConfig.h
    FinSetConfig.cpp ${gui_include_dir}/configdialog/FinSetConfig.h
    FinTabCalculator.cpp ${gui_include_dir}/configdialog/FinTabCalculator.h
    InstancesPanel.cpp ${gui_include_dir}/configdialog/InstancesPanel.h
    LaunchLugConfig.cpp ${gui_include_dir}/configdialog/LaunchLugConfig.h
    MaterialPanel.cpp ${gui_include_dir}/configdialog/MaterialPanel.h
    MotorConfig.cpp ${gui_include_dir}/configdialog/MotorConfig.h
    NoseConeConfig.cpp ${gui_include_dir}/configdialog/NoseConeConfig.h
    PlacementPanel.cpp ${gui_include_dir}/configdialog/PlacementPanel.h
    RailButtonConfig.cpp ${gui_include_dir}/configdialog/RailButtonConfig.h
    RocketComponentConfig.cpp ${gui_include_dir}/configdialog/RocketComponentConfig.h
    ShoulderPanel.cpp ${gui_include_dir}/configdialog/ShoulderPanel.h
    TransitionConfig.cpp ${gui_include_dir}/configdialog/TransitionConfig.h
    TrapezoidFinSetConfig.cpp ${gui_include_dir}/configdialog/TrapezoidFinSetConfig.h
    TubeFinSetConfig.cpp ${gui_include_dir}/configdialog/TubeFinSetConfig.h
)
