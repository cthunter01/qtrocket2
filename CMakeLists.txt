cmake_minimum_required(VERSION 3.28)   # FetchContent SYSTEM + EXCLUDE_FROM_ALL + FIND_PACKAGE_ARGS
project(QtRocket VERSION 0.2.0
        DESCRIPTION "Model rocket design and flight simulation, a C++ re-implementation of OpenRocket"
        LANGUAGES C CXX)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Use a preset: cmake --preset clang-debug")
endif()

# Presets set the build type; this covers a bare `cmake -B build`. A parent project keeps its own choice.
if(PROJECT_IS_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)     # no C++ modules; remove this line to use them
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

option(QTROCKET_BUILD_TESTS "Build the unit tests" ${PROJECT_IS_TOP_LEVEL})
# The GUI needs Qt 6.8 or newer; without Qt, configure with -DQTROCKET_BUILD_GUI=OFF for the core and the CLI alone.
option(QTROCKET_BUILD_GUI "Build the Qt 6 GUI: QtRocket_gui, the QtRocket application and its tests" ON)
option(QTROCKET_BUILD_DOCS "Add a 'docs' target when Doxygen is found" ${PROJECT_IS_TOP_LEVEL})
option(QTROCKET_WARNINGS_AS_ERRORS "Treat compiler and clang-tidy warnings as errors" OFF)
option(QTROCKET_ENABLE_COVERAGE "Clang source-based coverage and a 'coverage' target" OFF)
option(QTROCKET_ENABLE_CLANG_TIDY "Run clang-tidy on our sources while compiling" OFF)
option(QTROCKET_ENABLE_IPO "Link-time optimisation for our targets" OFF)
set(QTROCKET_SANITIZERS "" CACHE STRING "Sanitizers for our targets, e.g. address;undefined or thread")

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM AND NOT CMAKE_CXX_COMPILER_LAUNCHER)   # a launcher the user chose wins
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# Point clangd (in any editor) at the most recently configured preset.
# Windows allows symlinks only in Developer Mode (or as admin), so a failure is a note, not an error.
if(PROJECT_IS_TOP_LEVEL)
    file(CREATE_LINK ${PROJECT_BINARY_DIR}/compile_commands.json
         ${PROJECT_SOURCE_DIR}/compile_commands.json RESULT link_result SYMBOLIC)
    if(NOT link_result EQUAL 0)
        message(STATUS "Could not link compile_commands.json into the source tree (${link_result}). "
                       "On Windows, turn on Developer Mode to allow symlinks.")
    endif()
endif()

# Project modules are included by path, so a parent's CMAKE_MODULE_PATH cannot replace them.
include(${PROJECT_SOURCE_DIR}/cmake/ProjectOptions.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/Dependencies.cmake)

# QtRocket_data: the build tree has the installed layout, so the programs find data/ (the motor database and the
# example designs) in share/QtRocket beside their bin/, as in an archive (src/cli/DataDirectory.cpp, and the GUI's own
# rule). The programs and the test executables that read it depend on this copy.
file(GLOB_RECURSE qtrocket_data_files CONFIGURE_DEPENDS LIST_DIRECTORIES false
     RELATIVE ${PROJECT_SOURCE_DIR}/data ${PROJECT_SOURCE_DIR}/data/*)
set(qtrocket_data_copies "")
foreach(data_file IN LISTS qtrocket_data_files)
    set(data_copy ${PROJECT_BINARY_DIR}/share/QtRocket/${data_file})
    cmake_path(GET data_copy PARENT_PATH data_copy_directory)
    add_custom_command(OUTPUT ${data_copy}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${data_copy_directory}
        COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/data/${data_file} ${data_copy}
        DEPENDS ${PROJECT_SOURCE_DIR}/data/${data_file}
        COMMENT "Copying data/${data_file} into the build tree"
        VERBATIM)
    list(APPEND qtrocket_data_copies ${data_copy})
endforeach()
add_custom_target(QtRocket_data DEPENDS ${qtrocket_data_copies})

add_subdirectory(src)

# The GUI (gui/). Qt, which it requires when it is on, and rocketplot, its plots, are found or fetched in
# cmake/Dependencies.cmake (its GUI section, with the macOS work-around for Qt 6.8.3).
if(QTROCKET_BUILD_GUI)
    add_subdirectory(gui)
endif()

# Shipped data files: the motor database and the example designs (read at run time from share/QtRocket).
include(GNUInstallDirs)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/data/ DESTINATION ${CMAKE_INSTALL_DATADIR}/QtRocket)
# The licence, OpenRocket's and the third-party notices, and the README go with every archive (share/doc/QtRocket).
install(FILES ${PROJECT_SOURCE_DIR}/LICENSE ${PROJECT_SOURCE_DIR}/NOTICE ${PROJECT_SOURCE_DIR}/README.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR})

if(QTROCKET_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(QTROCKET_ENABLE_COVERAGE)
    include(${PROJECT_SOURCE_DIR}/cmake/Coverage.cmake)
endif()
if(QTROCKET_BUILD_DOCS)
    include(${PROJECT_SOURCE_DIR}/cmake/Docs.cmake)
endif()

# Release archives of what the install() rules install, named <project>-<version>-<CPACK_SYSTEM_NAME>.
# Made by the dist-* workflow presets (cmake --workflow --preset dist-linux) and the Release GitHub workflow.
if(PROJECT_IS_TOP_LEVEL)
    include(CPack)
endif()
