#pragma once

#include <cstddef>
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

/// Builds a zip archive in memory, deflate-compressed, with UTF-8 entry names.
class ZipWriter
{
public:
    /// Queues an entry. Adding the same name twice stores it twice, as zip allows; callers avoid
    /// that.
    void add(std::string name, std::span<const std::byte> data);

    /// The complete archive with every entry added so far.
    [[nodiscard]] Result<std::vector<std::byte>> finish() const;

private:
    struct Pending
    {
        std::string            name;
        std::vector<std::byte> data;
    };
    std::vector<Pending> m_pending;
};

}  // namespace QtRocket
