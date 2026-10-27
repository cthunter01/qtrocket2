# GUI area 10, flightconfig: the "Motors & Configuration" tab (OpenRocket's FlightConfigurationPanel and
# gui/main/flightconfigpanel/): its tables' models, cell texts and delegate, its operations and its panels.
# Each source with its header (${gui_include_dir}/flightconfig/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    FlightConfigurablePanel.cpp ${gui_include_dir}/flightconfig/FlightConfigurablePanel.h
    FlightConfigurableTableModel.cpp ${gui_include_dir}/flightconfig/FlightConfigurableTableModel.h
    FlightConfigurationCellText.cpp ${gui_include_dir}/flightconfig/FlightConfigurationCellText.h
    FlightConfigurationEditing.cpp ${gui_include_dir}/flightconfig/FlightConfigurationEditing.h
    FlightConfigurationPanel.cpp ${gui_include_dir}/flightconfig/FlightConfigurationPanel.h
    MotorCellDelegate.cpp ${gui_include_dir}/flightconfig/MotorCellDelegate.h
    MotorConfigurationPanel.cpp ${gui_include_dir}/flightconfig/MotorConfigurationPanel.h
    MotorMountConfigurationPanel.cpp ${gui_include_dir}/flightconfig/MotorMountConfigurationPanel.h
    MotorMountTableModel.cpp ${gui_include_dir}/flightconfig/MotorMountTableModel.h
    RecoveryConfigurationPanel.cpp ${gui_include_dir}/flightconfig/RecoveryConfigurationPanel.h
    RowColumnSelectionModel.cpp ${gui_include_dir}/flightconfig/RowColumnSelectionModel.h
    SeparationConfigurationPanel.cpp ${gui_include_dir}/flightconfig/SeparationConfigurationPanel.h
    TableTransferKeys.cpp ${gui_include_dir}/flightconfig/TableTransferKeys.h
)
