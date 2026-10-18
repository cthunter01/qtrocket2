# QtRocket_gui: the Qt 6 Widgets GUI (CLAUDE.md, section GUI), a static library linked by the QtRocket application
# (main.cpp) and by QtRocket_gui_tests (tests/gui). Headers are in include/QtRocket/gui/<area>/, sources in
# src/<area>/; each src/<area>/CMakeLists.txt adds its .cpp files and its headers with target_sources(). The areas
# are added in their include order: each includes only the areas before it (and any subsystem of the core), which
# the lint.layers test checks (cmake/CheckLayers.cmake).
add_library(QtRocket_gui STATIC)
add_library(QtRocket::gui ALIAS QtRocket_gui)
target_include_directories(QtRocket_gui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(QtRocket_gui PUBLIC QtRocket::core Qt6::Widgets Qt6::Svg)
# Also every program that includes a GUI header must compile without Qt's keywords: the core's headers call
# Signal::emit(), which Qt's emit macro would break.
target_compile_definitions(QtRocket_gui PUBLIC QT_NO_KEYWORDS)
QtRocket_configure_qt_target(QtRocket_gui)

# The headers of the areas, which their CMakeLists.txt list beside the sources (AUTOMOC reads only listed headers).
set(gui_include_dir ${CMAKE_CURRENT_SOURCE_DIR}/include/QtRocket/gui)
add_subdirectory(src/util)
add_subdirectory(src/settings)
add_subdirectory(src/binding)
add_subdirectory(src/widgets)
add_subdirectory(src/tree)
add_subdirectory(src/figure)
add_subdirectory(src/configdialog)
add_subdirectory(src/dialogs)
add_subdirectory(src/main)
add_subdirectory(src/app)
add_subdirectory(resources)

# The application. WIN32_EXECUTABLE: no console window on Windows (Qt's entry point provides WinMain). The macOS
# bundle and the install() rules come with the release archives of the GUI (Milestone 9).
qt_add_executable(QtRocket WIN32 main.cpp)
target_link_libraries(QtRocket PRIVATE QtRocket::gui)
add_dependencies(QtRocket QtRocket_data)
QtRocket_configure_qt_target(QtRocket)
