# QtRocket_core: the Qt-free model, physics, simulation and file I/O. Each subsystem directory adds its sources
# with target_sources(). The executables (cli/, later gui/) and the tests link against it.
add_library(QtRocket_core STATIC)
add_library(QtRocket::core ALIAS QtRocket_core)
target_include_directories(QtRocket_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_definitions(QtRocket_core PRIVATE QTROCKET_VERSION="${PROJECT_VERSION}")
target_link_libraries(QtRocket_core PRIVATE pugixml::pugixml MINIZIP::minizip-ng SQLite::SQLite3)
QtRocket_configure_target(QtRocket_core)

# Subsystems, in dependency order (each may include only the ones before it).
add_subdirectory(util)
add_subdirectory(file)

add_subdirectory(cli)
