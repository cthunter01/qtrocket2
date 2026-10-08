#pragma once

// A directory of its own for a test, removed with everything in it when the test ends.

#include <algorithm>
#include <filesystem>
#include <format>
#include <random>
#include <string>
#include <string_view>
#include <system_error>

#include <gtest/gtest.h>

#include "QtRocket/util/FileIo.h"

namespace QtRocket::Test
{

/// A new directory under the system's temporary directory, named after the running test.
class TempDir
{
public:
    TempDir()
    {
        const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
        std::string name = std::format("qtrocket_{}_{}_{}", info->test_suite_name(), info->name(),
                                       std::random_device{}());
        // A parameterised test has '/' in the names of its suite and of the test. They must not
        // become directories of their own, which would stay behind when this one is removed.
        std::ranges::replace(name, '/', '_');
        m_path = std::filesystem::temp_directory_path() / name;
        std::filesystem::create_directories(m_path);
    }
    ~TempDir()
    {
        std::error_code ignored;
        std::filesystem::remove_all(m_path, ignored);
    }
    TempDir(const TempDir&)            = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&)                 = delete;
    TempDir& operator=(TempDir&&)      = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return m_path; }

    /// @p name (a relative path) under the directory.
    [[nodiscard]] std::filesystem::path resolve(const std::filesystem::path& name) const
    {
        return m_path / name;
    }

    /// Writes @p text to @p name under the directory, creating directories as needed.
    [[nodiscard]] std::filesystem::path write(const std::filesystem::path& name,
                                              std::string_view             text) const
    {
        const std::filesystem::path file = m_path / name;
        std::filesystem::create_directories(file.parent_path());
        EXPECT_TRUE(QtRocket::writeTextFile(file, text));
        return file;
    }

    /// Copies @p source to @p target under the directory, creating directories as needed.
    [[nodiscard]] std::filesystem::path copy(const std::filesystem::path& source,
                                             const std::filesystem::path& target) const
    {
        const std::filesystem::path file = m_path / target;
        std::filesystem::create_directories(file.parent_path());
        std::filesystem::copy_file(source, file);
        return file;
    }

private:
    std::filesystem::path m_path;
};

}  // namespace QtRocket::Test
