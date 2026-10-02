#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// Reads a whole file. Fails with ErrorCode::IO when it does not exist, is a directory or cannot
/// be read. A directory is refused before it is opened, so that every platform reports it the same
/// way (libstdc++ opens one and fails while reading, libc++ reads it as empty, MSVC fails to open
/// it).
[[nodiscard]] Result<std::vector<std::byte>> readFile(const std::filesystem::path& path);

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

/// Reinterprets bytes as text without copying semantics surprises (a plain byte-for-byte copy).
[[nodiscard]] std::string bytesToString(std::span<const std::byte> bytes);

/// The bytes of a string, byte for byte.
[[nodiscard]] std::vector<std::byte> stringToBytes(std::string_view text);

}  // namespace QtRocket
