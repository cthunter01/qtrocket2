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
// zlib window-bits conventions: 15 + 16 writes a gzip header, 15 + 32 accepts either a gzip or a
// zlib header.
constexpr std::int64_t kWindowBitsGzip       = 15 + 16;
constexpr std::int64_t kWindowBitsAutoDetect = 15 + 32;

}  // namespace

bool looksLikeGzip(std::span<const std::byte> bytes) noexcept
{
    return bytes.size() >= 2 && bytes[0] == std::byte{0x1f} && bytes[1] == std::byte{0x8b};
}

namespace
{

/// A stream that could not be started: nothing of it was read.
[[nodiscard]] InflatedPrefix notStarted(ErrorCode code, std::string message)
{
    return {.bytes = {}, .failure = fail(code, std::move(message)).error()};
}

}  // namespace

InflatedPrefix gzipInflatePrefix(std::span<const std::byte> compressed, std::size_t maxBytes)
{
    if (compressed.size() > kMaxStreamSize)
    {
        return notStarted(ErrorCode::UNSUPPORTED_FORMAT, "gzip stream larger than 2 GiB");
    }
    Detail::MemStream  source;
    Detail::ZlibStream inflater;
    if (!source.valid() || !inflater.valid())
    {
        return notStarted(ErrorCode::UNKNOWN, "minizip: cannot create streams");
    }
    // The memory stream only reads from the buffer in this mode; minizip's API takes a non-const
    // pointer.
    auto* data =
        const_cast<std::byte*>(compressed.data());  // NOLINT(cppcoreguidelines-pro-type-const-cast)
    mz_stream_mem_set_buffer(source.get(), data, static_cast<std::int32_t>(compressed.size()));
    if (!source.open(MZ_OPEN_MODE_READ))
    {
        return notStarted(ErrorCode::UNKNOWN, "minizip: cannot open memory stream");
    }
    mz_stream_set_prop_int64(inflater.get(), MZ_STREAM_PROP_COMPRESS_WINDOW, kWindowBitsAutoDetect);
    mz_stream_set_base(inflater.get(), source.get());
    if (!inflater.open(MZ_OPEN_MODE_READ))
    {
        return notStarted(ErrorCode::PARSE, "gzip: cannot start decompression");
    }
    InflatedPrefix                    prefix;
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
                                 total - static_cast<std::int64_t>(prefix.bytes.size()), 0)),
                             chunk.size());
        const std::span<const std::byte> got(chunk.data(), inflated);
        if (got.size() > maxBytes - prefix.bytes.size())
        {
            prefix.failure =
                fail(ErrorCode::IO, std::format("Input exceeds maximum size of {} bytes", maxBytes))
                    .error();
            return prefix;
        }
        prefix.bytes.insert(prefix.bytes.end(), got.begin(), got.end());
        if (n == MZ_BUF_ERROR)
        {
            // The data ends before the stream does (Java: InflaterInputStream.fill()).
            prefix.failure =
                fail(ErrorCode::PARSE, std::string(kUnexpectedEndOfZlibStream)).error();
            return prefix;
        }
        if (n < 0)
        {
            prefix.failure =
                fail(ErrorCode::PARSE,
                     std::format("gzip: corrupt or truncated stream (minizip error {})", n))
                    .error();
            return prefix;
        }
    }
    if (!inflater.close())
    {
        prefix.failure = fail(ErrorCode::PARSE, "gzip: corrupt or truncated stream").error();
    }
    return prefix;
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
