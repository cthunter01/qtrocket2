#include "QtRocket/aero/lookup/CsvMachAoALookup.h"

#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket::CsvMachAoALookup
{

namespace
{

/// The header's column positions by normalised name (Java: Map<String, Integer>).
using HeaderIndex = std::map<std::string, std::size_t, std::less<>>;

constexpr std::string_view kSeparatorHint =
    "Make sure the column is included and you are using the correct field separator.";

/// normalize(), with "angleofattack" made "aoa".
[[nodiscard]] std::string normalizeHeaderName(std::string_view name)
{
    std::string normalized = MachAoALookup::normalize(name);
    if (normalized == "angleofattack")
    {
        normalized = "aoa";
    }
    return normalized;
}

/// Java's parseHeader().
[[nodiscard]] Result<HeaderIndex> parseHeader(std::string_view             headerLine,
                                              std::span<const std::string> requiredValueColumns,
                                              char                         separator)
{
    const std::vector<std::string> headers = Strings::split(headerLine, separator);
    HeaderIndex                    result;
    for (std::size_t i = 0; i < headers.size(); i++)
    {
        std::string normalized = normalizeHeaderName(headers[i]);
        if (!normalized.empty())
        {
            result.try_emplace(std::move(normalized), i);  // putIfAbsent
        }
    }

    if (!result.contains("mach"))
    {
        return fail(ErrorCode::PARSE, "Lookup table header must contain a 'mach' column. " +
                                          std::string{kSeparatorHint});
    }

    for (const std::string& column : requiredValueColumns)
    {
        if (!result.contains(normalizeHeaderName(column)))
        {
            return fail(ErrorCode::PARSE, "Lookup table header missing required column '" + column +
                                              "'. " + std::string{kSeparatorHint});
        }
    }
    return result;
}

/// Java's parse(tokens, index, column): the number in field @p index (nullopt for Java's null).
[[nodiscard]] Result<double> parseField(std::span<const std::string> tokens,
                                        std::optional<std::size_t> index, const std::string& column)
{
    if (!index)
    {
        return fail(ErrorCode::PARSE, "Column index missing for '" + column + "'");
    }
    if (*index >= tokens.size())
    {
        return fail(ErrorCode::PARSE, "Row missing value for column '" + column + "'");
    }
    const std::string_view      token = Strings::trim(tokens[*index]);
    const std::optional<double> value = Strings::javaParseDouble(token);
    if (!value)
    {
        return fail(ErrorCode::PARSE, "Illegal numeric value '" + std::string{token} +
                                          "' in column '" + column + "'");
    }
    return *value;
}

/// The position of @p name in @p header, or nullopt (Java: headerIndex.get() returning null).
[[nodiscard]] std::optional<std::size_t> positionOf(const HeaderIndex& header,
                                                    std::string_view   name)
{
    const auto found = header.find(name);
    if (found == header.end())
    {
        return std::nullopt;
    }
    return found->second;
}

/// One data row (Java's local mach, aoaDegrees and values).
struct ParsedRow
{
    double                mach;
    std::optional<double> aoaDegrees;
    MachAoALookup::Values values;
};

/// Parses the data row @p line: the Mach number, the angle of attack when the header has an
/// "aoa" column, and every value column but "aoa".
[[nodiscard]] Result<ParsedRow> parseRow(std::string_view line, const HeaderIndex& header,
                                         std::span<const std::string> normalizedColumns,
                                         char                         separator)
{
    const std::vector<std::string> tokens = Strings::split(line, separator);
    const Result<double>           mach   = parseField(tokens, positionOf(header, "mach"), "mach");
    if (!mach)
    {
        return std::unexpected(mach.error());
    }
    ParsedRow row{.mach = *mach, .aoaDegrees = std::nullopt, .values = {}};
    if (header.contains("aoa"))
    {
        const Result<double> aoa = parseField(tokens, positionOf(header, "aoa"), "aoa");
        if (!aoa)
        {
            return std::unexpected(aoa.error());
        }
        row.aoaDegrees = *aoa;
    }

    for (const std::string& column : normalizedColumns)
    {
        if (column == "aoa")
        {
            continue;
        }
        const Result<double> value = parseField(tokens, positionOf(header, column), column);
        if (!value)
        {
            return std::unexpected(value.error());
        }
        row.values.insert_or_assign(column, *value);
    }
    return row;
}

}  // namespace

Result<MachAoALookup> fromCsv(const std::filesystem::path& path,
                              std::span<const std::string> requiredValueColumns, char separator)
{
    const Result<std::string> text = readTextFile(path);
    // Java's Files.readAllLines() decodes UTF-8 strictly: malformed input is an IOException.
    if (!text || Strings::toValidUtf8(*text) != *text)
    {
        return fail(ErrorCode::IO, "Failed to read lookup table from " + pathToUtf8(path));
    }
    const std::vector<std::string> lines = splitLines(*text);
    return parse(lines, requiredValueColumns, separator);
}

Result<MachAoALookup> parse(std::span<const std::string> lines,
                            std::span<const std::string> requiredValueColumns, char separator)
{
    const std::vector<std::string> normalizedColumns =
        MachAoALookup::normalizeColumns(requiredValueColumns);
    MachAoALookup::Builder builder = MachAoALookup::builder(requiredValueColumns);
    if (builder.hasError())
    {
        return builder.build();  // Java throws from MachAoALookup.builder()
    }
    std::optional<HeaderIndex> headerIndex;

    for (const std::string& rawLine : lines)
    {
        const std::string_view line = Strings::trim(rawLine);
        if (line.empty() || line.starts_with('#'))
        {
            continue;
        }

        if (!headerIndex)
        {
            Result<HeaderIndex> header = parseHeader(line, normalizedColumns, separator);
            if (!header)
            {
                return std::unexpected(std::move(header.error()));
            }
            headerIndex = std::move(*header);
            continue;
        }

        const Result<ParsedRow> row = parseRow(line, *headerIndex, normalizedColumns, separator);
        if (!row)
        {
            return std::unexpected(row.error());
        }
        builder.addData(row->mach, row->aoaDegrees, row->values);
        if (builder.hasError())
        {
            return builder.build();  // Java throws from addData()
        }
    }

    if (!headerIndex)
    {
        return fail(ErrorCode::PARSE, "Lookup table is missing a header row");
    }
    return builder.build();
}

std::vector<std::string> splitLines(std::string_view text)
{
    std::vector<std::string> lines;
    std::size_t              start = 0;
    std::size_t              i     = 0;
    while (i < text.size())
    {
        const char c = text[i];
        if (c == '\n' || c == '\r')
        {
            lines.emplace_back(text.substr(start, i - start));
            if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
            {
                ++i;
            }
            start = i + 1;
        }
        ++i;
    }
    if (start < text.size())
    {
        lines.emplace_back(text.substr(start));
    }
    return lines;
}

}  // namespace QtRocket::CsvMachAoALookup
