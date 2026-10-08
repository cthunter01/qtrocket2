#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// Reads the entries of a ZIP archive held in memory one after another from their local file
/// headers, as java.util.zip.ZipInputStream reads a stream (JDK 17). OpenRocket reads motor
/// archives this way; ZipArchive reads the central directory instead, which a truncated or
/// streamed archive lacks and which can list other entries, or list them in another order.
///
/// The entries end at the first place that does not hold a whole local header with the signature
/// "PK\3\4" (the central directory, other data, the end of the data, or data too short for a
/// header), so data that does not start with a local header holds no entries. An entry is read
/// by its local header: STORED entries by their size, DEFLATED entries to the end of their
/// deflate stream and then, when the header says so (bit 3), a data descriptor with or without
/// its signature; sizes of 0xFFFFFFFF are taken from a ZIP64 extra field. Reading an entry, or
/// moving past it without reading it, checks it as Java does, and the failures are ErrorCode::PARSE
/// with Java's messages: "invalid entry CRC (expected 0x... but got 0x...)", "invalid entry size
/// (expected ... but got ... bytes)", "invalid entry compressed size ...", "invalid compression
/// method", "only DEFLATED entries can have EXT descriptor", "encrypted ZIP entry not supported",
/// "unexpected EOF" (a STORED entry cut short), "Unexpected end of ZLIB input stream" (a DEFLATED
/// one), and for a name that is not valid UTF-8 (whatever the header's UTF-8 flag says, as Java
/// decodes names in UTF-8 either way) "malformed input off : <offset>, length : <length>".
/// Deviations: a local header cut off inside its name or extra field, or a data descriptor cut
/// off, fails with "Unexpected end of ZIP data" (Java's EOFException has no message), and corrupt
/// deflate data fails with "invalid deflate data in ZIP entry" (Java reports zlib's message).
///
/// After a failure the stream is at no entry: Java's callers close the stream there, and so
/// should a caller here stop reading.
class ZipInputStream final
{
public:
    /// An entry, as its local header gives it.
    struct Entry
    {
        std::string name;              ///< the path inside the archive, decoded as UTF-8
        bool        directory{false};  ///< the name ends with '/' (ZipEntry.isDirectory())
        /// The size of the contents the local header declares (ZipEntry.getSize()), which a
        /// reader may test before it reads them, or -1 when the header does not say: an entry
        /// with a data descriptor, as a writer that streams (Java's ZipOutputStream) makes every
        /// deflated entry, has its sizes behind its data. The declared size is the header's
        /// word and nothing more: readEntry() checks it against the contents. A ZIP64 field can
        /// make it negative, as Java's long.
        std::int64_t size{-1};
    };

    /// A reader of @p data, which must outlive it.
    explicit ZipInputStream(std::span<const std::byte> data) noexcept;
    /// A temporary buffer would dangle.
    explicit ZipInputStream(std::vector<std::byte>&& data) = delete;

    /// The next entry (getNextEntry()), after reading through and checking the rest of the
    /// current one; none at the end of the entries.
    [[nodiscard]] Result<std::optional<Entry>> nextEntry();

    /// The contents of the entry nextEntry() returned last, read and checked to its end; empty
    /// once read, or before the first entry.
    [[nodiscard]] Result<std::vector<std::byte>> readEntry();

    /// readEntry() for contents of at most @p maxBytes bytes, as OpenRocket's
    /// FileUtils.readBytes(stream, maxBytes) reads an entry: contents that are longer fail with
    /// ErrorCode::IO and "Input exceeds maximum size of <maxBytes> bytes", and no more than
    /// @p maxBytes bytes are ever held. As in Java the limit is met while reading, before the
    /// entry's end is reached and checked: an entry that is too long fails with this message
    /// whatever its CRC and sizes say, and a failure that comes first in the data (a STORED
    /// entry cut short before the limit, corrupt deflate data) is reported as readEntry() does.
    [[nodiscard]] Result<std::vector<std::byte>> readEntry(std::size_t maxBytes);

private:
    /// The current entry's local header.
    struct Header
    {
        std::uint16_t flag{0};
        std::uint16_t method{0};
        std::uint32_t crc{0};
        /// Java's long sizes: a ZIP64 field can make them negative.
        std::int64_t compressedSize{0};
        std::int64_t size{0};
        std::size_t  dataStart{0};
    };

    /// Reads the data of the entry @p header describes to its end into @p contents (when not
    /// null), which may take @p limit bytes, and checks it.
    [[nodiscard]] Result<void> readData(const Header& header, std::vector<std::byte>* contents,
                                        std::size_t limit);
    [[nodiscard]] Result<void> readStored(const Header& header, std::vector<std::byte>* contents,
                                          std::size_t limit);
    [[nodiscard]] Result<void> readDeflated(const Header& header, std::vector<std::byte>* contents,
                                            std::size_t limit);

    std::span<const std::byte> m_data;
    /// Where the next read starts.
    std::size_t m_position{0};
    /// The header of the entry whose data has not been read yet.
    std::optional<Header> m_current;
};

}  // namespace QtRocket
