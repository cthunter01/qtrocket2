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

/// A temporary file of replaceFile(), removed when it goes out of scope (an exception included)
/// unless it was renamed to its target (release()).
class TemporaryFile
{
public:
    explicit TemporaryFile(std::filesystem::path path) noexcept : m_path(std::move(path)) { }
    ~TemporaryFile()
    {
        if (!m_path.empty())
        {
            std::error_code ignored;
            // A temporary file that took the permissions of a read-only target is read-only
            // itself, and Windows does not remove a read-only file.
            std::filesystem::permissions(m_path, std::filesystem::perms::owner_write,
                                         std::filesystem::perm_options::add, ignored);
            std::filesystem::remove(m_path, ignored);
        }
    }
    TemporaryFile(const TemporaryFile&)            = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;
    TemporaryFile(TemporaryFile&&)                 = delete;
    TemporaryFile& operator=(TemporaryFile&&)      = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return m_path; }

    /// The file is the target now: it stays.
    void release() noexcept { m_path.clear(); }

private:
    std::filesystem::path m_path;
};

/// The write permissions of a file, for anyone.
constexpr std::filesystem::perms kWritePermissions = std::filesystem::perms::owner_write |
                                                     std::filesystem::perms::group_write |
                                                     std::filesystem::perms::others_write;

/// Whether a rename cannot replace a file marked read-only: Windows's MoveFileEx, through which
/// std::filesystem::rename() replaces a file there, refuses one; rename() on Linux and macOS
/// replaces it as it is.
#ifdef _WIN32
constexpr bool kRenameRefusesReadOnlyTarget = true;
#else
constexpr bool kRenameRefusesReadOnlyTarget = false;
#endif

/// The file replaceThrough() is to replace.
struct ReplacedTarget
{
    /// The permissions (std::filesystem::perms::all of them) of the file the target names, a
    /// link followed, when it is a regular file that has permissions; none when there is no such
    /// file. The new file gets them (see replaceThrough()).
    std::optional<std::filesystem::perms> permissions;
    /// Whether the directory entry of the target is that file itself, not a link to it: the
    /// rename replaces the entry, so only a file's own read-only attribute can stand in its way.
    bool isFile{false};

    /// Whether the target names a file marked read-only: no write permission for anyone (the
    /// read-only attribute on Windows).
    [[nodiscard]] bool isReadOnly() const noexcept
    {
        return permissions.has_value() &&
               (*permissions & kWritePermissions) == std::filesystem::perms::none;
    }
};

/// What stands at @p target now (see ReplacedTarget).
[[nodiscard]] ReplacedTarget replacedTarget(const std::filesystem::path& target)
{
    std::error_code                    error;
    const std::filesystem::file_status existing = std::filesystem::status(target, error);
    if (error || !std::filesystem::is_regular_file(existing) ||
        existing.permissions() == std::filesystem::perms::unknown)
    {
        return {};
    }
    const std::filesystem::file_status entry = std::filesystem::symlink_status(target, error);
    return {.permissions = existing.permissions() & std::filesystem::perms::all,
            .isFile      = !error && std::filesystem::is_regular_file(entry)};
}

/// Renames @p temporary to @p target, which it replaces. Where a rename cannot replace a file
/// marked read-only (kRenameRefusesReadOnlyTarget), such a target (@p replaced) has its
/// read-only attribute cleared first and set again when the rename fails, so that a failure
/// leaves it as it was. Only a target that is the file itself is cleared, never through a link
/// (perm_options::nofollow): the rename replaces a link, not the file it points to, which must
/// stay as it is.
[[nodiscard]] Result<void> renameOver(const std::filesystem::path& temporary,
                                      const std::filesystem::path& target,
                                      const ReplacedTarget&        replaced)
{
    const auto cannotReplace = [&target](const std::error_code& error) {
        return fail(ErrorCode::IO,
                    std::format("cannot replace '{}': {}", pathToUtf8(target), error.message()));
    };
    const bool clearsReadOnly =
        kRenameRefusesReadOnlyTarget && replaced.isFile && replaced.isReadOnly();
    std::error_code error;
    if (clearsReadOnly)
    {
        std::filesystem::permissions(
            target, std::filesystem::perms::owner_write,
            std::filesystem::perm_options::add | std::filesystem::perm_options::nofollow, error);
        if (error)
        {
            return cannotReplace(error);
        }
    }
    std::filesystem::rename(temporary, target, error);
    if (error)
    {
        if (clearsReadOnly)
        {
            std::error_code ignored;
            std::filesystem::permissions(
                target, replaced.permissions.value_or(std::filesystem::perms::none),
                std::filesystem::perm_options::replace | std::filesystem::perm_options::nofollow,
                ignored);
        }
        return cannotReplace(error);
    }
    return {};
}

/// replaceFile(): a temporary file in the directory of @p target, which @p write fills and which
/// is then renamed to @p target; removed again on any failure. A target that is there passes its
/// permissions on to the file that replaces it, read-only included, with read permission for
/// its owner added (see renameOver() for a read-only target).
[[nodiscard]] Result<FileReplacement> replaceThrough(
    const std::filesystem::path&                                     target,
    const std::function<Result<void>(const std::filesystem::path&)>& write)
{
    const ReplacedTarget                replaced = replacedTarget(target);
    const Result<std::filesystem::path> created  = createTemporaryFile(target.parent_path());
    if (!created.has_value())
    {
        return std::unexpected(created.error());
    }
    TemporaryFile temporary(*created);
    if (const Result<void> written = write(temporary.path()); !written.has_value())
    {
        return std::unexpected(written.error());
    }
    if (replaced.permissions.has_value())
    {
        // The new file is the old one to whoever may read or write it (Java's new file has the
        // permissions a new file gets), but its owner can always read it: a file of no
        // permissions at all would otherwise be one that its owner cannot open after the save
        // (QtRocket's rule). Best effort: a file system without permissions keeps its own.
        std::error_code ignored;
        std::filesystem::permissions(
            temporary.path(), *replaced.permissions | std::filesystem::perms::owner_read, ignored);
    }
    if (const Result<void> renamed = renameOver(temporary.path(), target, replaced);
        !renamed.has_value())
    {
        return std::unexpected(renamed.error());
    }
    temporary.release();
    return FileReplacement{.wasReadOnly = replaced.isReadOnly()};
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

Result<FileReplacement> replaceFile(const std::filesystem::path& target,
                                    std::span<const std::byte>   data)
{
    return replaceThrough(target, [data](const std::filesystem::path& temporary) {
        return writeFile(temporary, data);
    });
}

Result<FileReplacement> replaceFileWith(
    const std::filesystem::path&                           target,
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
