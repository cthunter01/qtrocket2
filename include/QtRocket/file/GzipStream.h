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

/// Decompresses a gzip stream as gzipInflatePrefix() reads one: the data of all its members.
/// Fails with ErrorCode::PARSE on corrupt or truncated input (see there for the messages), a
/// stream that lacks a part of the check sum and length at its end included.
[[nodiscard]] Result<std::vector<std::byte>> gzipInflate(std::span<const std::byte> compressed);

/// The message of the failure of a deflate stream whose data ends before the stream does, of
/// gzipInflatePrefix() and of ZipInputStream alike: the text of the EOFException that Java's
/// InflaterInputStream throws there. (Java's XML parser takes that exception for the end of
/// its input, which is why a loader asks for it.)
inline constexpr std::string_view kUnexpectedEndOfZlibStream =
    "Unexpected end of ZLIB input stream";

/// The message of the failure of a gzip stream that ends inside the header of its first member
/// or inside the check sum and length behind the data of a member. Java's EOFException has no
/// message there; to its XML parser it is the end of the input all the same.
inline constexpr std::string_view kUnexpectedEndOfGzipStream = "Unexpected end of GZIP data";

/// The message of the failure of a gzip stream whose check sum or length behind the data of a
/// member is not that of the data (Java's ZipException).
inline constexpr std::string_view kCorruptGzipTrailer = "Corrupt GZIP trailer";

/// What gzipInflatePrefix() made of a stream that may be damaged or too long.
struct InflatedPrefix
{
    /// What the stream gave before it ended or failed: the data of its members, one after the
    /// other.
    std::vector<std::byte> bytes;
    /// Why the stream gave no more, when it did not simply end.
    std::optional<Error> failure;
    /// Whether the failure was met behind the data of a member, in the check sum and the
    /// length that follow it, and not in a header or in the data itself. All of that member's
    /// data is among the bytes then.
    bool failedBehindData{false};
    /// How many of the bytes had come when the data of the first member that gave any had
    /// ended; none when the stream failed before that, or gave nothing. It is bytes.size()
    /// for a whole stream of one member. (A reader of Java's GZIPInputStream that asks for
    /// more gets no more than these bytes at once: the stream says that nothing is available
    /// when the data of a member has ended.)
    std::optional<std::size_t> firstMemberEnd;
};

/// gzipInflate() for a reader that wants the start of the data also when the stream is damaged
/// further on, and that takes @p maxBytes bytes at most: the bytes are everything that could be
/// inflated before the stream ended, failed or went beyond the limit, as Java's reader of a
/// GZIPInputStream has the bytes that came before the exception (the loader of a design file
/// looks at the first bytes of the document before it reads the rest).
///
/// The stream is read as java.util.zip.GZIPInputStream reads it (the JDK 17 of the probes,
/// 17.0.20):
/// - A member is a header, deflate data, and the CRC-32 and the length of what the data
///   inflates to. The header is the ten bytes 1f 8b 08 <flags> and six bytes that are not
///   looked at (the time, the extra flags, the system), then, as the flags say, an extra field
///   with its length (FEXTRA, 4), a name and a comment that each end with a zero byte (FNAME,
///   8; FCOMMENT, 16) and the lower half of the CRC-32 of the header so far (FHCRC, 2), which
///   is checked. The other flags, the reserved ones included, are not looked at.
/// - Behind a member another one may follow, whose data is appended, and so on. What follows
///   a member and is no header of a member (other data, a header that is cut off or whose
///   check sum is wrong, another compression method) ends the stream there without a failure.
///   (GZIPInputStream once looked for another member only when its stream had more bytes
///   available or more than 26 were left over in its inflater, so that up to 18 bytes behind
///   a member could go unread; the JDK of the probes always looks, and so does this.)
///
/// The failure is
/// - ErrorCode::PARSE with Java's message "Unsupported compression method" or "Corrupt GZIP
///   header", or kUnexpectedEndOfGzipStream, for the header of the first member ("Not in GZIP
///   format" for data that does not start with 1f 8b): there are no bytes;
/// - ErrorCode::PARSE and kUnexpectedEndOfZlibStream for a member whose data ends before its
///   deflate stream does;
/// - ErrorCode::PARSE and "invalid deflate data in GZIP stream" for data that is no deflate
///   stream (Java reports zlib's own message, such as "invalid block type");
/// - ErrorCode::PARSE with kUnexpectedEndOfGzipStream for a stream that ends inside the eight
///   bytes behind the data of a member, or kCorruptGzipTrailer when their check sum or length
///   is wrong (failedBehindData; the bytes hold all of the member's data);
/// - ErrorCode::IO and "Input exceeds maximum size of <maxBytes> bytes" (the text of
///   OpenRocket's FileUtils.readBytes()) for a stream that holds more: the inflating stops
///   there, and no more than @p maxBytes bytes are ever held.
[[nodiscard]] InflatedPrefix gzipInflatePrefix(std::span<const std::byte> compressed,
                                               std::size_t                maxBytes);

/// Compresses @p data as a gzip stream at the default compression level.
[[nodiscard]] Result<std::vector<std::byte>> gzipDeflate(std::span<const std::byte> data);

}  // namespace QtRocket
