include(FetchContent)

# FIND_PACKAGE_ARGS: an installed package (find_package) wins; otherwise the source is downloaded.
# SYSTEM: the dependency's headers are system headers, so our warnings and clang-tidy skip them.
# EXCLUDE_FROM_ALL: only the parts of the dependency we link against get built.

if(QTROCKET_BUILD_TESTS)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    # MSVC: link the same C runtime as our targets (CMAKE_MSVC_RUNTIME_LIBRARY: the DLL one unless a preset
    # says otherwise) instead of GoogleTest's static default.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.18.0
        GIT_SHALLOW    TRUE
        SYSTEM
        EXCLUDE_FROM_ALL
        FIND_PACKAGE_ARGS NAMES GTest)
    FetchContent_MakeAvailable(googletest)
    # A GoogleTest built from source links into the test binary, which keeps one copy of each inline std::vector
    # member: some from our annotated code (ProjectOptions.cmake: _GLIBCXX_SANITIZE_VECTOR, and libc++ annotates
    # under ASan by default), some from GoogleTest's. A vector reserved by an annotated copy and filled by an
    # unannotated one is a false container-overflow, so GoogleTest gets the same instrumentation. An installed
    # GoogleTest is imported (a separate library with its own copies) and is left alone.
    if("address" IN_LIST QTROCKET_SANITIZERS)
        foreach(gtest_target IN ITEMS gtest gtest_main gmock gmock_main)
            if(TARGET ${gtest_target})
                get_target_property(gtest_imported ${gtest_target} IMPORTED)
                if(NOT gtest_imported)
                    target_compile_options(${gtest_target} PRIVATE -fsanitize=address)
                    target_compile_definitions(${gtest_target} PRIVATE _GLIBCXX_SANITIZE_VECTOR)
                endif()
            endif()
        endforeach()
    endif()
endif()

# ---- QtRocket_core dependencies (all Qt-free) ----

# XML: .ork documents, .rse motor files, later .orc presets and RockSim .rkt.
FetchContent_Declare(pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG        v1.16
    GIT_SHALLOW    TRUE
    SYSTEM
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS NAMES pugixml)
FetchContent_MakeAvailable(pugixml)

# Decimal parsing (Strings::parseDouble): std::from_chars for floating point is missing from Apple's libc++
# (availability-gated), so every platform parses with fast_float, the header-only library that libstdc++'s own
# from_chars wraps, and gets the same correctly rounded result.
set(FASTFLOAT_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(fast_float
    GIT_REPOSITORY https://github.com/fastfloat/fast_float.git
    GIT_TAG        v8.3.0
    GIT_SHALLOW    TRUE
    SYSTEM
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS NAMES FastFloat)
FetchContent_MakeAvailable(fast_float)

# Zip and gzip: an .ork file is a zip (rocket.ork, preview.png, decals, thrustcurves/*.rse) or a gzip stream.
# minizip-ng brings zlib itself: the installed one when found, otherwise (and always in the dist presets, which
# never take libraries from the build machine) zlib-ng in zlib-compatible mode, built from source.
set(MZ_COMPAT           OFF CACHE BOOL "" FORCE)
# Plain zlib, not zlib-ng: distributions ship zlib-ng without the static target minizip-ng asks for.
set(MZ_ZLIB_FLAVOR      "zlib" CACHE STRING "" FORCE)
set(MZ_ZLIB             ON  CACHE BOOL "" FORCE)
set(MZ_BZIP2            OFF CACHE BOOL "" FORCE)
set(MZ_LZMA             OFF CACHE BOOL "" FORCE)
set(MZ_PPMD             OFF CACHE BOOL "" FORCE)
set(MZ_ZSTD             OFF CACHE BOOL "" FORCE)
set(MZ_LIBCOMP          OFF CACHE BOOL "" FORCE)
set(MZ_PKCRYPT          OFF CACHE BOOL "" FORCE)
set(MZ_WZAES            OFF CACHE BOOL "" FORCE)
set(MZ_OPENSSL          OFF CACHE BOOL "" FORCE)
set(MZ_LIBBSD           OFF CACHE BOOL "" FORCE)
set(MZ_ICONV            OFF CACHE BOOL "" FORCE)
set(MZ_BUILD_TESTS      OFF CACHE BOOL "" FORCE)
set(MZ_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(MZ_FETCH_LIBS       ON  CACHE BOOL "" FORCE)
if(FETCHCONTENT_TRY_FIND_PACKAGE_MODE STREQUAL "NEVER")
    set(MZ_FORCE_FETCH_LIBS ON CACHE BOOL "" FORCE)
endif()
# The zlib-ng that minizip-ng fetches, at a fixed release: its clone_repo() takes ZLIB_REPOSITORY and ZLIB_TAG when
# they are set, and the moving branch 'stable' otherwise.
set(ZLIB_REPOSITORY     https://github.com/zlib-ng/zlib-ng.git CACHE STRING "" FORCE)
set(ZLIB_TAG            2.3.3 CACHE STRING "" FORCE)
FetchContent_Declare(minizip-ng
    GIT_REPOSITORY https://github.com/zlib-ng/minizip-ng.git
    GIT_TAG        4.2.2
    GIT_SHALLOW    TRUE
    SYSTEM
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS NAMES minizip-ng)
FetchContent_MakeAvailable(minizip-ng)

# The bundled thrust-curve motor database (data/motors/initial_motors.db).
include(${PROJECT_SOURCE_DIR}/cmake/sqlite3.cmake)

# ---- test-only dependencies ----
if(QTROCKET_BUILD_TESTS)
    # Golden reference data (tests/data/goldens) is JSON. Never linked into the core.
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.12.0
        GIT_SHALLOW    TRUE
        SYSTEM
        EXCLUDE_FROM_ALL
        FIND_PACKAGE_ARGS NAMES nlohmann_json)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# ---- GUI dependencies (QTROCKET_BUILD_GUI): Qt and rocketplot ----
# Nothing of this section is found, fetched or installed without the GUI (-DQTROCKET_BUILD_GUI=OFF, the dist-*
# presets until Milestone 9).
if(QTROCKET_BUILD_GUI)
    # Qt is required when the GUI is on, never just used when found, so that a build without Qt cannot silently lose
    # the GUI. Qt 6.8 is the oldest Qt supported (CI builds it on macOS and Windows, the newest on Linux).
    set(qtrocket_qt_components Core Gui Widgets Svg)
    if(QTROCKET_BUILD_TESTS)
        list(APPEND qtrocket_qt_components Test)
    endif()
    # A work-around for Qt before 6.8.4 and 6.9.2 on macOS 26: Qt's FindWrapOpenGL.cmake links the AGL framework on
    # every Apple build ('-framework AGL' when it finds none), and the macOS 26 SDK no longer has it ('ld: framework
    # AGL not found'; Qt commit cdb33c3d5621 removed the linkage, which nothing of Qt needs). Its
    # find_library(WrapOpenGL_AGL NAMES AGL) keeps a value found before, so that variable names the OpenGL framework,
    # which Qt links anyway. On every Apple build: a search for AGL itself can find the run-time framework under
    # /System/Library/Frameworks although the SDK lacks it (CI's first macOS run). CI's macOS job builds against Qt
    # 6.8.3 on macOS 26. Remove this once the oldest Qt supported is 6.9.2 or newer. It comes before every
    # find_package(Qt6), rocketplot's own below included.
    if(APPLE)
        find_library(WrapOpenGL_AGL NAMES OpenGL)
    endif()
    find_package(Qt6 6.8 REQUIRED COMPONENTS ${qtrocket_qt_components})
    message(STATUS "QtRocket GUI: Qt ${Qt6_VERSION}")

    # Plots (from Milestone 3): rocketplot 0.6.0 (https://github.com/cthunter01/rocketplot, MIT; the tag is commit
    # c969fe1), a Qt 6 Widgets library, linked by QtRocket_gui (gui/CMakeLists.txt) and used only in gui/'s area
    # plot/ (CLAUDE.md, lint.layers). It runs its own find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Svg): after
    # ours, so that the AGL work-around above has run first. As a subproject it builds only its library: no tests,
    # demo, Designer plugin, docs or install rules of the library, but the install rule of its licence
    # (share/doc/rocketplot/LICENSE, component rocketplot_licenses).
    # - ROCKETPLOT_SANITIZERS = ours: libstdc++ annotates std::vector for AddressSanitizer only where
    #   _GLIBCXX_SANITIZE_VECTOR is defined, and a program keeps one copy of each inline vector member, so rocketplot's
    #   code is instrumented as ours is (an unsanitized one makes our asan build report a false container-overflow,
    #   which can appear late and far from rocketplot). For the same reason a sanitized build never takes an installed
    #   rocketplot (no FIND_PACKAGE_ARGS then; the asan, tsan and coverage presets also set
    #   FETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER).
    # - FIND_PACKAGE_ARGS 0.6: an installed rocketplot is taken only in the version QtRocket is written for (its
    #   package accepts the same minor version only, as a 0.x minor version may change the API); otherwise the tag is
    #   fetched.
    # - ROCKETPLOT_BUNDLE_MATH_FONT=OFF: no Latin Modern Math (700 KB, GUST Font License): QtRocket's plots set no math
    #   (every text goes through rocketplot::literalText()). ROCKETPLOT_BUILD_SHARED=OFF: a static library, as ours.
    # - SYSTEM: its headers are system headers, outside our warnings and clang-tidy; its own targets get none of our
    #   options (QtRocket_configure_target() is never called for them), so neither our warnings, -Werror, clang-tidy
    #   nor coverage reach its sources. No EXCLUDE_FROM_ALL: it drops every install rule of the subproject, its
    #   licence's among them.
    set(ROCKETPLOT_SANITIZERS "${QTROCKET_SANITIZERS}" CACHE STRING "" FORCE)
    set(ROCKETPLOT_BUNDLE_MATH_FONT OFF CACHE BOOL "" FORCE)
    set(ROCKETPLOT_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(rocketplot_find_package_args FIND_PACKAGE_ARGS 0.6 NAMES rocketplot)
    if(QTROCKET_SANITIZERS)
        set(rocketplot_find_package_args "")
    endif()
    FetchContent_Declare(rocketplot
        GIT_REPOSITORY https://github.com/cthunter01/rocketplot.git
        GIT_TAG        v0.6.0
        GIT_SHALLOW    TRUE
        SYSTEM
        ${rocketplot_find_package_args})
    FetchContent_MakeAvailable(rocketplot)
endif()
