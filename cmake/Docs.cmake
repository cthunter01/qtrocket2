# 'docs' target: Doxygen HTML for the public headers, if Doxygen is installed.
#   cmake --build --preset clang-debug --target docs
# Output: build/<preset>/docs/html/index.html. Not part of 'all'.

find_package(Doxygen OPTIONAL_COMPONENTS dot)
if(NOT DOXYGEN_FOUND)
    message(STATUS "Doxygen not found; the 'docs' target is unavailable")
    return()
endif()

set(DOXYGEN_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/docs)
# File names relative to the repository, and each class's include line as the code writes it:
# #include "QtRocket/rocket/BodyTube.h" (quotes, not angle brackets: CLAUDE.md).
set(DOXYGEN_STRIP_FROM_PATH ${PROJECT_SOURCE_DIR})
set(DOXYGEN_STRIP_FROM_INC_PATH ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/gui/include)
set(DOXYGEN_FORCE_LOCAL_INCLUDES YES)
set(DOXYGEN_USE_MDFILE_AS_MAINPAGE ${PROJECT_SOURCE_DIR}/README.md)
set(DOXYGEN_EXTRACT_ALL YES)
set(DOXYGEN_WARN_IF_UNDOCUMENTED NO)
set(DOXYGEN_EXTENSION_MAPPING "h=C++")   # headers are .h, but always C++
if(DOXYGEN_DOT_FOUND)
    set(DOXYGEN_HAVE_DOT YES)
endif()

# The core's headers and, with the GUI, its headers (gui/include: "QtRocket/gui/util/MakeChild.h"); README.md as the
# main page and the page it links to, the GUI's differences from OpenRocket.
set(docs_inputs ${PROJECT_SOURCE_DIR}/include)
if(QTROCKET_BUILD_GUI)
    list(APPEND docs_inputs ${PROJECT_SOURCE_DIR}/gui/include)
endif()
doxygen_add_docs(docs
    ${docs_inputs}
    ${PROJECT_SOURCE_DIR}/README.md
    ${PROJECT_SOURCE_DIR}/docs/gui-differences.md
    COMMENT "Generating Doxygen HTML")
