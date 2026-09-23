#pragma once

#include <filesystem>

namespace QtRocket::Test
{

/// tests/data in the source tree: fixtures and golden files.
inline std::filesystem::path testDataDir()
{
    return std::filesystem::path{QTROCKET_TEST_DATA_DIR};
}

/// data/ in the source tree: the shipped motor database and example designs.
inline std::filesystem::path dataDir()
{
    return std::filesystem::path{QTROCKET_DATA_DIR};
}

}  // namespace QtRocket::Test
