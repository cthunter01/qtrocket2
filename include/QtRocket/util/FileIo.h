#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// Reads a whole file. Fails with ErrorCode::IO when it does not exist or cannot be read.
[[nodiscard]] Result<std::vector<std::byte>> readFile(const std::filesystem::path& path);

/// Reads a whole file as text (bytes are taken as-is; no newline or encoding conversion).
[[nodiscard]] Result<std::string> readTextFile(const std::filesystem::path& path);

/// Writes @p data to @p path, replacing the file. Fails with ErrorCode::IO on any error.
[[nodiscard]] Result<void> writeFile(const std::filesystem::path& path,
                                     std::span<const std::byte>   data);

/// Writes @p text to @p path, replacing the file.
[[nodiscard]] Result<void> writeTextFile(const std::filesystem::path& path, std::string_view text);

/// Reinterprets bytes as text without copying semantics surprises (a plain byte-for-byte copy).
[[nodiscard]] std::string bytesToString(std::span<const std::byte> bytes);

/// The bytes of a string, byte for byte.
[[nodiscard]] std::vector<std::byte> stringToBytes(std::string_view text);

}  // namespace QtRocket
