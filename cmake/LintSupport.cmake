# Helpers of the lint scripts that the lint.* tests run (cmake -P): CheckLayers.cmake and CheckNoQtTestMacros.cmake.

# qtrocket_lint_sources(<out_var> <directory>...)
# Every C++ file (.h, .hpp, .cpp, .cc, .cxx, .inl, .ipp, and Objective-C++ .mm, where macOS-only code would go) under
# the directories, sorted. A directory that does not exist gives none.
function(qtrocket_lint_sources out_var)
    set(files "")
    foreach(directory IN LISTS ARGN)
        if(IS_DIRECTORY "${directory}")
            file(GLOB_RECURSE found LIST_DIRECTORIES false
                 "${directory}/*.h" "${directory}/*.hpp" "${directory}/*.cpp" "${directory}/*.cc" "${directory}/*.cxx"
                 "${directory}/*.inl" "${directory}/*.ipp" "${directory}/*.mm")
            list(APPEND files ${found})
        endif()
    endforeach()
    list(SORT files)
    set(${out_var} "${files}" PARENT_SCOPE)
endfunction()

# The characters a CMake list cannot hold as they are ('\', ';', '[', ']') stand for themselves as control characters
# in the lines that qtrocket_lint_read_lines() gives; qtrocket_lint_unescape() puts them back.
string(ASCII 1 qtrocket_lint_backslash)
string(ASCII 2 qtrocket_lint_semicolon)
string(ASCII 3 qtrocket_lint_open_bracket)
string(ASCII 4 qtrocket_lint_close_bracket)

# qtrocket_lint_read_lines(<out_var> <file>)
# The lines of the file as a list, one element per line (empty lines included, so that the position in the list is
# the line number less one).
function(qtrocket_lint_read_lines out_var path)
    file(READ "${path}" text)
    string(REPLACE "\r" "" text "${text}")
    string(REPLACE "\\" "${qtrocket_lint_backslash}" text "${text}")
    string(REPLACE ";" "${qtrocket_lint_semicolon}" text "${text}")
    string(REPLACE "[" "${qtrocket_lint_open_bracket}" text "${text}")
    string(REPLACE "]" "${qtrocket_lint_close_bracket}" text "${text}")
    string(REPLACE "\n" ";" text "${text}")
    set(${out_var} "${text}" PARENT_SCOPE)
endfunction()

# qtrocket_lint_unescape(<out_var> <line>)
# A line of qtrocket_lint_read_lines() as the file has it, for a message.
function(qtrocket_lint_unescape out_var line)
    string(REPLACE "${qtrocket_lint_backslash}" "\\" line "${line}")
    string(REPLACE "${qtrocket_lint_semicolon}" ";" line "${line}")
    string(REPLACE "${qtrocket_lint_open_bracket}" "[" line "${line}")
    string(REPLACE "${qtrocket_lint_close_bracket}" "]" line "${line}")
    string(STRIP "${line}" line)
    set(${out_var} "${line}" PARENT_SCOPE)
endfunction()

# qtrocket_lint_strip_line(<line_var> <in_comment_var>)
# Replaces the line in <line_var> by its code: without its comments, and with every character and string literal
# emptied. <in_comment_var> is TRUE while a /* comment is open, from one line to the next (start each file with
# FALSE). Good enough for a lint, not a C++ lexer: an escaped quote inside a string literal ("\"") or a raw string may
# leave a little text in or take a little out.
function(qtrocket_lint_strip_line line_var in_comment_var)
    set(line "${${line_var}}")
    set(in_comment "${${in_comment_var}}")
    string(REGEX REPLACE "'([^'${qtrocket_lint_backslash}]|${qtrocket_lint_backslash}.)'" "''" line "${line}")
    string(REGEX REPLACE "\"[^\"]*\"" "\"\"" line "${line}")
    set(code "")
    while(NOT line STREQUAL "")
        if(in_comment)
            string(FIND "${line}" "*/" end)
            if(end EQUAL -1)
                set(line "")
            else()
                math(EXPR end "${end} + 2")
                string(SUBSTRING "${line}" ${end} -1 line)
                set(in_comment FALSE)
            endif()
        else()
            string(FIND "${line}" "//" line_comment)
            string(FIND "${line}" "/*" block_comment)
            if(line_comment EQUAL -1 AND block_comment EQUAL -1)
                string(APPEND code "${line}")
                set(line "")
            elseif(block_comment EQUAL -1 OR (NOT line_comment EQUAL -1 AND line_comment LESS block_comment))
                string(SUBSTRING "${line}" 0 ${line_comment} before)
                string(APPEND code "${before}")
                set(line "")
            else()
                string(SUBSTRING "${line}" 0 ${block_comment} before)
                string(APPEND code "${before} ")
                math(EXPR start "${block_comment} + 2")
                string(SUBSTRING "${line}" ${start} -1 line)
                set(in_comment TRUE)
            endif()
        endif()
    endwhile()
    set(${line_var} "${code}" PARENT_SCOPE)
    set(${in_comment_var} "${in_comment}" PARENT_SCOPE)
endfunction()

# qtrocket_lint_include(<out_var> <line> [<quoted_var>])
# The path that an #include (or Objective-C #import) line names ("QtRocket/rocket/BodyTube.h", "QWidget", ...), or ""
# for another line; <quoted_var> is TRUE for "path", FALSE for <path>.
function(qtrocket_lint_include out_var line)
    set(path "")
    set(quoted FALSE)
    if(line MATCHES "^[ \t]*#[ \t]*(include|import)[ \t]*([\"<])([^\">]+)[\">]")
        set(path "${CMAKE_MATCH_3}")
        if(CMAKE_MATCH_2 STREQUAL "\"")
            set(quoted TRUE)
        endif()
    endif()
    set(${out_var} "${path}" PARENT_SCOPE)
    if(ARGC GREATER 2)
        set(${ARGV2} ${quoted} PARENT_SCOPE)
    endif()
endfunction()

# qtrocket_lint_include_path_problem(<out_var> <file> <path> <quoted>)
# What is wrong with the path of an include of the file, or "": an include climbs out of no directory ("../x.h"),
# and a quoted one names "QtRocket/<subsystem>/..." or a private header beside the file ("MinizipHandle.h"), so that
# the layer checks see what it includes ("../main/MainWindow.h" or "widgets/Spinner.h" would get past them).
function(qtrocket_lint_include_path_problem out_var file path quoted)
    set(problem "")
    string(REPLACE "${qtrocket_lint_backslash}" "/" slashed "${path}")
    if(slashed MATCHES "(^|/)\\.\\.(/|$)")
        set(problem "a relative include: name the header as \"QtRocket/<subsystem>/<header>\"")
    elseif(quoted AND NOT slashed MATCHES "^QtRocket/")
        get_filename_component(directory "${file}" DIRECTORY)
        if(slashed MATCHES "/")
            set(problem "a quoted include is \"QtRocket/<subsystem>/<header>\" or a header beside the file")
        elseif(NOT EXISTS "${directory}/${slashed}")
            set(problem "not a header beside the file: name the header as \"QtRocket/<subsystem>/<header>\"")
        endif()
    endif()
    set(${out_var} "${problem}" PARENT_SCOPE)
endfunction()

# qtrocket_lint_is_qt_include(<out_var> <path>)
# TRUE when the included path is one of Qt's headers: <QWidget>, <Qt>, <QtCore/qobject.h>, <qglobal.h>, ...
# (QtRocket's own "QtRocket/..." is not).
function(qtrocket_lint_is_qt_include out_var path)
    set(qt FALSE)
    if(path MATCHES "^Q" AND NOT path MATCHES "^QtRocket/")
        set(qt TRUE)
    elseif(path MATCHES "^q[a-z0-9_]*\\.h$")
        set(qt TRUE)
    endif()
    set(${out_var} ${qt} PARENT_SCOPE)
endfunction()

# qtrocket_lint_is_rocketplot_include(<out_var> <path>)
# TRUE when the included path is one of rocketplot's headers: <rocketplot/PlotWidget.h>, ... (in any case, as the file
# systems of Windows and macOS find <RocketPlot/PlotWidget.h> too).
function(qtrocket_lint_is_rocketplot_include out_var path)
    string(TOLOWER "${path}" lower_path)
    set(rocketplot FALSE)
    if(lower_path MATCHES "^rocketplot/")
        set(rocketplot TRUE)
    endif()
    set(${out_var} ${rocketplot} PARENT_SCOPE)
endfunction()
