# QtRocket_core: the Qt-free model, physics, simulation and file I/O. Each subsystem directory adds its sources
# with target_sources(). The executables (cli/, later gui/) and the tests link against it.
add_library(QtRocket_core STATIC)
add_library(QtRocket::core ALIAS QtRocket_core)
target_include_directories(QtRocket_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_definitions(QtRocket_core PRIVATE QTROCKET_VERSION="${PROJECT_VERSION}")
target_link_libraries(QtRocket_core PRIVATE pugixml::pugixml MINIZIP::minizip-ng SQLite::SQLite3 FastFloat::fast_float)
QtRocket_configure_target(QtRocket_core)

# Subsystems, in dependency order (each may include only the ones before it). unit/ comes before
# logging/, a deviation from the plan's util -> logging -> unit order: OpenRocket's Warning formats
# its speeds and angles through UnitGroup, and unit/ needs nothing from logging/. motor/ comes
# before rocket/, another deviation: motor mounts and motor configurations hold motors, and the
# motor classes need nothing from the rocket model. MotorClusterState and ThrustState, which
# OpenRocket keeps in its simulation package, live in mass/: MassCalculation needs them, and
# simulation/ builds on mass/ (its steppers call MassCalculator), never the reverse. document/ (the
# document with its undo history, its attachments and its decal images) comes after simulation/
# and before file/: Simulation, which OpenRocket keeps in its document package, lives in
# simulation/ (the engine needs it), the document holds simulations, and the loaders and savers
# of file/ fill and read a document. So document/ never includes file/ (the attachments that read
# an archive are in file/), and simulation/ never includes document/.
add_subdirectory(util)
add_subdirectory(unit)
add_subdirectory(logging)
add_subdirectory(preferences)
add_subdirectory(material)
add_subdirectory(motor)
add_subdirectory(rocket)
add_subdirectory(models)
add_subdirectory(mass)
add_subdirectory(aero)
add_subdirectory(simulation)
add_subdirectory(document)
add_subdirectory(file)

add_subdirectory(cli)
