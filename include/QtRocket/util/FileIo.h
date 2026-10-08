#pragma once

#include <cstddef>
#include <filesystem>
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

/// Writes @p data to @p path, replacing the file. Fails with ErrorCode::IO on any error.
[[nodiscard]] Result<void> writeFile(const std::filesystem::path& path,
                                     std::span<const std::byte>   data);

/// Writes @p text to @p path, replacing the file.
[[nodiscard]] Result<void> writeTextFile(const std::filesystem::path& path, std::string_view text);

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
/// and no separator at the end (a root stays what it is), so "a//b/" is "a/b". Nothing else
/// changes: "." and ".." stay. The elements are joined with the platform's separator.
[[nodiscard]] std::filesystem::path withoutRedundantSeparators(const std::filesystem::path& path);

/// Reinterprets bytes as text without copying semantics surprises (a plain byte-for-byte copy).
[[nodiscard]] std::string bytesToString(std::span<const std::byte> bytes);

/// The bytes of a string, byte for byte.
[[nodiscard]] std::vector<std::byte> stringToBytes(std::string_view text);

}  // namespace QtRocket
