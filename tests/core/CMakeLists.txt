# QtRocket_core_tests: every test of the Qt-free core. Sources are listed per subsystem in the same order as src/.
add_executable(QtRocket_core_tests
    ThirdPartyTests.cpp
    util/CoordinateTests.cpp
    util/MathUtilTests.cpp
    util/QuaternionTests.cpp
    util/BugErrorTests.cpp
    util/Md5Tests.cpp
    util/ModIdTests.cpp
    util/SignalTests.cpp
    util/UuidTests.cpp
    util/Geometry2DTests.cpp
    util/InertiaTests.cpp
    util/LinearInterpolatorTests.cpp
    util/PinkNoiseTests.cpp
    util/PolyInterpolatorTests.cpp
    util/Rotation2DTests.cpp
    util/VersionTests.cpp
    file/GzipStreamTests.cpp
    file/ZipArchiveTests.cpp
)
target_include_directories(QtRocket_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# The third-party canary tests use the libraries directly; everything else goes through QtRocket::core.
target_link_libraries(QtRocket_core_tests PRIVATE QtRocket::core GTest::gtest_main pugixml::pugixml SQLite::SQLite3)
# Test data (fixtures, goldens) and the shipped data files are read from the source tree, so the tests run from
# any working directory.
target_compile_definitions(QtRocket_core_tests PRIVATE
    QTROCKET_TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data"
    QTROCKET_DATA_DIR="${PROJECT_SOURCE_DIR}/data")
QtRocket_configure_target(QtRocket_core_tests)

include(GoogleTest)
# PRE_TEST: list the tests when ctest runs, not at build time (the binary may be sanitized or cross-compiled).
gtest_discover_tests(QtRocket_core_tests DISCOVERY_MODE PRE_TEST)
