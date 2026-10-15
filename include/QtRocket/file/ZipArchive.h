#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// A zip archive held in memory. Every file entry is extracted when the archive is parsed: an .ork
/// file is a few megabytes at most, and this keeps the class a plain value with no library handles
/// to manage.
class ZipArchive
{
public:
    struct Entry
    {
        std::string name;  ///< the path inside the archive, as stored ("thrustcurves/abc.rse")
        std::vector<std::byte> data;  ///< the uncompressed contents
    };

    /// Parses @p bytes as a zip archive. Directory entries are skipped.
    [[nodiscard]] static Result<ZipArchive> fromBytes(std::span<const std::byte> bytes);

    /// True when @p bytes starts with the local-file-header signature "PK\3\4".
    [[nodiscard]] static bool looksLikeZip(std::span<const std::byte> bytes) noexcept;

    [[nodiscard]] const std::vector<Entry>& entries() const noexcept { return m_entries; }
    [[nodiscard]] bool                      contains(std::string_view name) const noexcept;
    /// The entry's contents, or nullptr when there is no entry of exactly that name.
    [[nodiscard]] const std::vector<std::byte>* find(std::string_view name) const noexcept;

private:
    std::vector<Entry> m_entries;
};

/// Builds a zip archive in memory, deflate-compressed, with UTF-8 entry names, as
/// java.util.zip.ZipOutputStream writes one for OpenRocket's GeneralRocketSaver (setLevel(9)).
/// Every entry is written as a streaming writer writes it, its sizes and CRC in a data
/// descriptor behind its data (so ZipInputStream::Entry::size is -1 for it), and without ZIP64
/// fields, so that a reader of local headers such as java.util.zip.ZipInputStream reads on
/// behind an entry of no bytes. Each entry is deflated at level 9 (best compression), and its
/// central header says "made by" version 2.0 on MS-DOS (20) and "needed" version 2.0, as Java's
/// do; there is no extra field and no file attribute.
///
/// The time of the entries: the local time of finish() unless a time is set (setEntryTime()),
/// which every entry then carries as given, whatever the time zone of the process, so that the
/// same entries give an archive of the same headers on every platform (the deflated bytes are
/// zlib's, and zlib and zlib-ng deflate differently: compare the entries of an archive, never its
/// bytes).
///
/// The names are written as given, byte for byte. finish() refuses, and gives no archive for:
/// - a name added twice: "duplicate entry: <name>" (ErrorCode::IO), Java's ZipException;
/// - a name of more than 65535 bytes: "entry name too long" (ErrorCode::INVALID_ARGUMENT), the
///   text of Java's refusal of a name of more than 65535 characters;
/// - a name that holds a backslash or a NUL character (ErrorCode::INVALID_ARGUMENT,
///   backslashInName() and nulInName()).
///
/// Deviations from java.util.zip.ZipOutputStream:
/// - The refusals are made by finish(), for the whole archive; Java refuses a name when the
///   entry is begun (putNextEntry()), behind the entries before it.
/// - Java writes a name of more than 65535 bytes but not characters (UTF-8 of two or three
///   bytes a character) with its length cut to 16 bits, an archive no reader reads right; it is
///   refused here.
/// - Java writes a backslash and a NUL as they are. minizip, which writes the archive here,
///   turns a backslash into a slash on Windows only, and ends a name at a NUL: a name that would
///   be written otherwise than it was given, or otherwise on one platform than on another, is
///   refused.
/// - The general purpose flags of an entry also hold minizip's bit for the best compression
///   (0x080A where Java writes 0x0808), and its local header the size of its contents, where
///   Java writes 0; a reader takes both from the central header and the data descriptor.
/// - A time is the local time of the moment (now) or the fields of setEntryTime(); Java converts
///   a moment to the local time of the process. As in Java, a time before 1980, the first year a
///   zip header can hold, is written as 1980-01-01 00:00:00, and a second is written in steps of
///   two (an odd second as the one before it). A time after 2107 is refused (Java writes the
///   latest time a header can hold and the time itself in an extra field).
class ZipWriter
{
public:
    /// A local date and time as a zip header holds it (the MS-DOS form): the year, the month
    /// (1 to 12), the day of the month, the hour (0 to 23), the minute and the second (0 to 59).
    struct EntryTime
    {
        int year{1980};
        int month{1};
        int day{1};
        int hour{0};
        int minute{0};
        int second{0};

        [[nodiscard]] bool operator==(const EntryTime&) const = default;
    };

    /// The failure for a name of more than 65535 bytes (Java's text).
    static constexpr std::string_view kNameTooLong = "entry name too long";

    /// The failure for @p name added twice (Java's text): "duplicate entry: <name>".
    [[nodiscard]] static std::string duplicateEntry(std::string_view name);

    /// The failure for @p name, which holds a backslash (QtRocket's text): "The zip entry name
    /// '<name>' holds a backslash, which an archive written on Windows would turn into a slash."
    [[nodiscard]] static std::string backslashInName(std::string_view name);

    /// The failure for @p name, which holds a NUL character (QtRocket's text): "The zip entry
    /// name '<name>' holds a NUL character, which an archive cannot hold." (the NUL written as
    /// "\0").
    [[nodiscard]] static std::string nulInName(std::string_view name);

    /// The failure for @p time, which is not a time a zip header can hold (QtRocket's text):
    /// "The zip entry time 2108-01-01 00:00:00 is not a date and time from 1980 to 2107."
    [[nodiscard]] static std::string invalidEntryTime(const EntryTime& time);

    /// The 32 bits of @p time in the MS-DOS form of a zip header: the date in the high 16 bits
    /// (the year from 1980 in 7 bits, the month in 4, the day in 5), the time in the low 16 (the
    /// hour in 5 bits, the minute in 6, the second halved in 5); a time before 1980 is
    /// 1980-01-01 00:00:00 (Java's javaToDosTime()). Nullopt for a time after 2107 or with a
    /// field out of its range, a day included that its month does not have.
    [[nodiscard]] static std::optional<std::uint32_t> dosDateTime(const EntryTime& time) noexcept;

    /// Queues an entry. The name is checked by finish().
    void add(std::string name, std::span<const std::byte> data);

    /// Sets the time every entry carries; nullopt (the default) for the local time of finish().
    void setEntryTime(std::optional<EntryTime> time) noexcept { m_entryTime = time; }
    [[nodiscard]] const std::optional<EntryTime>& getEntryTime() const noexcept
    {
        return m_entryTime;
    }

    /// The complete archive with every entry added so far, or the failure of a name or of the
    /// entry time (see the class comment), for which there is no archive at all.
    [[nodiscard]] Result<std::vector<std::byte>> finish() const;

private:
    struct Pending
    {
        std::string            name;
        std::vector<std::byte> data;
    };

    /// The failure of the first name that cannot be written (see the class comment), or none.
    [[nodiscard]] Result<void> checkNames() const;

    std::vector<Pending>     m_pending;
    std::optional<EntryTime> m_entryTime;
};

}  // namespace QtRocket
