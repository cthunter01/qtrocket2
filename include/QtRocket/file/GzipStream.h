#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// True when @p bytes starts with the gzip magic number (0x1f 0x8b).
[[nodiscard]] bool looksLikeGzip(std::span<const std::byte> bytes) noexcept;

/// Decompresses a gzip (or zlib-wrapped) stream. Fails with ErrorCode::PARSE on corrupt or
/// truncated input (see gzipInflatePrefix() for the messages).
[[nodiscard]] Result<std::vector<std::byte>> gzipInflate(std::span<const std::byte> compressed);

/// The message of the failure of a deflate stream whose data ends before the stream does, of
/// gzipInflatePrefix() and of ZipInputStream alike: the text of the EOFException that Java's
/// InflaterInputStream throws there. (Java's XML parser takes that exception for the end of
/// its input, which is why a loader asks for it.)
inline constexpr std::string_view kUnexpectedEndOfZlibStream =
    "Unexpected end of ZLIB input stream";

/// What gzipInflatePrefix() made of a stream that may be damaged or too long.
struct InflatedPrefix
{
    /// What the stream gave before it ended or failed.
    std::vector<std::byte> bytes;
    /// Why the stream gave no more, when it did not simply end.
    std::optional<Error> failure;
};

/// gzipInflate() for a reader that wants the start of the data also when the stream is damaged
/// further on, and that takes @p maxBytes bytes at most: the bytes are everything that could be
/// inflated before the stream ended, failed or went beyond the limit, as Java's reader of a
/// GZIPInputStream has the bytes that came before the exception (the loader of a design file
/// looks at the first bytes of the document before it reads the rest). The failure is
/// - ErrorCode::PARSE and kUnexpectedEndOfZlibStream for a stream whose data ends before the
///   stream does, the check sum and the length behind the data included;
/// - ErrorCode::PARSE and a text of QtRocket's own for data that is no gzip stream, or that is
///   one whose check sum or length at the end is wrong (the bytes are then all of the data);
/// - ErrorCode::IO and "Input exceeds maximum size of <maxBytes> bytes" (the text of
///   OpenRocket's FileUtils.readBytes()) for a stream that holds more: the inflating stops
///   there, and no more than @p maxBytes bytes are ever held.
/// Only the first member of a stream of several gzip members is read (Java's GZIPInputStream
/// reads them all, one after the other).
[[nodiscard]] InflatedPrefix gzipInflatePrefix(std::span<const std::byte> compressed,
                                               std::size_t                maxBytes);

/// Compresses @p data as a gzip stream at the default compression level.
[[nodiscard]] Result<std::vector<std::byte>> gzipDeflate(std::span<const std::byte> data);

}  // namespace QtRocket
