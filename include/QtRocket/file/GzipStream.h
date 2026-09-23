#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// True when @p bytes starts with the gzip magic number (0x1f 0x8b).
[[nodiscard]] bool looksLikeGzip(std::span<const std::byte> bytes) noexcept;

/// Decompresses a gzip (or zlib-wrapped) stream. Fails with ErrorCode::PARSE on corrupt or
/// truncated input.
[[nodiscard]] Result<std::vector<std::byte>> gzipInflate(std::span<const std::byte> compressed);

/// Compresses @p data as a gzip stream at the default compression level.
[[nodiscard]] Result<std::vector<std::byte>> gzipDeflate(std::span<const std::byte> data);

}  // namespace QtRocket
