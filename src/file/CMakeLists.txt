target_sources(QtRocket_core PRIVATE
    GzipStream.cpp
    motor/AbstractMotorLoader.cpp
    motor/GeneralMotorLoader.cpp
    motor/MotorDatabaseLoader.cpp
    motor/RaspMotorLoader.cpp
    motor/RockSimMotorLoader.cpp
    motor/RockSimMotorWriter.cpp
    motor/ZipFileMotorLoader.cpp
    ZipArchive.cpp
)
