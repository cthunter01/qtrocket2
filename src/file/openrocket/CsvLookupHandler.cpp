#include "QtRocket/file/openrocket/CsvLookupHandler.h"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/aero/lookup/CsvMachAoALookup.h"
#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

CsvLookupHandler::CsvLookupHandler(SimulationOptions&       options,
                                   std::vector<std::string> requiredColumns, bool isDrag)
  : m_options(&options), m_requiredColumns(std::move(requiredColumns)), m_isDrag(isDrag)
{
}

Result<ElementHandler*> CsvLookupHandler::openElement(std::string_view element,
                                                      const Attributes& /*attributes*/,
                                                      WarningSet& /*warnings*/)
{
    if (element == "row")
    {
        return &PlainTextHandler::instance();
    }
    return nullptr;
}

Result<void> CsvLookupHandler::closeElement(std::string_view element,
                                            const Attributes& /*attributes*/,
                                            std::string_view content, WarningSet& /*warnings*/)
{
    if (element == "row")
    {
        const std::string_view trimmed = Strings::trim(content);
        if (!trimmed.empty())
        {
            m_rows.emplace_back(trimmed);
        }
    }
    return {};
}

Result<void> CsvLookupHandler::endHandler(std::string_view element, const Attributes& attributes,
                                          std::string_view /*content*/, WarningSet&   warnings)
{
    // Extract file path from attributes if present
    std::optional<std::filesystem::path> filePath;
    if (const auto file = attributes.find("file"); file != attributes.end())
    {
        const std::string_view trimmed = Strings::trim(file->second);
        if (!trimmed.empty())
        {
            filePath = withoutRedundantSeparators(pathFromUtf8(trimmed));
        }
    }

    // Try to load from embedded rows first (preferred)
    if (!m_rows.empty())
    {
        Result<MachAoALookup> table =
            CsvMachAoALookup::parse(m_rows, m_requiredColumns, detectSeparator(m_rows));
        std::optional<std::string> reason =
            table.has_value() ? nonFiniteReason(*table) : std::move(table.error().message);
        if (!reason.has_value())
        {
            // Store both the table and the original CSV rows to preserve edits
            auto shared = std::make_shared<const MachAoALookup>(std::move(*table));
            if (m_isDrag)
            {
                m_options->setDragLookup(filePath, std::move(shared), m_rows);
            }
            else
            {
                m_options->setStabilityLookup(filePath, std::move(shared), m_rows);
            }
            return {};
        }
        warnings.add(std::format("Failed to parse embedded CSV data in {}: {}", element, *reason));
    }

    // Fall back to loading from file if available
    if (filePath.has_value())
    {
        if (const Result<void> loaded = loadFromFile(*m_options, *filePath, m_isDrag); !loaded)
        {
            warnings.add(std::format("Failed to load {} from file '{}': {}", element,
                                     pathToUtf8(*filePath), loaded.error().message));
        }
    }
    else if (!m_rows.empty())
    {
        // If we have rows but no file path, still try to use the rows but with a warning
        warnings.add(std::format(
            "{} has embedded data but no file reference. Data may not persist correctly.",
            element));
    }
    return {};
}

Result<void> CsvLookupHandler::loadFromFile(SimulationOptions&           options,
                                            const std::filesystem::path& csvPath, bool isDrag)
{
    Result<std::shared_ptr<const MachAoALookup>> table =
        isDrag ? SimulationOptions::readDragLookupCsv(csvPath)
               : SimulationOptions::readStabilityLookupCsv(csvPath);
    if (!table.has_value())
    {
        return std::unexpected(std::move(table.error()));
    }
    // Not OpenRocket's (see the class comment): a table with a NaN or an infinity is refused.
    if (std::optional<std::string> reason = nonFiniteReason(**table))
    {
        return fail(ErrorCode::PARSE, std::move(*reason));
    }
    // What set...LookupCsvPath() stores: the file and the table, the rows as they are.
    if (isDrag)
    {
        options.setDragLookup(csvPath, std::move(*table), options.getDragLookupCsvRows());
    }
    else
    {
        options.setStabilityLookup(csvPath, std::move(*table), options.getStabilityLookupCsvRows());
    }
    return {};
}

std::optional<std::string> CsvLookupHandler::nonFiniteReason(const MachAoALookup& table)
{
    const std::optional<MachAoALookup::NonFiniteNumber> number = table.findNonFinite();
    if (!number.has_value())
    {
        return std::nullopt;
    }
    std::string_view column = number->column;
    if (number->kind == MachAoALookup::NonFiniteNumber::Kind::MACH)
    {
        column = "mach";
    }
    else if (number->kind == MachAoALookup::NonFiniteNumber::Kind::AOA)
    {
        column = "aoa";
    }
    // The text of the CSV reader for a field that is no number.
    return std::format("Illegal numeric value '{}' in column '{}'",
                       Strings::javaDoubleToString(number->value), column);
}

char CsvLookupHandler::detectSeparator(std::span<const std::string> rows) noexcept
{
    if (rows.empty())
    {
        return ',';
    }
    const std::string_view firstRow = rows.front();
    // Count occurrences of each common separator
    const std::ptrdiff_t commaCount     = std::ranges::count(firstRow, ',');
    const std::ptrdiff_t semicolonCount = std::ranges::count(firstRow, ';');
    const std::ptrdiff_t tabCount       = std::ranges::count(firstRow, '\t');

    // Return the separator with the most occurrences
    if (commaCount >= semicolonCount && commaCount >= tabCount && commaCount > 0)
    {
        return ',';
    }
    if (semicolonCount >= tabCount && semicolonCount > 0)
    {
        return ';';
    }
    if (tabCount > 0)
    {
        return '\t';
    }
    // Default to comma
    return ',';
}

}  // namespace QtRocket
