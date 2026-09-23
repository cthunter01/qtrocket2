add_executable(QtRocket_cli main.cpp)
target_link_libraries(QtRocket_cli PRIVATE QtRocket::core)
QtRocket_configure_target(QtRocket_cli)

include(GNUInstallDirs)
install(TARGETS QtRocket_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
