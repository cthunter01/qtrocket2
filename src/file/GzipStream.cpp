#include "QtRocket/file/GzipStream.h"

#include <mz.h>
#include <mz_strm.h>
#include <mz_strm_mem.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "MinizipHandle.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

namespace
{

constexpr std::size_t kMaxStreamSize =
    static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
constexpr std::int32_t kGrowSize  = 256 * 1024;
constexpr std::size_t  kChunkSize = std::size_t{64} * 1024;
// zlib window-bits conventions: 15 + 16 writes a gzip header, -15 reads deflate data without
// a header.
constexpr std::int64_t kWindowBitsGzip       = 15 + 16;
constexpr std::int64_t kWindowBitsRawDeflate = -15;

/// The header of a member without its optional fields: the magic number, the compression
/// method, the flags, and the time, the extra flags and the system, which are not looked at.
constexpr std::size_t kFixedHeaderSize = 10;
/// What follows the data of a member: its CRC-32 and its length.
constexpr std::size_t kTrailerSize       = 8;
constexpr unsigned    kCompressionMethod = 8;
// The flags GZIPInputStream looks at.
constexpr unsigned                   kFlagHeaderCrc = 2;   // FHCRC
constexpr unsigned                   kFlagExtra     = 4;   // FEXTRA
constexpr unsigned                   kFlagName      = 8;   // FNAME
constexpr unsigned                   kFlagComment   = 16;  // FCOMMENT
[[nodiscard]] std::unexpected<Error> endOfStream()
{
    return fail(ErrorCode::PARSE, std::string(kUnexpectedEndOfGzipStream));
}

/// A little-endian number of @p count bytes at @p offset of @p bytes.
[[nodiscard]] std::uint32_t littleEndian(std::span<const std::byte> bytes, std::size_t offset,
                                         std::size_t count) noexcept
{
    std::uint32_t value = 0;
    for (std::size_t i = count; i > 0; i--)
    {
        value = (value << 8U) | std::to_integer<std::uint32_t>(bytes[offset + i - 1]);
    }
    return value;
}

/// Moves @p length past a field of the header that ends with a zero byte (the name, the
/// comment); false when @p stream ends before the zero.
[[nodiscard]] bool skipZeroTerminated(std::span<const std::byte> stream,
                                      std::size_t&               length) noexcept
{
    while (length < stream.size())
    {
        if (stream[length++] == std::byte{0})
        {
            return true;
        }
    }
    return false;
}

/// GZIPInputStream.readHeader(): the length of the header of the member at the start of
/// @p stream, or why there is none (Java's exception; an EOFException, which has no message,
/// is kUnexpectedEndOfGzipStream).
[[nodiscard]] Result<std::size_t> headerLength(std::span<const std::byte> stream)
{
    // Check header magic
    if (stream.size() < 2)
    {
        return endOfStream();
    }
    if (!looksLikeGzip(stream))
    {
        return fail(ErrorCode::PARSE, "Not in GZIP format");
    }
    // Check compression method
    if (stream.size() < 3)
    {
        return endOfStream();
    }
    if (std::to_integer<unsigned>(stream[2]) != kCompressionMethod)
    {
        return fail(ErrorCode::PARSE, "Unsupported compression method");
    }
    // Read flags; skip MTIME, XFL, and OS fields
    if (stream.size() < kFixedHeaderSize)
    {
        return endOfStream();
    }
    const auto  flags  = std::to_integer<unsigned>(stream[3]);
    std::size_t length = kFixedHeaderSize;
    // Skip optional extra field
    if ((flags & kFlagExtra) != 0)
    {
        if (stream.size() - length < 2)
        {
            return endOfStream();
        }
        const std::size_t extra = littleEndian(stream, length, 2);
        length += 2;
        if (stream.size() - length < extra)
        {
            return endOfStream();
        }
        length += extra;
    }
    // Skip optional file name
    if ((flags & kFlagName) != 0 && !skipZeroTerminated(stream, length))
    {
        return endOfStream();
    }
    // Skip optional file comment
    if ((flags & kFlagComment) != 0 && !skipZeroTerminated(stream, length))
    {
        return endOfStream();
    }
    // Check optional header CRC
    if ((flags & kFlagHeaderCrc) != 0)
    {
        const std::uint32_t expected = Detail::crc32(0, stream.first(length)) & 0xFFFFU;
        if (stream.size() - length < 2)
        {
            return endOfStream();
        }
        if (littleEndian(stream, length, 2) != expected)
        {
            return fail(ErrorCode::PARSE, "Corrupt GZIP header");
        }
        length += 2;
    }
    return length;
}

/// What inflating the deflate data of a member gave.
struct InflatedMember
{
    /// The bytes of deflate data that were read, to the end of the deflate stream.
    std::size_t read{0};
    /// Why the data was not read to its end.
    std::optional<Error> failure;
};

[[nodiscard]] InflatedMember memberFailure(ErrorCode code, std::string message)
{
    return {.read = 0, .failure = fail(code, std::move(message)).error()};
}

/// Inflates the raw deflate stream at the start of @p data to its end and appends what it
/// gives to @p out, which may hold @p maxBytes bytes: more is a failure, met when the bytes
/// come and before the rest is read. After a failure @p out has what could be inflated before
/// it.
[[nodiscard]] InflatedMember inflateMember(std::span<const std::byte> data,
                                           std::vector<std::byte>& out, std::size_t maxBytes)
{
    if (data.empty())
    {
        // Java: InflaterInputStream.fill() finds nothing to read.
        return memberFailure(ErrorCode::PARSE, std::string(kUnexpectedEndOfZlibStream));
    }
    Detail::MemStream  source;
    Detail::ZlibStream inflater;
    if (!source.valid() || !inflater.valid())
    {
        return memberFailure(ErrorCode::UNKNOWN, "minizip: cannot create streams");
    }
    // The memory stream only reads from the buffer in this mode; minizip's API takes a non-const
    // pointer.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    mz_stream_mem_set_buffer(source.get(), const_cast<std::byte*>(data.data()),
                             static_cast<std::int32_t>(data.size()));
    if (!source.open(MZ_OPEN_MODE_READ))
    {
        return memberFailure(ErrorCode::UNKNOWN, "minizip: cannot open memory stream");
    }
    mz_stream_set_prop_int64(inflater.get(), MZ_STREAM_PROP_COMPRESS_WINDOW, kWindowBitsRawDeflate);
    mz_stream_set_base(inflater.get(), source.get());
    if (!inflater.open(MZ_OPEN_MODE_READ))
    {
        return memberFailure(ErrorCode::UNKNOWN, "minizip: cannot start decompression");
    }
    const std::size_t                 before = out.size();
    std::array<std::byte, kChunkSize> chunk{};
    while (true)
    {
        const std::int32_t n =
            mz_stream_read(inflater.get(), chunk.data(), static_cast<std::int32_t>(chunk.size()));
        if (n == 0)
        {
            break;
        }
        // A failing call has inflated what it could before it failed: those bytes are in the
        // chunk, and the stream's count of its output says how many they are.
        std::int64_t total = 0;
        mz_stream_get_prop_int64(inflater.get(), MZ_STREAM_PROP_TOTAL_OUT, &total);
        const std::size_t inflated =
            n > 0 ? static_cast<std::size_t>(n)
                  : std::min(static_cast<std::size_t>(std::max<std::int64_t>(
                                 total - static_cast<std::int64_t>(out.size() - before), 0)),
                             chunk.size());
        const std::span<const std::byte> got(chunk.data(), inflated);
        if (got.size() > maxBytes - out.size())
        {
            return memberFailure(ErrorCode::IO,
                                 std::format("Input exceeds maximum size of {} bytes", maxBytes));
        }
        out.insert(out.end(), got.begin(), got.end());
        if (n == MZ_BUF_ERROR)
        {
            // The data ends before the stream does (Java: InflaterInputStream.fill()).
            return memberFailure(ErrorCode::PARSE, std::string(kUnexpectedEndOfZlibStream));
        }
        if (n == MZ_DATA_ERROR)
        {
            return memberFailure(ErrorCode::PARSE, "invalid deflate data in GZIP stream");
        }
        if (n < 0)
        {
            return memberFailure(
                ErrorCode::PARSE,
                std::format("gzip: cannot inflate the stream (minizip error {})", n));
        }
    }
    std::int64_t read = 0;
    mz_stream_get_prop_int64(inflater.get(), MZ_STREAM_PROP_TOTAL_IN, &read);
    return {.read = static_cast<std::size_t>(std::max<std::int64_t>(read, 0)), .failure = {}};
}

}  // namespace

bool looksLikeGzip(std::span<const std::byte> bytes) noexcept
{
    return bytes.size() >= 2 && bytes[0] == std::byte{0x1f} && bytes[1] == std::byte{0x8b};
}

InflatedPrefix gzipInflatePrefix(std::span<const std::byte> compressed, std::size_t maxBytes)
{
    InflatedPrefix prefix;
    if (compressed.size() > kMaxStreamSize)
    {
        prefix.failure =
            fail(ErrorCode::UNSUPPORTED_FORMAT, "gzip stream larger than 2 GiB").error();
        return prefix;
    }
    const Result<std::size_t> firstHeader = headerLength(compressed);
    if (!firstHeader)
    {
        prefix.failure = firstHeader.error();
        return prefix;
    }
    // Where the deflate data of the member starts.
    std::size_t data = *firstHeader;
    while (true)
    {
        const std::size_t before = prefix.bytes.size();
        InflatedMember    member = inflateMember(compressed.subspan(data), prefix.bytes, maxBytes);
        if (member.failure.has_value())
        {
            prefix.failure = std::move(member.failure);
            return prefix;
        }
        if (!prefix.firstMemberEnd.has_value() && !prefix.bytes.empty())
        {
            prefix.firstMemberEnd = prefix.bytes.size();
        }

        // readTrailer()
        const std::size_t end = data + member.read;
        if (end > compressed.size() || compressed.size() - end < kTrailerSize)
        {
            prefix.failure          = endOfStream().error();
            prefix.failedBehindData = true;
            return prefix;
        }
        const std::span<const std::byte> written = std::span(prefix.bytes).subspan(before);
        // rfc1952; ISIZE is the input size modulo 2^32
        const auto length = static_cast<std::uint32_t>(written.size() & 0xFFFFFFFFU);
        if (littleEndian(compressed, end, 4) != Detail::crc32(0, written) ||
            littleEndian(compressed, end + 4, 4) != length)
        {
            prefix.failure = fail(ErrorCode::PARSE, std::string(kCorruptGzipTrailer)).error();
            prefix.failedBehindData = true;
            return prefix;
        }

        // "try concatenated case": the header of another member may follow.
        const std::size_t         afterTrailer = end + kTrailerSize;
        const Result<std::size_t> nextHeader   = headerLength(compressed.subspan(afterTrailer));
        if (!nextHeader)
        {
            // "ignore any malformed, do nothing"
            return prefix;
        }
        data = afterTrailer + *nextHeader;
    }
}

Result<std::vector<std::byte>> gzipInflate(std::span<const std::byte> compressed)
{
    InflatedPrefix prefix = gzipInflatePrefix(compressed, std::numeric_limits<std::size_t>::max());
    if (prefix.failure.has_value())
    {
        return std::unexpected(std::move(*prefix.failure));
    }
    return std::move(prefix.bytes);
}

Result<std::vector<std::byte>> gzipDeflate(std::span<const std::byte> data)
{
    if (data.size() > kMaxStreamSize)
    {
        return fail(ErrorCode::UNSUPPORTED_FORMAT, "data larger than 2 GiB");
    }
    Detail::MemStream  sink;
    Detail::ZlibStream deflater;
    if (!sink.valid() || !deflater.valid())
    {
        return fail(ErrorCode::UNKNOWN, "minizip: cannot create streams");
    }
    mz_stream_mem_set_grow_size(sink.get(), kGrowSize);
    if (!sink.open(MZ_OPEN_MODE_CREATE | MZ_OPEN_MODE_WRITE))
    {
        return fail(ErrorCode::UNKNOWN, "minizip: cannot open memory stream");
    }
    mz_stream_set_prop_int64(deflater.get(), MZ_STREAM_PROP_COMPRESS_WINDOW, kWindowBitsGzip);
    mz_stream_set_prop_int64(deflater.get(), MZ_STREAM_PROP_COMPRESS_LEVEL,
                             MZ_COMPRESS_LEVEL_DEFAULT);
    mz_stream_set_base(deflater.get(), sink.get());
    if (!deflater.open(MZ_OPEN_MODE_WRITE))
    {
        return fail(ErrorCode::UNKNOWN, "gzip: cannot start compression");
    }
    const auto size = static_cast<std::int32_t>(data.size());
    if (size > 0 && mz_stream_write(deflater.get(), data.data(), size) != size)
    {
        return fail(ErrorCode::UNKNOWN, "gzip: compression failed");
    }
    if (!deflater.close())
    {
        return fail(ErrorCode::UNKNOWN, "gzip: cannot finish the stream");
    }
    const void*  buffer = nullptr;
    std::int32_t length = 0;
    mz_stream_mem_get_buffer(sink.get(), &buffer);
    mz_stream_mem_get_buffer_length(sink.get(), &length);
    if (buffer == nullptr || length < 0)
    {
        return fail(ErrorCode::UNKNOWN, "gzip: no output was produced");
    }
    std::vector<std::byte> out(static_cast<std::size_t>(length));
    std::memcpy(out.data(), buffer, out.size());
    return out;
}

}  // namespace QtRocket
