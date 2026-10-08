#pragma once

// A directory of its own for a test, removed with everything in it when the test ends, and a
// guard that makes a directory the current one for the time of a test.

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

/// Makes a directory the current directory of the process while it lives, and the directory
/// that was current before again when it ends. For a test of code that reads a file by a
/// relative name (a lookup table that a design names): what happens to lie in the directory the
/// tests are run from must not decide what the test sees. The tests of a process run one after
/// the other, so no other test sees the change.
///
/// Declare it after the TempDir it points into, so that it ends first: a directory that is the
/// current one of a process cannot be removed on every platform.
class CurrentDirectoryGuard
{
public:
    /// Makes @p directory, which is made when it does not exist, the current directory.
    explicit CurrentDirectoryGuard(const std::filesystem::path& directory)
      : m_previous(std::filesystem::current_path())
    {
        std::filesystem::create_directories(directory);
        std::filesystem::current_path(directory);
    }
    ~CurrentDirectoryGuard()
    {
        std::error_code ignored;
        std::filesystem::current_path(m_previous, ignored);
    }
    CurrentDirectoryGuard(const CurrentDirectoryGuard&)            = delete;
    CurrentDirectoryGuard& operator=(const CurrentDirectoryGuard&) = delete;
    CurrentDirectoryGuard(CurrentDirectoryGuard&&)                 = delete;
    CurrentDirectoryGuard& operator=(CurrentDirectoryGuard&&)      = delete;

private:
    std::filesystem::path m_previous;
};

}  // namespace QtRocket::Test
