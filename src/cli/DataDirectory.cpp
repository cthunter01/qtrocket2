#include "QtRocket/cli/DataDirectory.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/util/FileIo.h"

// The calls to the operating system, a branch for each platform (CLAUDE.md). The three macros
// exclude each other; a branch is an #ifdef of its own (not #elif, for the compilers that do not
// have C++23's #elifdef).
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>

#include <cstdio>

// windows.h first: shellapi.h needs its types. WIN32_LEAN_AND_MEAN and NOMINMAX come from
// src/cli/CMakeLists.txt.
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on
#endif
#ifdef __APPLE__
#include <cstdint>
#include <system_error>

#include <mach-o/dyld.h>
#endif
#ifdef __linux__
#include <system_error>
#endif

namespace QtRocket
{

#ifdef _WIN32
namespace
{

/// @p text, UTF-16 as Windows gives it, as UTF-8; an unpaired surrogate becomes U+FFFD.
[[nodiscard]] std::string toUtf8(std::wstring_view text)
{
    if (text.empty())
    {
        return {};
    }
    const int wideLength = static_cast<int>(text.size());
    const int length =
        WideCharToMultiByte(CP_UTF8, 0, text.data(), wideLength, nullptr, 0, nullptr, nullptr);
    if (length <= 0)
    {
        return {};
    }
    std::string utf8(static_cast<std::size_t>(length), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, text.data(), wideLength, utf8.data(), length, nullptr,
                            nullptr) != length)
    {
        return {};
    }
    return utf8;
}

}  // namespace
#endif

std::optional<std::filesystem::path> DataDirectory::executablePath()
{
#ifdef _WIN32
    // GetModuleFileNameW cuts a path that does not fit and then returns the size of the buffer;
    // a long path may have up to 32767 characters.
    std::wstring buffer(260, L'\0');
    while (buffer.size() <= 65536)
    {
        const DWORD length =
            GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
        {
            return std::nullopt;
        }
        if (length < buffer.size())
        {
            buffer.resize(length);
            return std::filesystem::path(buffer);
        }
        buffer.resize(buffer.size() * 2);
    }
    return std::nullopt;
#endif
#ifdef __APPLE__
    // _NSGetExecutablePath returns -1 for a buffer that is too small and sets the size it needs.
    std::uint32_t size = 1024;
    std::string   buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0)
    {
        buffer.assign(size, '\0');
        if (_NSGetExecutablePath(buffer.data(), &size) != 0)
        {
            return std::nullopt;
        }
    }
    if (const std::size_t end = buffer.find('\0'); end != std::string::npos)
    {
        buffer.resize(end);
    }
    // The path may go through links (a link to the program in /usr/local/bin): the data lies
    // beside the program itself.
    std::error_code       error;
    std::filesystem::path path(buffer);
    std::filesystem::path resolved = std::filesystem::canonical(path, error);
    if (error)
    {
        return path;
    }
    return resolved;
#endif
#ifdef __linux__
    std::error_code       error;
    std::filesystem::path path = std::filesystem::read_symlink("/proc/self/exe", error);
    if (error)
    {
        return std::nullopt;
    }
    return path;
#endif
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__linux__)
    return std::nullopt;
#endif
}

std::filesystem::path DataDirectory::forExecutable(const std::filesystem::path& executable)
{
    return (executable.parent_path() / ".." / "share" / "QtRocket").lexically_normal();
}

std::filesystem::path DataDirectory::dataDirectory(std::string_view argv0)
{
    if (const std::optional<std::filesystem::path> executable = executablePath();
        executable.has_value())
    {
        return forExecutable(*executable);
    }
    return forExecutable(absolutePath(pathFromUtf8(argv0)));
}

std::vector<std::string> DataDirectory::commandLineArguments(int argc, char** argv)
{
#ifdef _WIN32
    int     count = 0;
    LPWSTR* wide  = CommandLineToArgvW(GetCommandLineW(), &count);
    if (wide != nullptr)
    {
        const std::span<LPWSTR>  all(wide, static_cast<std::size_t>(count > 0 ? count : 0));
        std::vector<std::string> arguments;
        arguments.reserve(all.size());
        for (std::size_t i = 1; i < all.size(); i++)
        {
            arguments.push_back(toUtf8(all[i]));
        }
        LocalFree(wide);
        return arguments;
    }
#endif
    const std::span<char*>   all(argv, static_cast<std::size_t>(argc > 0 ? argc : 0));
    std::vector<std::string> arguments;
    arguments.reserve(all.size());
    for (std::size_t i = 1; i < all.size(); i++)
    {
        arguments.emplace_back(all[i]);
    }
    return arguments;
}

void DataDirectory::useBinaryStandardStreams()
{
#ifdef _WIN32
    static_cast<void>(_setmode(_fileno(stdout), _O_BINARY));
    static_cast<void>(_setmode(_fileno(stderr), _O_BINARY));
#endif
}

}  // namespace QtRocket
