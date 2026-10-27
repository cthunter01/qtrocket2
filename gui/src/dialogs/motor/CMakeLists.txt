# GUI area 9, dialogs: the motor chooser (OpenRocket's gui/dialogs/motor/ and its thrustcurve/ package), a
# subdirectory of the area. Each source with its header (${gui_include_dir}/dialogs/motor/X.h): AUTOMOC reads only
# the headers listed here.
target_sources(QtRocket_gui PRIVATE
    CheckList.cpp ${gui_include_dir}/dialogs/motor/CheckList.h
    MotorFilterPanel.cpp ${gui_include_dir}/dialogs/motor/MotorFilterPanel.h
    MotorInformationPanel.cpp ${gui_include_dir}/dialogs/motor/MotorInformationPanel.h
    MotorTableModel.cpp ${gui_include_dir}/dialogs/motor/MotorTableModel.h
    ThrustCurveMotorSelectionPanel.cpp ${gui_include_dir}/dialogs/motor/ThrustCurveMotorSelectionPanel.h
)
