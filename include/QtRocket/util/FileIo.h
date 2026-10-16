#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// Reads a whole file. Fails with ErrorCode::IO when it does not exist, is a directory or cannot
/// be read. A directory is refused before it is opened, so that every platform reports it the same
/// way (libstdc++ opens one and fails while reading, libc++ reads it as empty, MSVC fails to open
/// it).
[[nodiscard]] Result<std::vector<std::byte>> readFile(const std::filesystem::path& path);

/// Reads a whole file that may hold @p maxBytes bytes at most, for a file whose name or contents
/// come from untrusted input (Java: FileUtils.readBytes(InputStream, int)). Fails as
/// readFile(path) does, and with ErrorCode::IO and "Input exceeds maximum size of <maxBytes>
/// bytes" for a larger one: a file that says it is larger is refused before anything is read
/// or reserved, and whatever gives no size or gives more than it said (a device, a pipe, a file
/// that grows) is given up after @p maxBytes + 1 bytes. No more than @p maxBytes bytes are ever
/// held.
[[nodiscard]] Result<std::vector<std::byte>> readFile(const std::filesystem::path& path,
                                                      std::size_t                  maxBytes);

/// Reads a whole file as text (bytes are taken as-is; no newline or encoding conversion).
[[nodiscard]] Result<std::string> readTextFile(const std::filesystem::path& path);

/// Writes @p data to @p path, replacing the file, in binary mode (no newline conversion). Fails
/// with ErrorCode::IO on any error; a file that was opened is then left as far as it was written.
[[nodiscard]] Result<void> writeFile(const std::filesystem::path& path,
                                     std::span<const std::byte>   data);

/// Writes @p text to @p path, replacing the file, byte for byte as writeFile() does.
[[nodiscard]] Result<void> writeTextFile(const std::filesystem::path& path, std::string_view text);

/// What replaceFile() and replaceFileWith() found where they wrote.
struct FileReplacement
{
    /// Whether the file that was replaced was marked read-only: no write permission for anyone,
    /// which is the read-only attribute on Windows. It was replaced all the same, as OpenRocket
    /// replaces it, and the new file is read-only too. False when there was no such file.
    bool wasReadOnly{false};
};

/// Replaces the file @p target with @p data so that it is never left half written (OpenRocket's
/// GeneralRocketSaver.save(File, ...)): the data go to a new temporary file in the directory of
/// @p target, "QtRocket-<16 hexadecimal digits>.tmp", which is then renamed to @p target
/// (std::filesystem::rename(), which replaces an existing file in one step). A failure, of the
/// temporary file or of the rename, removes the temporary file and leaves @p target as it was
/// (or missing, when it was missing): ErrorCode::IO, "cannot create a temporary file in
/// '<directory>'" for a directory that does not exist or cannot be written to, writeFile()'s
/// failures and "cannot replace '<target>': <reason>". A target that is a directory, or that is
/// open in another program on Windows, is not replaced.
///
/// A file that is replaced passes its permissions (std::filesystem::perms::all of them) on to
/// the new one, as far as the file system keeps them. A file marked read-only (no write
/// permission for anyone, which is the read-only attribute on Windows) is replaced as any other,
/// so the new file is read-only too, and the result says that it was read-only
/// (FileReplacement::wasReadOnly), for the caller to tell the user. On Linux and macOS the rename
/// replaces such a file as it is. On Windows a rename cannot replace a read-only file, so its
/// read-only attribute is cleared just before the rename and put back when the rename fails,
/// which leaves the file as it was.
///
/// Deviations from Java: Java moves an existing target aside to "<target>-bak" first and then
/// renames the temporary file, putting the target back when that fails; a rename that replaces
/// the target needs no such step. Java's temporary file is "ORSave<digits>.tmp". Java gives the
/// new file the permissions of a new file, so a read-only file it replaces is writable after
/// the save; here the new file keeps the permissions of the old one, read-only included, on
/// every platform.
[[nodiscard]] Result<FileReplacement> replaceFile(const std::filesystem::path& target,
                                                  std::span<const std::byte>   data);

/// replaceFile() with the data that @p contents gives, which is asked only after the temporary
/// file was created (Java creates it before it writes the design), so that a directory that
/// cannot take the file fails before @p contents runs. A failure of @p contents is passed on;
/// the temporary file is removed and @p target left as it was.
[[nodiscard]] Result<FileReplacement> replaceFileWith(
    const std::filesystem::path&                           target,
    const std::function<Result<std::vector<std::byte>>()>& contents);

/// @p path as UTF-8 text, for messages and for APIs that take UTF-8 file names: path::string()
/// goes through the Windows ANSI code page, which garbles or rejects (throws for) the characters
/// outside it.
[[nodiscard]] std::string pathToUtf8(const std::filesystem::path& path);

/// The path the UTF-8 text @p text names, the opposite of pathToUtf8(): the text is read as
/// UTF-8 on every platform (a path made from a plain std::string goes through the Windows ANSI
/// code page). A sequence that is not UTF-8 is read as U+FFFD (Strings::toValidUtf8()), so that
/// no text, such as a file name out of a design file, can make the conversion throw.
[[nodiscard]] std::filesystem::path pathFromUtf8(std::string_view text);

/// Java's File.getAbsolutePath(): @p path resolved against the current directory, as
/// std::filesystem::absolute() does it (no link is followed and the file need not exist; on POSIX
/// "." and ".." stay, as in Java, while Windows resolves them), and the current directory itself
/// for an empty path, which std::filesystem::absolute() refuses on some platforms. @p path as it
/// is when the current directory cannot be asked.
[[nodiscard]] std::filesystem::path absolutePath(const std::filesystem::path& path);

/// @p path as Java's File and Path spell the same text: a run of separators as one separator
/// and no separator at the end but the one of a root, so "a//b/" is "a/b" and "///" is "/".
/// Nothing else changes: "." and ".." stay. The elements are joined with the platform's
/// separator.
[[nodiscard]] std::filesystem::path withoutRedundantSeparators(const std::filesystem::path& path);

/// Reinterprets bytes as text without copying semantics surprises (a plain byte-for-byte copy).
[[nodiscard]] std::string bytesToString(std::span<const std::byte> bytes);

/// The bytes of a string, byte for byte.
[[nodiscard]] std::vector<std::byte> stringToBytes(std::string_view text);

}  // namespace QtRocket
