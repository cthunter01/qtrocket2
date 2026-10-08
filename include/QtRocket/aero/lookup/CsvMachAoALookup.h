#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/util/Error.h"

/// Reading a MachAoALookup from CSV (OpenRocket's aerodynamics/lookup/CsvMachAoALookup, a class
/// of static methods).
///
/// The format: lines are trimmed; empty lines and lines starting with '#' are skipped. The first
/// remaining line is the header, whose names are normalised as MachAoALookup::normalize() does
/// ("angleofattack" counting as "aoa"; the first of two equal names wins). It must have a "mach"
/// column and every required value column. When it has an "aoa" column, every row has an angle
/// of attack in degrees. Every following line is a row, split at the separator (empty fields
/// kept), each needed field trimmed and parsed as Java's Double.parseDouble does
/// (Strings::javaParseDouble).
///
/// Failures (Java: IllegalArgumentException, UncheckedIOException) are a Result: ErrorCode::IO
/// when the file cannot be read (a directory included) or is not valid UTF-8 ("Failed to read
/// lookup table from <path>", the path in UTF-8), ErrorCode::PARSE for the content with Java's
/// messages, and the Builder's errors (ErrorCode::INVALID_ARGUMENT, such as "No lookup data added"
/// for a header without rows).
///
/// Deviation from OpenRocket: the file that is read is bounded. A design file names the CSV
/// file of a lookup table (the file attribute of <draglookup> and <stabilitylookup>, the legacy
/// <draglookupcsv> and <stabilitylookupcsv>), so the name is untrusted input: a file larger than
/// kMaxFileBytes, and whatever is no regular file (a device, a pipe), is not read and fails with
/// the ErrorCode::IO and the text of a file that cannot be read. OpenRocket opens anything and
/// reads it to its end, which for a device never comes.
namespace QtRocket::CsvMachAoALookup
{

/// The largest CSV file fromCsv() reads, in bytes: 32 MiB, far beyond any table a simulation
/// could interpolate in (about a million rows).
inline constexpr std::size_t kMaxFileBytes = std::size_t{32} * 1024 * 1024;

/// Reads the table in the file @p path, whose lines end in \n, \r\n or \r (Java's
/// Files.readAllLines, UTF-8). A file of more than @p maxBytes bytes, and one that is no regular
/// file, is not read (see the deviation above).
[[nodiscard]] Result<MachAoALookup> fromCsv(const std::filesystem::path& path,
                                            std::span<const std::string> requiredValueColumns,
                                            char                         separator = ',',
                                            std::size_t                  maxBytes  = kMaxFileBytes);

/// Parses the table from @p lines.
[[nodiscard]] Result<MachAoALookup> parse(std::span<const std::string> lines,
                                          std::span<const std::string> requiredValueColumns,
                                          char                         separator);

/// Splits @p text into lines as Java's Files.readAllLines does: at \n, \r\n and \r, with no
/// empty last line for a final line terminator.
[[nodiscard]] std::vector<std::string> splitLines(std::string_view text);

}  // namespace QtRocket::CsvMachAoALookup
