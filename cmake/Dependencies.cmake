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
