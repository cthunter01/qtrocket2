add_executable(QtRocket_tests greet_tests.cpp)
target_link_libraries(QtRocket_tests PRIVATE QtRocket::lib GTest::gtest_main)
QtRocket_configure_target(QtRocket_tests)

include(GoogleTest)
# PRE_TEST: list the tests when ctest runs, not at build time (the binary may be sanitized or cross-compiled).
gtest_discover_tests(QtRocket_tests DISCOVERY_MODE PRE_TEST)
