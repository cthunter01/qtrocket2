#include "QtRocket/file/ZipInputStream.h"

#include <mz.h>
#include <mz_crypt.h>
#include <mz_strm.h>
#include <mz_strm_mem.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "MinizipHandle.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

namespace
{

constexpr std::uint32_t    kLocalHeaderSignature = 0x04034b50;
constexpr std::uint32_t    kDescriptorSignature  = 0x08074b50;
constexpr std::size_t      kLocalHeaderSize      = 30;
constexpr std::size_t      kDescriptorSize       = 16;
constexpr std::size_t      kZip64DescriptorSize  = 24;
constexpr std::uint16_t    kStored               = 0;
constexpr std::uint16_t    kDeflated             = 8;
constexpr std::uint16_t    kFlagEncrypted        = 0x1;
constexpr std::uint16_t    kFlagDescriptor       = 0x8;
constexpr std::int64_t     kZip64Magic           = 0xFFFFFFFF;
constexpr std::uint16_t    kZip64ExtraId         = 0x0001;
constexpr std::int64_t     kRawDeflateWindowBits = -15;
constexpr std::size_t      kChunkSize            = std::size_t{64} * 1024;
constexpr std::size_t      kMaxWindow            = std::numeric_limits<std::int32_t>::max();
constexpr std::string_view kUnexpectedEnd        = "Unexpected end of ZIP data";
/// No limit on the contents of an entry.
constexpr std::size_t kNoLimit = std::numeric_limits<std::size_t>::max();

/// FileUtils.readBytes()'s failure for contents beyond @p maxBytes.
[[nodiscard]] std::unexpected<Error> exceedsLimit(std::size_t maxBytes)
{
    return fail(ErrorCode::IO, std::format("Input exceeds maximum size of {} bytes", maxBytes));
}

[[nodiscard]] std::uint64_t littleEndian(std::span<const std::byte> bytes, std::size_t offset,
                                         std::size_t count) noexcept
{
    std::uint64_t value = 0;
    for (std::size_t i = count; i > 0; i--)
    {
        value = (value << 8U) | std::to_integer<std::uint64_t>(bytes[offset + i - 1]);
    }
    return value;
}

[[nodiscard]] std::uint16_t get16(std::span<const std::byte> bytes, std::size_t offset) noexcept
{
    return static_cast<std::uint16_t>(littleEndian(bytes, offset, 2));
}

[[nodiscard]] std::uint32_t get32(std::span<const std::byte> bytes, std::size_t offset) noexcept
{
    return static_cast<std::uint32_t>(littleEndian(bytes, offset, 4));
}

/// A 64-bit field as Java's get64() reads it, into a signed long.
[[nodiscard]] std::int64_t get64(std::span<const std::byte> bytes, std::size_t offset) noexcept
{
    return static_cast<std::int64_t>(littleEndian(bytes, offset, 8));
}

[[nodiscard]] std::uint32_t crc32(std::uint32_t crc, std::span<const std::byte> bytes)
{
    // Pieces of at most 2 GiB, as minizip counts in int32.
    while (!bytes.empty())
    {
        const std::size_t piece = std::min(bytes.size(), kMaxWindow);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        const auto* data = reinterpret_cast<const std::uint8_t*>(bytes.data());
        crc              = mz_crypt_crc32_update(crc, data, static_cast<std::int32_t>(piece));
        bytes            = bytes.subspan(piece);
    }
    return crc;
}

[[nodiscard]] std::string crcMismatch(std::uint32_t expected, std::uint32_t actual)
{
    return std::format("invalid entry CRC (expected 0x{:x} but got 0x{:x})", expected, actual);
}

/// A malformed UTF-8 sequence as String.decodeUTF8_UTF16() reports it when it may not replace
/// it (throwMalformed(offset, length)).
struct Malformed
{
    std::size_t offset;
    std::size_t length;
};

[[nodiscard]] bool isContinuation(unsigned b) noexcept
{
    return (b & 0xC0U) == 0x80U;
}

// The checks of decodeUTF8_UTF16() for a sequence whose lead byte @p bytes[0] was read, @p end
// being the end of the name; each returns the malformed sequence, or none and moves @p next past
// the well-formed one.

[[nodiscard]] std::optional<Malformed> twoByteSequence(std::span<const std::byte> bytes,
                                                       std::size_t&               next) noexcept
{
    if (next >= bytes.size())
    {
        return Malformed{.offset = next, .length = 1};
    }
    if (!isContinuation(std::to_integer<unsigned>(bytes[next++])))
    {
        return Malformed{.offset = next - 1, .length = 1};
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<Malformed> threeByteSequence(std::span<const std::byte> bytes,
                                                         std::size_t&               next) noexcept
{
    const auto b1 = std::to_integer<unsigned>(bytes[next - 1]);
    if (next + 1 < bytes.size())
    {
        const auto b2        = std::to_integer<unsigned>(bytes[next++]);
        const auto b3        = std::to_integer<unsigned>(bytes[next++]);
        const bool overlong  = b1 == 0xE0 && (b2 & 0xE0U) == 0x80U;
        const bool surrogate = b1 == 0xED && b2 >= 0xA0;
        if (overlong || surrogate || !isContinuation(b2) || !isContinuation(b3))
        {
            return Malformed{.offset = next - 3, .length = 3};
        }
        return std::nullopt;
    }
    if (next < bytes.size())
    {
        const auto b2 = std::to_integer<unsigned>(bytes[next]);
        if ((b1 == 0xE0 && (b2 & 0xE0U) == 0x80U) || !isContinuation(b2))
        {
            return Malformed{.offset = next - 1, .length = 2};
        }
    }
    return Malformed{.offset = next, .length = 1};
}

[[nodiscard]] std::optional<Malformed> fourByteSequence(std::span<const std::byte> bytes,
                                                        std::size_t&               next) noexcept
{
    if (next + 2 >= bytes.size())
    {
        return Malformed{.offset = next - 1, .length = 1};
    }
    const auto     b1 = std::to_integer<unsigned>(bytes[next - 1]);
    const auto     b2 = std::to_integer<unsigned>(bytes[next++]);
    const auto     b3 = std::to_integer<unsigned>(bytes[next++]);
    const auto     b4 = std::to_integer<unsigned>(bytes[next++]);
    const unsigned uc =
        ((b1 & 0x07U) << 18U) | ((b2 & 0x3FU) << 12U) | ((b3 & 0x3FU) << 6U) | (b4 & 0x3FU);
    if (!isContinuation(b2) || !isContinuation(b3) || !isContinuation(b4) || uc < 0x10000 ||
        uc > 0x10FFFF)
    {
        return Malformed{.offset = next - 4, .length = 4};
    }
    return std::nullopt;
}

/// The first malformed sequence of the name @p bytes as Java's strict UTF-8 decoding reports it.
[[nodiscard]] std::optional<Malformed> firstMalformed(std::span<const std::byte> bytes) noexcept
{
    std::size_t next = 0;
    while (next < bytes.size())
    {
        const auto               b1 = std::to_integer<unsigned>(bytes[next++]);
        std::optional<Malformed> malformed;
        if (b1 < 0x80)
        {
            continue;
        }
        if (b1 >= 0xC2 && b1 <= 0xDF)
        {
            malformed = twoByteSequence(bytes, next);
        }
        else if (b1 >= 0xE0 && b1 <= 0xEF)
        {
            malformed = threeByteSequence(bytes, next);
        }
        else if (b1 >= 0xF0 && b1 <= 0xF7)
        {
            malformed = fourByteSequence(bytes, next);
        }
        else
        {
            malformed = Malformed{.offset = next - 1, .length = 1};
        }
        if (malformed.has_value())
        {
            return malformed;
        }
    }
    return std::nullopt;
}

/// ZipEntry.setExtra0() for a local header whose sizes are ZIP64's: its ZIP64 field gives both.
void readZip64Sizes(std::span<const std::byte> extra, std::int64_t& size,
                    std::int64_t& compressedSize) noexcept
{
    std::size_t offset = 0;
    while (offset + 4 < extra.size())
    {
        const std::uint16_t tag    = get16(extra, offset);
        const std::size_t   length = get16(extra, offset + 2);
        offset += 4;
        if (offset + length > extra.size())
        {
            return;  // invalid data
        }
        if (tag == kZip64ExtraId && length >= 16)
        {
            size           = get64(extra, offset);
            compressedSize = get64(extra, offset + 8);
        }
        offset += length;
    }
}

/// What inflating an entry gave: its CRC, the bytes written and the compressed bytes read.
struct Inflated
{
    std::uint32_t crc{0};
    std::int64_t  written{0};
    std::int64_t  read{0};
};

/// Inflates the raw deflate stream at the start of @p data to its end, into @p contents when not
/// null (Java's InflaterInputStream), which may take @p limit bytes: more fails as
/// exceedsLimit(), when the bytes are met and before the rest is read.
[[nodiscard]] Result<Inflated> inflate(std::span<const std::byte> data,
                                       std::vector<std::byte>* contents, std::size_t limit)
{
    Detail::MemStream  source;
    Detail::ZlibStream inflater;
    if (!source.valid() || !inflater.valid())
    {
        return fail(ErrorCode::UNKNOWN, "minizip: cannot create streams");
    }
    // The memory stream only reads from the buffer; minizip's API takes a non-const pointer.
    // Deviation: deflate data beyond 2 GiB is not read (minizip counts in int32).
    const std::size_t window = std::min(data.size(), kMaxWindow);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    mz_stream_mem_set_buffer(source.get(), const_cast<std::byte*>(data.data()),
                             static_cast<std::int32_t>(window));
    if (!source.open(MZ_OPEN_MODE_READ))
    {
        return fail(ErrorCode::UNKNOWN, "minizip: cannot open memory stream");
    }
    mz_stream_set_prop_int64(inflater.get(), MZ_STREAM_PROP_COMPRESS_WINDOW, kRawDeflateWindowBits);
    mz_stream_set_base(inflater.get(), source.get());
    if (!inflater.open(MZ_OPEN_MODE_READ))
    {
        return fail(ErrorCode::UNKNOWN, "minizip: cannot start decompression");
    }

    Inflated                          inflated;
    std::array<std::byte, kChunkSize> chunk{};
    std::int32_t                      n = 0;
    while ((n = mz_stream_read(inflater.get(), chunk.data(),
                               static_cast<std::int32_t>(chunk.size()))) > 0)
    {
        const std::span<const std::byte> piece(chunk.data(), static_cast<std::size_t>(n));
        inflated.crc = crc32(inflated.crc, piece);
        inflated.written += n;
        if (contents != nullptr)
        {
            if (piece.size() > limit - contents->size())
            {
                return exceedsLimit(limit);
            }
            contents->insert(contents->end(), piece.begin(), piece.end());
        }
    }
    if (n == MZ_BUF_ERROR)
    {
        // the deflate data ends before its last block: InflaterInputStream.fill()
        return fail(ErrorCode::PARSE, "Unexpected end of ZLIB input stream");
    }
    if (n < 0)
    {
        return fail(ErrorCode::PARSE, "invalid deflate data in ZIP entry");
    }
    mz_stream_get_prop_int64(inflater.get(), MZ_STREAM_PROP_TOTAL_IN, &inflated.read);
    return inflated;
}

/// A deflated entry's CRC and sizes as its header or data descriptor gives them.
struct Expected
{
    std::uint32_t crc{0};
    std::int64_t  size{0};
    std::int64_t  compressedSize{0};
};

}  // namespace

ZipInputStream::ZipInputStream(std::span<const std::byte> data) noexcept : m_data(data) { }

Result<std::optional<ZipInputStream::Entry>> ZipInputStream::nextEntry()
{
    // getNextEntry() closes the current entry first, which reads it through.
    if (m_current.has_value())
    {
        const Header current = *m_current;
        if (Result<void> read = readData(current, nullptr, kNoLimit); !read)
        {
            return std::unexpected(std::move(read.error()));
        }
    }

    // readLOC(): no whole header, or no signature, is the end of the entries.
    if (m_data.size() - m_position < kLocalHeaderSize)
    {
        return std::nullopt;
    }
    const std::span<const std::byte> header = m_data.subspan(m_position, kLocalHeaderSize);
    if (get32(header, 0) != kLocalHeaderSignature)
    {
        return std::nullopt;
    }
    Header current;
    current.flag                 = get16(header, 6);
    current.method               = get16(header, 8);
    const std::size_t nameLength = get16(header, 26);
    const std::size_t extraSize  = get16(header, 28);
    m_position += kLocalHeaderSize;

    if (m_data.size() - m_position < nameLength)
    {
        return fail(ErrorCode::PARSE, std::string(kUnexpectedEnd));
    }
    const std::span<const std::byte> name = m_data.subspan(m_position, nameLength);
    m_position += nameLength;
    if (const std::optional<Malformed> malformed = firstMalformed(name))
    {
        return fail(ErrorCode::PARSE, std::format("malformed input off : {}, length : {}",
                                                  malformed->offset, malformed->length));
    }
    if ((current.flag & kFlagEncrypted) != 0)
    {
        return fail(ErrorCode::PARSE, "encrypted ZIP entry not supported");
    }
    if ((current.flag & kFlagDescriptor) == 0)
    {
        current.crc            = get32(header, 14);
        current.compressedSize = get32(header, 18);
        current.size           = get32(header, 22);
    }
    else if (current.method != kDeflated)  // "Data Descriptor" present
    {
        return fail(ErrorCode::PARSE, "only DEFLATED entries can have EXT descriptor");
    }

    if (m_data.size() - m_position < extraSize)
    {
        return fail(ErrorCode::PARSE, std::string(kUnexpectedEnd));
    }
    if (current.compressedSize == kZip64Magic || current.size == kZip64Magic)
    {
        readZip64Sizes(m_data.subspan(m_position, extraSize), current.size, current.compressedSize);
    }
    m_position += extraSize;

    current.dataStart = m_position;
    m_current         = current;
    std::string text(nameLength, '\0');
    std::ranges::transform(name, text.begin(), [](std::byte b) { return static_cast<char>(b); });
    const bool directory = text.ends_with('/');
    // ZipEntry.getSize(): the header's size, which an entry with a data descriptor leaves unset.
    const std::int64_t size = (current.flag & kFlagDescriptor) == 0 ? current.size : -1;
    return Entry{.name = std::move(text), .directory = directory, .size = size};
}

Result<std::vector<std::byte>> ZipInputStream::readEntry()
{
    return readEntry(kNoLimit);
}

Result<std::vector<std::byte>> ZipInputStream::readEntry(std::size_t maxBytes)
{
    std::vector<std::byte> contents;
    if (m_current.has_value())
    {
        const Header current = *m_current;
        if (Result<void> read = readData(current, &contents, maxBytes); !read)
        {
            return std::unexpected(std::move(read.error()));
        }
    }
    return contents;
}

Result<void> ZipInputStream::readData(const Header& header, std::vector<std::byte>* contents,
                                      std::size_t limit)
{
    m_current.reset();
    switch (header.method)
    {
        case kStored:
            return readStored(header, contents, limit);
        case kDeflated:
            return readDeflated(header, contents, limit);
        default:
            return fail(ErrorCode::PARSE, "invalid compression method");
    }
}

Result<void> ZipInputStream::readStored(const Header& header, std::vector<std::byte>* contents,
                                        std::size_t limit)
{
    // An entry of no (or a negative) size is at its end at once, without a CRC check.
    if (header.size <= 0)
    {
        return {};
    }
    const std::size_t available = m_data.size() - header.dataStart;
    // A bounded reader meets its limit as soon as more than that many bytes have come, which
    // is before a cut-off entry runs out when the data holds that many.
    if (contents != nullptr && std::cmp_greater(header.size, limit) && available > limit)
    {
        return exceedsLimit(limit);
    }
    if (std::cmp_greater(header.size, available))
    {
        return fail(ErrorCode::PARSE, "unexpected EOF");
    }
    const std::span<const std::byte> data =
        m_data.subspan(header.dataStart, static_cast<std::size_t>(header.size));
    m_position = header.dataStart + data.size();
    if (contents != nullptr)
    {
        contents->assign(data.begin(), data.end());
    }
    if (const std::uint32_t crc = crc32(0, data); crc != header.crc)
    {
        return fail(ErrorCode::PARSE, crcMismatch(header.crc, crc));
    }
    return {};
}

Result<void> ZipInputStream::readDeflated(const Header& header, std::vector<std::byte>* contents,
                                          std::size_t limit)
{
    const Result<Inflated> inflated = inflate(m_data.subspan(header.dataStart), contents, limit);
    if (!inflated)
    {
        return std::unexpected(inflated.error());
    }
    m_position = header.dataStart + static_cast<std::size_t>(inflated->read);

    // readEnd()
    Expected expected{
        .crc = header.crc, .size = header.size, .compressedSize = header.compressedSize};
    if ((header.flag & kFlagDescriptor) != 0)
    {
        // "Data Descriptor" present, its signature optional; ZIP64 sizes beyond 4 GiB.
        const bool        zip64  = inflated->written > kZip64Magic || inflated->read > kZip64Magic;
        const std::size_t length = zip64 ? kZip64DescriptorSize : kDescriptorSize;
        if (m_data.size() - m_position < length)
        {
            return fail(ErrorCode::PARSE, std::string(kUnexpectedEnd));
        }
        const std::span<const std::byte> descriptor = m_data.subspan(m_position, length);
        const bool                       signature  = get32(descriptor, 0) == kDescriptorSignature;
        const std::size_t                sizes      = signature ? 8 : 4;  // where the sizes start
        expected.crc                                = get32(descriptor, sizes - 4);
        expected.compressedSize = zip64 ? get64(descriptor, sizes) : get32(descriptor, sizes);
        expected.size = zip64 ? get64(descriptor, sizes + 8) : get32(descriptor, sizes + 4);
        // Without the signature the descriptor is 4 bytes shorter, which Java pushes back.
        m_position += signature ? length : length - 4;
    }
    if (expected.size != inflated->written)
    {
        return fail(ErrorCode::PARSE,
                    std::format("invalid entry size (expected {} but got {} bytes)", expected.size,
                                inflated->written));
    }
    if (expected.compressedSize != inflated->read)
    {
        return fail(ErrorCode::PARSE,
                    std::format("invalid entry compressed size (expected {} but got {} bytes)",
                                expected.compressedSize, inflated->read));
    }
    if (expected.crc != inflated->crc)
    {
        return fail(ErrorCode::PARSE, crcMismatch(expected.crc, inflated->crc));
    }
    return {};
}

}  // namespace QtRocket
