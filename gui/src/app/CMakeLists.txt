# GUI area 12, app: the application's start-up, its services and its data directory.
# Each source with its header (${gui_include_dir}/app/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    Application.cpp ${gui_include_dir}/app/Application.h
    ApplicationContext.cpp ${gui_include_dir}/app/ApplicationContext.h
    ApplicationSession.cpp ${gui_include_dir}/app/ApplicationSession.h
    DataPaths.cpp ${gui_include_dir}/app/DataPaths.h
    MotorDatabaseMetadataIO.cpp ${gui_include_dir}/app/MotorDatabaseMetadataIO.h
    StartupGuard.cpp ${gui_include_dir}/app/StartupGuard.h
)
