#include "QtRocket/util/FileIo.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <ios>
#include <iterator>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// readFile(): all of @p path, or its first @p maxBytes bytes at most when there is a limit.
[[nodiscard]] Result<std::vector<std::byte>> readWholeFile(const std::filesystem::path& path,
                                                           std::optional<std::size_t>   maxBytes)
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
    const std::uintmax_t   size = std::filesystem::file_size(path, ec);
    if (!ec)
    {
        // What the file says of its size is believed for a refusal, and for the reservation
        // only within the limit: a sparse file may say anything.
        if (maxBytes.has_value() && std::cmp_greater(size, *maxBytes))
        {
            return fail(ErrorCode::IO,
                        std::format("Input exceeds maximum size of {} bytes", *maxBytes));
        }
        bytes.reserve(static_cast<std::size_t>(size));
    }
    // istreambuf_iterator yields chars; std::byte is the same width, so a plain transform copies
    // it. The iterator calls the filebuf directly, so a read error that libstdc++'s underflow()
    // throws (EIO, ...) reaches here instead of setting the stream state.
    try
    {
        for (std::istreambuf_iterator<char> it(in), end; it != end; ++it)
        {
            // A source without a size, or with more than it said: a device, a pipe, a file that
            // is being written.
            if (maxBytes.has_value() && bytes.size() >= *maxBytes)
            {
                return fail(ErrorCode::IO,
                            std::format("Input exceeds maximum size of {} bytes", *maxBytes));
            }
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

/// The bytes writeFile() hands the stream at a time.
constexpr std::size_t kWritePiece = std::size_t{1024} * 1024;

/// How often replaceFile() draws a name for its temporary file that no file has yet.
constexpr int kTemporaryNameAttempts = 16;

[[nodiscard]] std::unexpected<Error> cannotOpenForWriting(const std::filesystem::path& path)
{
    return fail(ErrorCode::IO, std::format("cannot open '{}' for writing", pathToUtf8(path)));
}

/// Closes @p out, a stream that was written to @p path, and tells whether everything was
/// written: closing writes what the stream still holds.
[[nodiscard]] Result<void> closeAfterWriting(std::ofstream& out, const std::filesystem::path& path)
{
    out.close();
    if (!out)
    {
        return fail(ErrorCode::IO, std::format("error while writing '{}'", pathToUtf8(path)));
    }
    return {};
}

/// 64 bits for the name of a temporary file: from the platform's source of random numbers, or
/// from the clock where it has none (std::random_device may throw then).
[[nodiscard]] std::uint64_t temporaryNameBits()
{
    try
    {
        std::random_device  device;
        const std::uint64_t high = device();
        return (high << 32U) ^ device();
    }
    catch (const std::exception&)
    {
        return static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
    }
}

/// A new, empty file in @p directory (the current directory when it is empty) that no other
/// file had the name of (Java: File.createTempFile()).
[[nodiscard]] Result<std::filesystem::path> createTemporaryFile(
    const std::filesystem::path& directory)
{
    const std::filesystem::path  where = directory.empty() ? std::filesystem::path(".") : directory;
    const std::unexpected<Error> failure = fail(
        ErrorCode::IO, std::format("cannot create a temporary file in '{}'", pathToUtf8(where)));
    std::error_code error;
    if (!std::filesystem::is_directory(where, error))
    {
        return failure;
    }
    for (int attempt = 0; attempt < kTemporaryNameAttempts; ++attempt)
    {
        std::filesystem::path candidate =
            directory / std::format("QtRocket-{:016x}.tmp", temporaryNameBits());
        // The name is checked before it is taken (C++ has no exclusive open before C++23's
        // std::ios::noreplace, which not every standard library has): 64 random bits make a
        // file of that name appearing in between as good as impossible.
        if (std::filesystem::exists(candidate, error) || error)
        {
            continue;
        }
        std::ofstream out(candidate, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            return failure;
        }
        out.close();
        return candidate;
    }
    return failure;
}

/// replaceFile(): a temporary file in the directory of @p target, which @p write fills and which
/// is then renamed to @p target; removed again on any failure.
[[nodiscard]] Result<void> replaceThrough(
    const std::filesystem::path&                                     target,
    const std::function<Result<void>(const std::filesystem::path&)>& write)
{
    const Result<std::filesystem::path> temporary = createTemporaryFile(target.parent_path());
    if (!temporary.has_value())
    {
        return std::unexpected(temporary.error());
    }
    Result<void> replaced = write(*temporary);
    if (replaced.has_value())
    {
        std::error_code error;
        std::filesystem::rename(*temporary, target, error);
        if (!error)
        {
            return {};
        }
        replaced = fail(ErrorCode::IO, std::format("cannot replace '{}': {}", pathToUtf8(target),
                                                   error.message()));
    }
    std::error_code ignored;
    std::filesystem::remove(*temporary, ignored);
    return replaced;
}

}  // namespace

Result<std::vector<std::byte>> readFile(const std::filesystem::path& path)
{
    return readWholeFile(path, std::nullopt);
}

Result<std::vector<std::byte>> readFile(const std::filesystem::path& path, std::size_t maxBytes)
{
    return readWholeFile(path, maxBytes);
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
        return cannotOpenForWriting(path);
    }
    // A std::byte is no char that the stream takes: the bytes go through a buffer of chars, a
    // piece at a time.
    std::vector<char> buffer(std::min(data.size(), kWritePiece));
    for (std::span<const std::byte> rest = data; !rest.empty() && out;)
    {
        const std::size_t piece = std::min(rest.size(), kWritePiece);
        std::ranges::transform(rest.first(piece), buffer.begin(),
                               [](std::byte b) { return static_cast<char>(b); });
        out.write(buffer.data(), static_cast<std::streamsize>(piece));
        rest = rest.subspan(piece);
    }
    return closeAfterWriting(out, path);
}

Result<void> writeTextFile(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        return cannotOpenForWriting(path);
    }
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    return closeAfterWriting(out, path);
}

Result<void> replaceFile(const std::filesystem::path& target, std::span<const std::byte> data)
{
    return replaceThrough(target, [data](const std::filesystem::path& temporary) {
        return writeFile(temporary, data);
    });
}

Result<void> replaceFileWith(const std::filesystem::path&                           target,
                             const std::function<Result<std::vector<std::byte>>()>& contents)
{
    return replaceThrough(target,
                          [&contents](const std::filesystem::path& temporary) -> Result<void> {
                              const Result<std::vector<std::byte>> data = contents();
                              if (!data.has_value())
                              {
                                  return std::unexpected(data.error());
                              }
                              return writeFile(temporary, *data);
                          });
}

std::string pathToUtf8(const std::filesystem::path& path)
{
    const std::u8string text = path.u8string();
    return {text.begin(), text.end()};
}

std::filesystem::path pathFromUtf8(std::string_view text)
{
    // Through char8_t, so that the text is read as UTF-8 on every platform; made valid first,
    // because the conversion to the platform's path may refuse a malformed sequence.
    const std::string valid = Strings::toValidUtf8(text);
    return {std::u8string(valid.begin(), valid.end())};
}

std::filesystem::path absolutePath(const std::filesystem::path& path)
{
    std::error_code             error;
    const std::filesystem::path resolved = path.empty() ? std::filesystem::current_path(error)
                                                        : std::filesystem::absolute(path, error);
    return error ? path : resolved;
}

std::filesystem::path withoutRedundantSeparators(const std::filesystem::path& path)
{
    // The root first, its separators as one: a path of separators only ("///") can be a single
    // element to the iteration, spelled as it was written.
    std::filesystem::path normalized = path.root_path().lexically_normal();
    for (const std::filesystem::path& element : path.relative_path())
    {
        // The element after a separator at the end is empty.
        if (!element.empty())
        {
            normalized /= element;
        }
    }
    return normalized;
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
