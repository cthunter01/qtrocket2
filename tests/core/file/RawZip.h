#pragma once

// ZIP archives laid out byte by byte, for the tests of the classes that read an archive from its
// local file headers: entries whose headers say what the test wants, true or not. (ZipWriter
// makes a regular archive.) Test-only.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>
#include <vector>

#include "QtRocket/util/FileIo.h"

namespace QtRocket::Test
{

inline constexpr std::uint16_t kStored   = 0;
inline constexpr std::uint16_t kDeflated = 8;

/// "hello", its CRC and its raw deflate stream.
inline constexpr std::string_view            kHello    = "hello";
inline constexpr std::uint32_t               kHelloCrc = 0x3610a686;
inline constexpr std::array<std::uint8_t, 7> kHelloDeflated{0xcb, 0x48, 0xcd, 0xc9,
                                                            0xc9, 0x07, 0x00};

/// A local file header and what follows it, laid out byte by byte.
struct LocalEntry
{
    std::string_view       name;
    std::vector<std::byte> data;
    std::uint16_t          method{kStored};
    std::uint16_t          flags{0};
    std::uint32_t          crc{0};
    std::uint32_t          compressedSize{0};
    std::uint32_t          size{0};
    std::vector<std::byte> extra;
};

/// The CRC-32 of @p data (bit by bit; the tests' entries are short).
[[nodiscard]] inline std::uint32_t zipCrc32(const std::vector<std::byte>& data)
{
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const std::byte b : data)
    {
        crc ^= std::to_integer<std::uint32_t>(b);
        for (int bit = 0; bit < 8; bit++)
        {
            crc = (crc & 1U) != 0 ? (crc >> 1U) ^ 0xEDB88320U : crc >> 1U;
        }
    }
    return ~crc;
}

inline void put16(std::vector<std::byte>& out, std::uint32_t value)
{
    out.push_back(static_cast<std::byte>(value & 0xFFU));
    out.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
}

inline void put32(std::vector<std::byte>& out, std::uint32_t value)
{
    put16(out, value & 0xFFFFU);
    put16(out, value >> 16U);
}

inline void put64(std::vector<std::byte>& out, std::uint64_t value)
{
    put32(out, static_cast<std::uint32_t>(value & 0xFFFFFFFFU));
    put32(out, static_cast<std::uint32_t>(value >> 32U));
}

inline void append(std::vector<std::byte>& out, const LocalEntry& entry)
{
    put32(out, 0x04034b50);
    put16(out, 20);
    put16(out, entry.flags);
    put16(out, entry.method);
    put32(out, 0);  // time and date
    put32(out, entry.crc);
    put32(out, entry.compressedSize);
    put32(out, entry.size);
    put16(out, static_cast<std::uint32_t>(entry.name.size()));
    put16(out, static_cast<std::uint32_t>(entry.extra.size()));
    const std::vector<std::byte> name = stringToBytes(entry.name);
    out.insert(out.end(), name.begin(), name.end());
    out.insert(out.end(), entry.extra.begin(), entry.extra.end());
    out.insert(out.end(), entry.data.begin(), entry.data.end());
}

[[nodiscard]] inline LocalEntry stored(std::string_view name, std::string_view text)
{
    const std::vector<std::byte> data = stringToBytes(text);
    LocalEntry                   entry{.name           = name,
                                       .data           = data,
                                       .method         = kStored,
                                       .flags          = 0,
                                       .crc            = zipCrc32(data),
                                       .compressedSize = static_cast<std::uint32_t>(data.size()),
                                       .size           = static_cast<std::uint32_t>(data.size()),
                                       .extra          = {}};
    return entry;
}

[[nodiscard]] inline LocalEntry deflatedHello(std::string_view name)
{
    std::vector<std::byte> data(kHelloDeflated.size());
    std::ranges::transform(kHelloDeflated, data.begin(),
                           [](std::uint8_t b) { return static_cast<std::byte>(b); });
    return {.name           = name,
            .data           = data,
            .method         = kDeflated,
            .flags          = 0,
            .crc            = kHelloCrc,
            .compressedSize = static_cast<std::uint32_t>(kHelloDeflated.size()),
            .size           = static_cast<std::uint32_t>(kHello.size()),
            .extra          = {}};
}

/// An entry of no bytes in the ZIP64 form minizip writes one in when it is not told otherwise:
/// deflated, its sizes behind its data in a data descriptor whose two sizes are 8 bytes each,
/// announced by a ZIP64 extra field. java.util.zip.ZipInputStream (JDK 17) reads that
/// descriptor with 4-byte sizes (they fit) and so stops 8 bytes short of the next entry.
[[nodiscard]] inline LocalEntry zip64Empty(std::string_view name)
{
    LocalEntry entry{.name   = name,
                     .data   = {std::byte{0x03}, std::byte{0x00}},  // an empty deflate stream
                     .method = kDeflated,
                     .flags  = 0x0808,  // a data descriptor follows; the name is UTF-8
                     .crc    = 0,
                     .compressedSize = 0xFFFFFFFF,
                     .size           = 0xFFFFFFFF,
                     .extra          = {}};
    put16(entry.extra, 0x0001);
    put16(entry.extra, 16);
    put64(entry.extra, 0);
    put64(entry.extra, 0);
    put32(entry.data, 0x08074b50);
    put32(entry.data, 0);  // the CRC
    put64(entry.data, 2);  // the compressed size
    put64(entry.data, 0);  // the size
    return entry;
}

[[nodiscard]] inline std::vector<std::byte> archive(std::initializer_list<LocalEntry> entries)
{
    std::vector<std::byte> out;
    for (const LocalEntry& entry : entries)
    {
        append(out, entry);
    }
    return out;
}

/// The bytes @p hex spells, two hexadecimal digits each: an archive another program made, pasted
/// into a test.
[[nodiscard]] inline std::vector<std::byte> bytesFromHex(std::string_view hex)
{
    const auto digit = [](char c) -> unsigned {
        return c <= '9' ? static_cast<unsigned>(c - '0') : static_cast<unsigned>(c - 'a') + 10U;
    };
    std::vector<std::byte> bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
    {
        bytes.push_back(static_cast<std::byte>((digit(hex[i]) << 4U) | digit(hex[i + 1])));
    }
    return bytes;
}

/// The archive Java's ZipOutputStream makes of one deflated entry "decal.png" with the bytes 1,
/// 2, 3, 4 (ZipFileAttachmentTest.createArchive()): it streams, so the local header has no sizes
/// and a data descriptor follows the data.
inline constexpr std::string_view kJavaArchiveOfFourBytes =
    "504b0304140008080800a8b5475d00000000000000000000000009000000646563616c2e706e67636462660100"
    "504b0708cdfb3cb60600000004000000504b01021400140008080800a8b5475dcdfb3cb6060000000400000009"
    "0000000000000000000000000000000000646563616c2e706e67504b05060000000001000100370000003d0000"
    "000000";

/// The same of one entry "decal.png" of 65 zero bytes.
inline constexpr std::string_view kJavaArchiveOf65Zeros =
    "504b0304140008080800a8b5475d00000000000000000000000009000000646563616c2e706e676360a0100000"
    "504b070877f7cd1d0600000041000000504b01021400140008080800a8b5475d77f7cd1d060000004100000009"
    "0000000000000000000000000000000000646563616c2e706e67504b05060000000001000100370000003d0000"
    "000000";

}  // namespace QtRocket::Test
