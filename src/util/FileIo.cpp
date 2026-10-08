#include "QtRocket/util/FileIo.h"

#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "QtRocket/util/Error.h"

namespace QtRocket
{

Result<std::vector<std::byte>> readFile(const std::filesystem::path& path)
{
    // Refused up front (the error_code overload never throws): libstdc++'s filebuf opens a
    // directory and then throws from underflow() (EISDIR), libc++'s reads it as empty and MSVC's
    // fails to open it.
    std::error_code directoryError;
    if (std::filesystem::is_directory(path, directoryError))
    {
        return fail(ErrorCode::IO,
                    std::format("cannot read '{}': is a directory", pathToUtf8(path)));
    }
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return fail(ErrorCode::IO, std::format("cannot open '{}' for reading", pathToUtf8(path)));
    }
    std::vector<std::byte> bytes;
    std::error_code        ec;
    const auto             size = std::filesystem::file_size(path, ec);
    if (!ec)
    {
        bytes.reserve(static_cast<std::size_t>(size));
    }
    // istreambuf_iterator yields chars; std::byte is the same width, so a plain transform copies
    // it. The iterator calls the filebuf directly, so a read error that libstdc++'s underflow()
    // throws (EIO, ...) reaches here instead of setting the stream state.
    try
    {
        for (std::istreambuf_iterator<char> it(in), end; it != end; ++it)
        {
            bytes.push_back(static_cast<std::byte>(*it));
        }
    }
    catch (const std::ios_base::failure&)
    {
        return fail(ErrorCode::IO, std::format("error while reading '{}'", pathToUtf8(path)));
    }
    if (in.bad())
    {
        return fail(ErrorCode::IO, std::format("error while reading '{}'", pathToUtf8(path)));
    }
    return bytes;
}

Result<std::string> readTextFile(const std::filesystem::path& path)
{
    return readFile(path).transform(
        [](const std::vector<std::byte>& bytes) { return bytesToString(bytes); });
}

Result<void> writeFile(const std::filesystem::path& path, std::span<const std::byte> data)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        return fail(ErrorCode::IO, std::format("cannot open '{}' for writing", pathToUtf8(path)));
    }
    for (const std::byte b : data)
    {
        out.put(static_cast<char>(b));
    }
    out.flush();
    if (!out)
    {
        return fail(ErrorCode::IO, std::format("error while writing '{}'", pathToUtf8(path)));
    }
    return {};
}

Result<void> writeTextFile(const std::filesystem::path& path, std::string_view text)
{
    return writeFile(path, stringToBytes(text));
}

std::string pathToUtf8(const std::filesystem::path& path)
{
    const std::u8string text = path.u8string();
    return {text.begin(), text.end()};
}

std::filesystem::path absolutePath(const std::filesystem::path& path)
{
    std::error_code             error;
    const std::filesystem::path resolved = path.empty() ? std::filesystem::current_path(error)
                                                        : std::filesystem::absolute(path, error);
    return error ? path : resolved;
}

std::string bytesToString(std::span<const std::byte> bytes)
{
    std::string text;
    text.reserve(bytes.size());
    for (const std::byte b : bytes)
    {
        text.push_back(static_cast<char>(b));
    }
    return text;
}

std::vector<std::byte> stringToBytes(std::string_view text)
{
    std::vector<std::byte> bytes;
    bytes.reserve(text.size());
    for (const char c : text)
    {
        bytes.push_back(static_cast<std::byte>(c));
    }
    return bytes;
}

}  // namespace QtRocket
