# SQLite: the installed library when there is one (FindSQLite3), otherwise the amalgamation built from source.
# The amalgamation zip has no CMakeLists.txt, so FetchContent only downloads it and the target is defined here.
FetchContent_Declare(sqlite3
    URL      https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip
    URL_HASH SHA256=1e71ddf93849c6a6ecf58b827c0692073d2dd7ee40196158068f7b29f422e87d
    SYSTEM
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS NAMES SQLite3)
FetchContent_MakeAvailable(sqlite3)

if(NOT TARGET SQLite::SQLite3)
    add_library(sqlite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
    target_include_directories(sqlite3 SYSTEM PUBLIC ${sqlite3_SOURCE_DIR})
    # Read-only use of a bundled database: no extension loading, no double-quoted string literals.
    target_compile_definitions(sqlite3 PRIVATE
        SQLITE_OMIT_LOAD_EXTENSION SQLITE_THREADSAFE=1 SQLITE_DQS=0 SQLITE_OMIT_DEPRECATED)
    target_compile_options(sqlite3 PRIVATE $<IF:$<C_COMPILER_ID:MSVC>,/w,-w>)
    find_package(Threads REQUIRED)
    target_link_libraries(sqlite3 PRIVATE Threads::Threads)
    add_library(SQLite::SQLite3 ALIAS sqlite3)
endif()
