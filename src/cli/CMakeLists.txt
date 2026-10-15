# QtRocket_cli_lib: the command-line tool's commands (include/QtRocket/cli), linked into the executable and into
# QtRocket_core_tests, which runs them in its own process.
add_library(QtRocket_cli_lib STATIC
    Cli.cpp
    CliArguments.cpp
    CliEnvironment.cpp
    CliOutput.cpp
    DataDirectory.cpp
    InfoCommand.cpp
    MotorsCommand.cpp
    SimulateCommand.cpp
)
add_library(QtRocket::cli ALIAS QtRocket_cli_lib)
target_link_libraries(QtRocket_cli_lib PUBLIC QtRocket::core)
if(WIN32)
    # DataDirectory.cpp: windows.h without its min/max macros, and CommandLineToArgvW from shell32.
    target_compile_definitions(QtRocket_cli_lib PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(QtRocket_cli_lib PRIVATE shell32)
endif()
QtRocket_configure_target(QtRocket_cli_lib)

add_executable(QtRocket_cli main.cpp)
target_link_libraries(QtRocket_cli PRIVATE QtRocket::cli)
QtRocket_configure_target(QtRocket_cli)

# The build tree has the installed layout: bin/QtRocket_cli finds its motor database in share/QtRocket/motors
# (DataDirectory), so a copy of the database goes there whenever the executable is built.
set(cli_build_motor_database ${PROJECT_BINARY_DIR}/share/QtRocket/motors/initial_motors.db)
add_custom_command(OUTPUT ${cli_build_motor_database}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/share/QtRocket/motors
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/data/motors/initial_motors.db ${cli_build_motor_database}
    DEPENDS ${PROJECT_SOURCE_DIR}/data/motors/initial_motors.db
    COMMENT "Copying the motor database into the build tree"
    VERBATIM)
add_custom_target(QtRocket_cli_data DEPENDS ${cli_build_motor_database})
add_dependencies(QtRocket_cli QtRocket_cli_data)

include(GNUInstallDirs)
install(TARGETS QtRocket_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
