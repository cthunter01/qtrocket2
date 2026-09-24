target_sources(QtRocket_core PRIVATE
    GzipStream.cpp
    motor/AbstractMotorLoader.cpp
    motor/GeneralMotorLoader.cpp
    motor/MotorDatabaseLoader.cpp
    motor/RaspMotorLoader.cpp
    motor/RockSimMotorLoader.cpp
    motor/RockSimMotorWriter.cpp
    motor/ZipFileMotorLoader.cpp
    simplesax/AbstractElementHandler.cpp
    simplesax/DelegatorHandler.cpp
    simplesax/NullElementHandler.cpp
    simplesax/PlainTextHandler.cpp
    simplesax/SimpleSax.cpp
    XmlScanner.cpp
    ZipArchive.cpp
    ZipInputStream.cpp
)
