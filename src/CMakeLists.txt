# The library holds the code; the executable and the tests link against it.
add_library(QtRocket_lib STATIC greet.cpp)
add_library(QtRocket::lib ALIAS QtRocket_lib)
target_include_directories(QtRocket_lib PUBLIC ${PROJECT_SOURCE_DIR}/include)
QtRocket_configure_target(QtRocket_lib)

add_executable(QtRocket main.cpp)
target_link_libraries(QtRocket PRIVATE QtRocket::lib)
QtRocket_configure_target(QtRocket)

include(GNUInstallDirs)
install(TARGETS QtRocket RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
