#include "QtRocket/file/ZipArchive.h"

#include <mz.h>
#include <mz_strm.h>  // IWYU pragma: keep (mz_zip_rw.h needs its callback types declared first)
#include <mz_strm_mem.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <expected>
#include <format>
#include <limits>
#include <optional>
#include <set>
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

constexpr std::size_t kMaxArchiveSize =
    static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
constexpr std::int32_t kWriterGrowSize = 256 * 1024;

/// The longest name a zip header holds, in bytes (its length has 16 bits).
constexpr std::size_t kMaxNameBytes = 0xFFFF;
/// "Made by" version 2.0 on MS-DOS, Java's ZipOutputStream's.
constexpr std::uint16_t kVersionMadeBy = 20;
/// The years an MS-DOS date holds.
constexpr int kFirstDosYear = 1980;
constexpr int kLastDosYear  = kFirstDosYear + 127;
/// 1980-01-01 00:00:00 (Java: ZipUtils.DOSTIME_BEFORE_1980).
constexpr std::uint32_t kDosTimeBefore1980 = (1U << 21U) | (1U << 16U);

constexpr std::uint32_t kLocalHeaderSignature   = 0x04034b50;
constexpr std::uint32_t kCentralHeaderSignature = 0x02014b50;
constexpr std::uint32_t kEndRecordSignature     = 0x06054b50;
constexpr std::size_t   kLocalHeaderSize        = 30;
constexpr std::size_t   kCentralHeaderSize      = 46;
constexpr std::size_t   kEndRecordSize          = 22;

/// The days of @p month (1 to 12) of @p year, by the Gregorian calendar.
[[nodiscard]] constexpr int daysInMonth(int year, int month) noexcept
{
    switch (month)
    {
        case 2:
            return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0 ? 29 : 28;
        case 4:
        case 6:
        case 9:
        case 11:
            return 30;
        default:
            return 31;
    }
}

/// The local time of the process now, in the MS-DOS form (ZipWriter::dosDateTime()), through
/// minizip's own conversion (localtime_r, localtime_s on Windows); 1980-01-01 00:00:00 when the
/// clock cannot be read or converted, or is beyond 2107.
[[nodiscard]] std::uint32_t localDosDateTimeNow() noexcept
{
    const std::time_t now   = std::time(nullptr);
    std::tm           local = {};
    if (now == static_cast<std::time_t>(-1) || mz_zip_time_t_to_tm(now, &local) != MZ_OK)
    {
        return kDosTimeBefore1980;
    }
    const ZipWriter::EntryTime time{.year   = local.tm_year + 1900,
                                    .month  = local.tm_mon + 1,
                                    .day    = local.tm_mday,
                                    .hour   = local.tm_hour,
                                    .minute = local.tm_min,
                                    .second = std::min(local.tm_sec, 59)};  // a leap second
    return ZipWriter::dosDateTime(time).value_or(kDosTimeBefore1980);
}

/// The little-endian field of @p width bytes at @p offset of @p bytes.
[[nodiscard]] std::uint32_t fieldAt(std::span<const std::byte> bytes, std::size_t offset,
                                    std::size_t width) noexcept
{
    std::uint32_t value = 0;
    for (std::size_t i = width; i > 0; --i)
    {
        value = (value << 8U) | std::to_integer<std::uint32_t>(bytes[offset + i - 1]);
    }
    return value;
}

/// Writes @p value as a little-endian 32-bit field at @p offset of @p bytes.
void put32(std::span<std::byte> bytes, std::size_t offset, std::uint32_t value) noexcept
{
    for (std::size_t i = 0; i < 4; ++i)
    {
        bytes[offset + i] = static_cast<std::byte>((value >> (8U * i)) & 0xFFU);
    }
}

/// Writes @p dosDateTime (the time in its low 16 bits, the date in its high 16) as the time of
/// every entry of @p archive, which minizip has just written: into the local header of each
/// entry and into its central header. The archive has no comment, so its end record is its last
/// 22 bytes; the central headers follow one another from the offset that record gives. False
/// when the archive is not laid out so.
[[nodiscard]] bool stampEntryTime(std::span<std::byte> archive, std::uint32_t dosDateTime) noexcept
{
    if (archive.size() < kEndRecordSize)
    {
        return false;
    }
    const std::size_t end = archive.size() - kEndRecordSize;
    if (fieldAt(archive, end, 4) != kEndRecordSignature)
    {
        return false;
    }
    // The central headers end at the end record, or at a ZIP64 end record before it (more than
    // 65534 entries), whose signature is not a central header's.
    for (std::size_t central = fieldAt(archive, end + 16, 4);
         central + kCentralHeaderSize <= end &&
         fieldAt(archive, central, 4) == kCentralHeaderSignature;
         central += kCentralHeaderSize + fieldAt(archive, central + 28, 2) +
                    fieldAt(archive, central + 30, 2) + fieldAt(archive, central + 32, 2))
    {
        const std::size_t local = fieldAt(archive, central + 42, 4);
        if (local + kLocalHeaderSize > end || fieldAt(archive, local, 4) != kLocalHeaderSignature)
        {
            return false;
        }
        put32(archive, central + 12, dosDateTime);
        put32(archive, local + 10, dosDateTime);
    }
    return true;
}

}  // namespace

bool ZipArchive::looksLikeZip(std::span<const std::byte> bytes) noexcept
{
    return bytes.size() >= 4 && bytes[0] == std::byte{'P'} && bytes[1] == std::byte{'K'} &&
           bytes[2] == std::byte{3} && bytes[3] == std::byte{4};
}

Result<ZipArchive> ZipArchive::fromBytes(std::span<const std::byte> bytes)
{
    if (bytes.size() > kMaxArchiveSize)
    {
        return fail(ErrorCode::UNSUPPORTED_FORMAT, "zip archive larger than 2 GiB");
    }
    const Detail::ZipReader reader;
    if (!reader.valid())
    {
        return fail(ErrorCode::UNKNOWN, "minizip: cannot create reader");
    }
    // minizip reads from the caller's buffer (copy = 0) and keeps nothing of it after this call.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto* data = reinterpret_cast<const std::uint8_t*>(bytes.data());
    if (const std::int32_t err = mz_zip_reader_open_buffer(
            reader.get(), data, static_cast<std::int32_t>(bytes.size()), 0);
        err != MZ_OK)
    {
        return fail(ErrorCode::PARSE, std::format("not a zip archive (minizip error {})", err));
    }

    ZipArchive   archive;
    std::int32_t err = mz_zip_reader_goto_first_entry(reader.get());
    while (err == MZ_OK)
    {
        mz_zip_file* info = nullptr;
        if (mz_zip_reader_entry_get_info(reader.get(), &info) != MZ_OK || info == nullptr ||
            info->filename == nullptr)
        {
            return fail(ErrorCode::PARSE, "zip archive: cannot read an entry header");
        }
        if (mz_zip_reader_entry_is_dir(reader.get()) != MZ_OK)  // MZ_OK means "is a directory"
        {
            Entry entry;
            entry.name                = info->filename;
            const std::int32_t length = mz_zip_reader_entry_save_buffer_length(reader.get());
            if (length < 0)
            {
                return fail(ErrorCode::PARSE,
                            std::format("zip archive: entry '{}' has an invalid size", entry.name));
            }
            entry.data.resize(static_cast<std::size_t>(length));
            if (length > 0 &&
                mz_zip_reader_entry_save_buffer(reader.get(), entry.data.data(), length) != MZ_OK)
            {
                return fail(ErrorCode::PARSE,
                            std::format("zip archive: cannot extract entry '{}'", entry.name));
            }
            archive.m_entries.push_back(std::move(entry));
        }
        err = mz_zip_reader_goto_next_entry(reader.get());
    }
    if (err != MZ_END_OF_LIST)
    {
        return fail(ErrorCode::PARSE,
                    std::format("zip archive: corrupt central directory (minizip error {})", err));
    }
    mz_zip_reader_close(reader.get());
    return archive;
}

bool ZipArchive::contains(std::string_view name) const noexcept
{
    return find(name) != nullptr;
}

const std::vector<std::byte>* ZipArchive::find(std::string_view name) const noexcept
{
    const auto it = std::ranges::find(m_entries, name, &Entry::name);
    return it == m_entries.end() ? nullptr : &it->data;
}

void ZipWriter::add(std::string name, std::span<const std::byte> data)
{
    m_pending.push_back(Pending{.name = std::move(name), .data = {data.begin(), data.end()}});
}

std::string ZipWriter::duplicateEntry(std::string_view name)
{
    return std::format("duplicate entry: {}", name);
}

std::string ZipWriter::backslashInName(std::string_view name)
{
    return std::format(
        "The zip entry name '{}' holds a backslash, which an archive written on Windows would "
        "turn into a slash.",
        name);
}

std::string ZipWriter::nulInName(std::string_view name)
{
    std::string shown;
    for (const char c : name)
    {
        if (c == '\0')
        {
            shown += "\\0";
        }
        else
        {
            shown += c;
        }
    }
    return std::format(
        "The zip entry name '{}' holds a NUL character, which an archive cannot hold.", shown);
}

std::string ZipWriter::invalidEntryTime(const EntryTime& time)
{
    return std::format(
        "The zip entry time {:04}-{:02}-{:02} {:02}:{:02}:{:02} is not a date and time from 1980 "
        "to 2107.",
        time.year, time.month, time.day, time.hour, time.minute, time.second);
}

std::optional<std::uint32_t> ZipWriter::dosDateTime(const EntryTime& time) noexcept
{
    if (time.month < 1 || time.month > 12 || time.day < 1 ||
        time.day > daysInMonth(time.year, time.month) || time.hour < 0 || time.hour > 23 ||
        time.minute < 0 || time.minute > 59 || time.second < 0 || time.second > 59 ||
        time.year > kLastDosYear)
    {
        return std::nullopt;
    }
    if (time.year < kFirstDosYear)
    {
        return kDosTimeBefore1980;
    }
    const auto date = static_cast<std::uint32_t>(((time.year - kFirstDosYear) << 9) |
                                                 (time.month << 5) | time.day);
    const auto clock =
        static_cast<std::uint32_t>((time.hour << 11) | (time.minute << 5) | (time.second >> 1));
    return (date << 16U) | clock;
}

Result<void> ZipWriter::checkNames() const
{
    std::set<std::string_view> names;
    for (const Pending& pending : m_pending)
    {
        const std::string_view name = pending.name;
        if (name.size() > kMaxNameBytes)
        {
            return fail(ErrorCode::INVALID_ARGUMENT, std::string(kNameTooLong));
        }
        if (name.contains('\\'))
        {
            return fail(ErrorCode::INVALID_ARGUMENT, backslashInName(name));
        }
        if (name.contains('\0'))
        {
            return fail(ErrorCode::INVALID_ARGUMENT, nulInName(name));
        }
        if (!names.insert(name).second)
        {
            return fail(ErrorCode::IO, duplicateEntry(name));
        }
    }
    return {};
}

Result<std::vector<std::byte>> ZipWriter::finish() const
{
    if (const Result<void> names = checkNames(); !names)
    {
        return std::unexpected(names.error());
    }
    std::uint32_t entryTime = 0;
    if (m_entryTime.has_value())
    {
        const std::optional<std::uint32_t> given = dosDateTime(*m_entryTime);
        if (!given.has_value())
        {
            return fail(ErrorCode::INVALID_ARGUMENT, invalidEntryTime(*m_entryTime));
        }
        entryTime = *given;
    }
    else
    {
        entryTime = localDosDateTimeNow();
    }

    Detail::MemStream       stream;
    const Detail::ZipWriter writer;
    if (!stream.valid() || !writer.valid())
    {
        return fail(ErrorCode::UNKNOWN, "minizip: cannot create writer");
    }
    mz_stream_mem_set_grow_size(stream.get(), kWriterGrowSize);
    if (!stream.open(MZ_OPEN_MODE_CREATE | MZ_OPEN_MODE_WRITE))
    {
        return fail(ErrorCode::UNKNOWN, "minizip: cannot open memory stream");
    }
    mz_zip_writer_set_compress_method(writer.get(), MZ_COMPRESS_METHOD_DEFLATE);
    // Java: ZipOutputStream.setLevel(9).
    mz_zip_writer_set_compress_level(writer.get(), MZ_COMPRESS_LEVEL_BEST);
    if (mz_zip_writer_open(writer.get(), stream.get(), 0) != MZ_OK)
    {
        return fail(ErrorCode::UNKNOWN, "minizip: cannot open zip writer");
    }
    for (const Pending& pending : m_pending)
    {
        if (pending.data.size() > kMaxArchiveSize)
        {
            return fail(ErrorCode::UNSUPPORTED_FORMAT,
                        std::format("zip entry '{}' larger than 2 GiB", pending.name));
        }
        mz_zip_file info{};
        info.version_madeby     = kVersionMadeBy;
        info.filename           = pending.name.c_str();
        info.flag               = MZ_ZIP_FLAG_UTF8;
        info.compression_method = MZ_COMPRESS_METHOD_DEFLATE;
        // No time for minizip, which would convert it to a local time of its own: the headers
        // get entryTime when the archive is complete (stampEntryTime()).
        info.modified_date     = 0;
        info.uncompressed_size = static_cast<std::int64_t>(pending.data.size());
        // No ZIP64 for an entry: none comes near 4 GiB (see the check above), and left to
        // itself minizip takes a size of 0 for "not known yet" and writes an empty entry in
        // ZIP64 form, with a data descriptor of 8-byte sizes. java.util.zip.ZipInputStream
        // (JDK 17), and ZipInputStream here, read 4-byte sizes there and find no entry after
        // such a one, so OpenRocket would lose every attachment behind an empty entry.
        info.zip64 = MZ_ZIP64_DISABLE;
        // minizip rejects a null buffer even for a zero-length entry, and an empty vector's data()
        // may be null.
        static constexpr std::byte kEmpty{};
        const void*                buffer =
            pending.data.empty() ? static_cast<const void*>(&kEmpty) : pending.data.data();
        if (mz_zip_writer_add_buffer(writer.get(), buffer,
                                     static_cast<std::int32_t>(pending.data.size()),
                                     &info) != MZ_OK)
        {
            return fail(ErrorCode::UNKNOWN,
                        std::format("minizip: cannot add entry '{}'", pending.name));
        }
    }
    if (mz_zip_writer_close(writer.get()) != MZ_OK)
    {
        return fail(ErrorCode::UNKNOWN, "minizip: cannot finish the archive");
    }
    const void*  buffer = nullptr;
    std::int32_t length = 0;
    mz_stream_mem_get_buffer(stream.get(), &buffer);
    mz_stream_mem_get_buffer_length(stream.get(), &length);
    if (buffer == nullptr || length < 0)
    {
        return fail(ErrorCode::UNKNOWN, "minizip: no archive was produced");
    }
    std::vector<std::byte> out(static_cast<std::size_t>(length));
    std::memcpy(out.data(), buffer, out.size());
    if (!stampEntryTime(out, entryTime))
    {
        return fail(ErrorCode::UNKNOWN, "minizip: the archive has no central directory to stamp");
    }
    return out;
}

}  // namespace QtRocket
