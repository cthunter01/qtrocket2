# Script mode (cmake -P), the test lint.layers (tests/lint/CMakeLists.txt): checks the layers of the code, which the
# compiler cannot (one static library can include any of its own headers).
#
#   cmake -DSOURCE_DIR=<the repository> -P CheckLayers.cmake
#
# 1. The core (include/QtRocket/<subsystem>/, src/<subsystem>/): a subsystem includes only itself and the subsystems
#    before it, in the order of CLAUDE.md (src/CMakeLists.txt adds them in that order): util, unit, logging,
#    preferences, material, motor, rocket, models, mass, aero, simulation, document, file, figure, with cli (the
#    command-line tool's commands) on top. A subsystem the order does not name is a violation too: a new one is
#    added here and in CLAUDE.md. figure/ may be there or not.
# 2. The GUI (gui/include/QtRocket/gui/<area>/, gui/src/<area>/): an area includes only itself and the areas before
#    it, in the order of plan section 9.2: util, settings, binding, widgets, tree, figure, configdialog, dialogs,
#    main, app, with gui/main.cpp on top; any subsystem of the core, but never QtRocket/cli (the GUI does not link
#    the command-line tool's library).
# 3. The core is Qt-free: no Qt header in include/ or src/.
# 4. In gui/: no `new` outside gui/include/QtRocket/gui/util/MakeChild.h (makeChild() and makeQtOwned() hold the
#    GUI's only `new`), no Q_SLOTS (function-pointer connect() needs none), no <windows.h> (its macros ABSOLUTE,
#    RELATIVE and ERROR clash with enum constants of the core), no .ui file (layouts are built in code), no .qrc
#    file (resources go through qt_add_resources(): a .qrc in the static library loses its files at run time), and
#    binding/ is Qt-free: no Qt header and no header of another GUI area (they all use Qt).
# Comments and string literals do not count for the `new` and Q_SLOTS checks. Every violation is listed, then the
# script fails.

cmake_minimum_required(VERSION 3.28)
if(NOT DEFINED SOURCE_DIR OR NOT IS_DIRECTORY "${SOURCE_DIR}")
    message(FATAL_ERROR "CheckLayers.cmake requires -DSOURCE_DIR=<the repository>")
endif()
include(${CMAKE_CURRENT_LIST_DIR}/LintSupport.cmake)

set(core_order util unit logging preferences material motor rocket models mass aero simulation document file figure
    cli)
set(gui_order util settings binding widgets tree figure configdialog dialogs main app)
list(LENGTH gui_order gui_top)   # gui/main.cpp, above every area

set(violations "")
set(checked 0)

# Appends "<file>:<line>: <text>" to the violations of the caller.
function(add_violation file line text)
    file(RELATIVE_PATH relative "${SOURCE_DIR}" "${file}")
    set(violations "${violations}  ${relative}:${line}: ${text}\n" PARENT_SCOPE)
endfunction()

# The core: its include order and no Qt.
qtrocket_lint_sources(core_files "${SOURCE_DIR}/include" "${SOURCE_DIR}/src")
foreach(file IN LISTS core_files)
    file(RELATIVE_PATH relative "${SOURCE_DIR}" "${file}")
    # Two tests, not one with OR: CMake evaluates both sides, and the second MATCHES clears CMAKE_MATCH_1.
    if(relative MATCHES "^include/QtRocket/([^/]+)/")
        set(layer "${CMAKE_MATCH_1}")
    elseif(relative MATCHES "^src/([^/]+)/")
        set(layer "${CMAKE_MATCH_1}")
    else()
        add_violation("${file}" 1 "a source outside the subsystems of the core")
        continue()
    endif()
    list(FIND core_order "${layer}" layer_index)
    if(layer_index EQUAL -1)
        add_violation("${file}" 1 "'${layer}' is not a subsystem of the core's layer order (cmake/CheckLayers.cmake, CLAUDE.md)")
        continue()
    endif()
    math(EXPR checked "${checked} + 1")
    qtrocket_lint_read_lines(lines "${file}")
    set(number 0)
    foreach(line IN LISTS lines)
        math(EXPR number "${number} + 1")
        qtrocket_lint_include(path "${line}")
        if(path STREQUAL "")
            continue()
        endif()
        qtrocket_lint_is_qt_include(qt "${path}")
        if(qt)
            add_violation("${file}" ${number} "the core includes the Qt header <${path}>: the core is Qt-free")
        elseif(path MATCHES "^QtRocket/gui/")
            add_violation("${file}" ${number} "the core includes the GUI header \"${path}\"")
        elseif(path MATCHES "^QtRocket/([^/]+)/")
            set(included "${CMAKE_MATCH_1}")
            list(FIND core_order "${included}" included_index)
            if(included_index EQUAL -1)
                add_violation("${file}" ${number} "\"${path}\": '${included}' is not a subsystem of the core's layer order")
            elseif(included_index GREATER layer_index)
                add_violation("${file}" ${number}
                    "${layer} includes ${included} (\"${path}\"), which comes after it in the core's layer order")
            endif()
        endif()
    endforeach()
endforeach()

# The GUI: its area order, binding/ Qt-free, and the rules of gui/.
set(gui_dir "${SOURCE_DIR}/gui")
qtrocket_lint_sources(gui_files "${gui_dir}")
foreach(file IN LISTS gui_files)
    file(RELATIVE_PATH relative "${gui_dir}" "${file}")
    set(area "")
    if(relative MATCHES "^include/QtRocket/gui/([^/]+)/")
        set(area "${CMAKE_MATCH_1}")
    elseif(relative MATCHES "^src/([^/]+)/")
        set(area "${CMAKE_MATCH_1}")
    endif()
    if(NOT area STREQUAL "")
        list(FIND gui_order "${area}" area_index)
        if(area_index EQUAL -1)
            add_violation("${file}" 1 "'${area}' is not an area of the GUI's layer order (cmake/CheckLayers.cmake, CLAUDE.md)")
            continue()
        endif()
    elseif(relative MATCHES "^[^/]+$")
        set(area "the application")
        set(area_index ${gui_top})
    else()
        add_violation("${file}" 1 "a GUI source outside the areas (gui/include/QtRocket/gui/<area>/, gui/src/<area>/)")
        continue()
    endif()
    math(EXPR checked "${checked} + 1")
    set(make_child FALSE)
    if(relative STREQUAL "include/QtRocket/gui/util/MakeChild.h")
        set(make_child TRUE)
    endif()
    qtrocket_lint_read_lines(lines "${file}")
    set(number 0)
    set(in_comment FALSE)
    foreach(line IN LISTS lines)
        math(EXPR number "${number} + 1")
        qtrocket_lint_include(path "${line}")
        if(NOT path STREQUAL "")
            qtrocket_lint_is_qt_include(qt "${path}")
            string(TOLOWER "${path}" lower_path)
            if(lower_path STREQUAL "windows.h" OR lower_path STREQUAL "qt_windows.h")
                add_violation("${file}" ${number}
                    "<${path}>: its macros (ABSOLUTE, RELATIVE, ERROR, ...) clash with enum constants of the core")
            elseif(qt AND area STREQUAL "binding")
                add_violation("${file}" ${number} "binding/ includes the Qt header <${path}>: binding/ is Qt-free")
            elseif(path MATCHES "^QtRocket/cli/")
                add_violation("${file}" ${number}
                    "the GUI includes \"${path}\": it does not use the command-line tool's library")
            elseif(path MATCHES "^QtRocket/gui/([^/]+)/")
                set(included "${CMAKE_MATCH_1}")
                list(FIND gui_order "${included}" included_index)
                if(included_index EQUAL -1)
                    add_violation("${file}" ${number} "\"${path}\": '${included}' is not an area of the GUI's layer order")
                elseif(area STREQUAL "binding" AND NOT included STREQUAL "binding")
                    add_violation("${file}" ${number}
                        "binding/ includes \"${path}\": binding/ is Qt-free and includes no other GUI area")
                elseif(included_index GREATER area_index)
                    add_violation("${file}" ${number}
                        "${area} includes ${included} (\"${path}\"), which comes after it in the GUI's layer order")
                endif()
            elseif(path MATCHES "^QtRocket/([^/]+)/")
                list(FIND core_order "${CMAKE_MATCH_1}" included_index)
                if(included_index EQUAL -1)
                    add_violation("${file}" ${number} "\"${path}\": '${CMAKE_MATCH_1}' is not a subsystem of the core")
                endif()
            endif()
        endif()
        qtrocket_lint_strip_line(line in_comment)
        if(NOT make_child AND line MATCHES "(^|[^A-Za-z0-9_])new([^A-Za-z0-9_]|$)")
            add_violation("${file}" ${number} "`new`: a parented QObject is made with makeChild() (util/MakeChild.h)")
        endif()
        if(line MATCHES "(^|[^A-Za-z0-9_])Q_SLOTS([^A-Za-z0-9_]|$)")
            add_violation("${file}" ${number} "Q_SLOTS: connect() takes member function pointers and lambdas")
        endif()
    endforeach()
endforeach()

file(GLOB_RECURSE forbidden_files LIST_DIRECTORIES false "${gui_dir}/*.ui" "${gui_dir}/*.qrc")
foreach(file IN LISTS forbidden_files)
    if(file MATCHES "\\.ui$")
        add_violation("${file}" 1 "a .ui file: layouts are built in code")
    else()
        add_violation("${file}" 1 "a .qrc file: resources go through qt_add_resources() (gui/resources/CMakeLists.txt)")
    endif()
endforeach()

if(NOT violations STREQUAL "")
    message("${violations}")
    message(FATAL_ERROR "lint.layers: the layers are broken (the list above, cmake/CheckLayers.cmake says the rules)")
endif()
message(STATUS "lint.layers: ${checked} files checked, the layers hold")
