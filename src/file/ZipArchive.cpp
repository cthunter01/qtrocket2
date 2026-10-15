#include "QtRocket/file/ZipArchive.h"

#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <expected>
#include <format>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "MinizipHandle.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

namespace
{

constexpr std::size_t kMaxArchiveSize =
    static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());

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

/// The archive a ZipWriter writes, as it grows (QtRocket's own output for minizip in place of
/// its memory stream, which grows by a fixed step, copying the whole archive each time, and
/// counts in 32 bits), and what stopped it.
struct ArchiveBytes
{
    std::vector<std::byte> bytes;  ///< as far as it was written
    std::size_t            position{0};
    bool                   outOfMemory{false};  ///< a write could not get the memory
    bool                   tooLarge{false};     ///< a write would have gone beyond 2 GiB
};

/// The minizip stream of an ArchiveBytes: minizip calls a stream through the mz_stream at its
/// start, and the callbacks below get the address of the whole struct back (a standard-layout
/// struct and its first member have the same address).
struct ArchiveStream
{
    mz_stream      stream{};
    mz_stream_vtbl table{};
    ArchiveBytes*  out{nullptr};
};
static_assert(std::is_standard_layout_v<ArchiveStream>);

/// The ArchiveBytes of @p stream, an ArchiveStream minizip hands back.
[[nodiscard]] ArchiveBytes& bytesOf(void* stream) noexcept
{
    return *static_cast<ArchiveStream*>(stream)->out;
}

/// Makes @p out hold @p size bytes at least; false when the memory runs out (noted in @p out).
[[nodiscard]] bool grow(ArchiveBytes& out, std::size_t size) noexcept
{
    if (size <= out.bytes.size())
    {
        return true;
    }
    try
    {
        // std::vector grows its capacity geometrically, so the archive is copied a few times
        // in all, however many writes make it.
        out.bytes.resize(size);
        return true;
    }
    catch (const std::bad_alloc&)
    {
        out.outOfMemory = true;
        return false;
    }
}

[[nodiscard]] std::int32_t archiveOpen(void* /*stream*/, const char* /*path*/,
                                       std::int32_t /*mode*/) noexcept
{
    return MZ_OK;
}

[[nodiscard]] std::int32_t archiveIsOpen(void* /*stream*/) noexcept
{
    return MZ_OK;
}

[[nodiscard]] std::int32_t archiveRead(void* stream, void* buffer, std::int32_t size) noexcept
{
    ArchiveBytes& in = bytesOf(stream);
    if (size <= 0 || in.position >= in.bytes.size())
    {
        return 0;
    }
    const std::size_t count =
        std::min(static_cast<std::size_t>(size), in.bytes.size() - in.position);
    const std::span<std::byte> target(static_cast<std::byte*>(buffer), count);
    std::ranges::copy(std::span<const std::byte>(in.bytes).subspan(in.position, count),
                      target.begin());
    in.position += count;
    return static_cast<std::int32_t>(count);
}

[[nodiscard]] std::int32_t archiveWrite(void* stream, const void* buffer,
                                        std::int32_t size) noexcept
{
    ArchiveBytes& out = bytesOf(stream);
    if (size <= 0)
    {
        return 0;
    }
    const auto count = static_cast<std::size_t>(size);
    if (out.position > kMaxArchiveSize || count > kMaxArchiveSize - out.position)
    {
        out.tooLarge = true;
        return MZ_WRITE_ERROR;
    }
    if (!grow(out, out.position + count))
    {
        return MZ_MEM_ERROR;
    }
    const std::span<const std::byte> source(static_cast<const std::byte*>(buffer), count);
    std::ranges::copy(source, std::span<std::byte>(out.bytes).subspan(out.position).begin());
    out.position += count;
    return size;
}

[[nodiscard]] std::int64_t archiveTell(void* stream) noexcept
{
    return static_cast<std::int64_t>(bytesOf(stream).position);
}

[[nodiscard]] std::int32_t archiveSeek(void* stream, std::int64_t offset,
                                       std::int32_t origin) noexcept
{
    ArchiveBytes& out  = bytesOf(stream);
    std::int64_t  from = 0;
    switch (origin)
    {
        case MZ_SEEK_SET:
            break;
        case MZ_SEEK_CUR:
            from = static_cast<std::int64_t>(out.position);
            break;
        case MZ_SEEK_END:
            from = static_cast<std::int64_t>(out.bytes.size());
            break;
        default:
            return MZ_SEEK_ERROR;
    }
    // As minizip's memory stream: a position beyond the end, in an archive being written, makes
    // the archive that long.
    constexpr auto kLast = static_cast<std::int64_t>(kMaxArchiveSize);
    if (offset < -from || offset > kLast - from)
    {
        return MZ_SEEK_ERROR;
    }
    const auto position = static_cast<std::size_t>(from + offset);
    if (!grow(out, position))
    {
        return MZ_MEM_ERROR;
    }
    out.position = position;
    return MZ_OK;
}

[[nodiscard]] std::int32_t archiveClose(void* /*stream*/) noexcept
{
    return MZ_OK;
}

[[nodiscard]] std::int32_t archiveError(void* /*stream*/) noexcept
{
    return MZ_OK;
}

/// The callbacks of an ArchiveStream; it is never created or deleted by minizip, and has no
/// properties.
[[nodiscard]] mz_stream_vtbl archiveStreamTable() noexcept
{
    mz_stream_vtbl table{};
    table.open    = archiveOpen;
    table.is_open = archiveIsOpen;
    table.read    = archiveRead;
    table.write   = archiveWrite;
    table.tell    = archiveTell;
    table.seek    = archiveSeek;
    table.close   = archiveClose;
    table.error   = archiveError;
    return table;
}

/// The refusal of @p name (see the class comment), which @p names do not hold yet, or none.
[[nodiscard]] Result<void> checkName(std::string_view                          name,
                                     const std::set<std::string, std::less<>>& names)
{
    if (name.size() > kMaxNameBytes)
    {
        return fail(ErrorCode::INVALID_ARGUMENT, std::string(ZipWriter::kNameTooLong));
    }
    if (name.contains('\\'))
    {
        return fail(ErrorCode::INVALID_ARGUMENT, ZipWriter::backslashInName(name));
    }
    if (name.contains('\0'))
    {
        return fail(ErrorCode::INVALID_ARGUMENT, ZipWriter::nulInName(name));
    }
    if (names.contains(name))
    {
        return fail(ErrorCode::IO, ZipWriter::duplicateEntry(name));
    }
    return {};
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

std::string ZipWriter::outOfMemory(std::string_view name)
{
    return std::format("Out of memory writing the zip entry {}", name);
}

/// What a writer holds: the archive so far, the stream minizip writes it through, minizip's writer
/// and the names of the entries, and the first failure.
struct ZipWriter::State
{
    ArchiveBytes  bytes;
    ArchiveStream stream;
    /// Declared after the stream: a writer that was never finished is closed by its destructor,
    /// which writes the central directory to the stream.
    Detail::ZipWriter                  writer;
    std::set<std::string, std::less<>> names;
    std::optional<Error>               failure;

    State()
    {
        stream.stream.vtbl = &stream.table;
        stream.table       = archiveStreamTable();
        stream.out         = &bytes;
        if (!writer.valid())
        {
            failure = fail(ErrorCode::UNKNOWN, "minizip: cannot create writer").error();
            return;
        }
        mz_zip_writer_set_compress_method(writer.get(), MZ_COMPRESS_METHOD_DEFLATE);
        // Java: ZipOutputStream.setLevel(9).
        mz_zip_writer_set_compress_level(writer.get(), MZ_COMPRESS_LEVEL_BEST);
        if (mz_zip_writer_open(writer.get(), static_cast<void*>(&stream), 0) != MZ_OK)
        {
            failure = fail(ErrorCode::UNKNOWN, "minizip: cannot open zip writer").error();
        }
    }
    State(const State&)            = delete;
    State& operator=(const State&) = delete;
    State(State&&)                 = delete;
    State& operator=(State&&)      = delete;
    ~State()                       = default;

    /// The failure of minizip's @p what (a text for an error of its own): the memory that ran out
    /// while @p name was written or the archive that grew too long, as the stream noted them.
    [[nodiscard]] Error failureOf(std::string_view name, std::string_view what) const
    {
        if (bytes.outOfMemory)
        {
            return fail(ErrorCode::IO, outOfMemory(name)).error();
        }
        if (bytes.tooLarge)
        {
            return fail(ErrorCode::UNSUPPORTED_FORMAT, std::string(kArchiveTooLarge)).error();
        }
        return fail(ErrorCode::UNKNOWN, std::string(what)).error();
    }
};

ZipWriter::ZipWriter() : m_state(std::make_unique<State>()) { }

ZipWriter::~ZipWriter() = default;

ZipWriter::ZipWriter(ZipWriter&&) noexcept = default;

ZipWriter& ZipWriter::operator=(ZipWriter&&) noexcept = default;

void ZipWriter::add(std::string_view name, std::span<const std::byte> data)
{
    QTROCKET_ASSERT(m_state != nullptr);
    State& state = *m_state;
    if (state.failure.has_value())
    {
        return;
    }
    if (const Result<void> checked = checkName(name, state.names); !checked)
    {
        state.failure = checked.error();
        return;
    }
    if (data.size() > kMaxArchiveSize)
    {
        state.failure = fail(ErrorCode::UNSUPPORTED_FORMAT,
                             std::format("zip entry '{}' larger than 2 GiB", name))
                            .error();
        return;
    }
    const std::string filename(name);
    state.names.insert(filename);

    mz_zip_file info{};
    info.version_madeby     = kVersionMadeBy;
    info.filename           = filename.c_str();
    info.flag               = MZ_ZIP_FLAG_UTF8;
    info.compression_method = MZ_COMPRESS_METHOD_DEFLATE;
    // No time for minizip, which would convert it to a local time of its own: the headers get
    // the entry time when the archive is complete (stampEntryTime()).
    info.modified_date     = 0;
    info.uncompressed_size = static_cast<std::int64_t>(data.size());
    // No ZIP64 for an entry: none comes near 4 GiB (see the check above), and left to itself
    // minizip takes a size of 0 for "not known yet" and writes an empty entry in ZIP64 form, with
    // a data descriptor of 8-byte sizes. java.util.zip.ZipInputStream (JDK 17), and
    // ZipInputStream here, read 4-byte sizes there and find no entry after such a one, so
    // OpenRocket would lose every attachment behind an empty entry.
    info.zip64 = MZ_ZIP64_DISABLE;
    // minizip rejects a null buffer even for a zero-length entry, and an empty span's data() may
    // be null.
    static constexpr std::byte kEmpty{};
    const void* buffer = data.empty() ? static_cast<const void*>(&kEmpty) : data.data();
    if (mz_zip_writer_add_buffer(state.writer.get(), buffer, static_cast<std::int32_t>(data.size()),
                                 &info) != MZ_OK)
    {
        state.failure = state.failureOf(name, std::format("minizip: cannot add entry '{}'", name));
    }
}

Result<std::vector<std::byte>> ZipWriter::finish()
{
    QTROCKET_ASSERT(m_state != nullptr);
    const std::unique_ptr<State> state = std::move(m_state);
    if (state->failure.has_value())
    {
        return std::unexpected(*state->failure);
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
    if (mz_zip_writer_close(state->writer.get()) != MZ_OK)
    {
        return std::unexpected(
            state->failureOf("(the central directory)", "minizip: cannot finish the archive"));
    }
    std::vector<std::byte> out = std::move(state->bytes.bytes);
    if (!stampEntryTime(out, entryTime))
    {
        return fail(ErrorCode::UNKNOWN, "minizip: the archive has no central directory to stamp");
    }
    return out;
}

}  // namespace QtRocket
