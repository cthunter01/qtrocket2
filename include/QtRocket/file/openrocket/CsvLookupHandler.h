#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class SimulationOptions;

/// Reads one <draglookup> or <stabilitylookup> element of a simulation's <conditions>
/// (OpenRocket's file/openrocket/importt/CsvLookupHandler): a lookup table that replaces the
/// computed drag or stability, embedded in the design as its CSV rows.
///
///     <draglookup file="path/to/file.csv">
///       <row>Mach,AoA,Cd</row>
///       <row>0.30,0,0.35</row>
///     </draglookup>
///
/// The conditions handler makes one per element with the options it fills, the value columns
/// the table must have ("cd"; "cn", "cm" and "cp") and which of the two tables it is. The work
/// is done when the element closes (endHandler()):
/// 1. The file attribute, trimmed, is the CSV file the table came from, when it is not blank.
/// 2. With rows (the <row> children, each trimmed, a blank one dropped), the table is parsed
///    from them (CsvMachAoALookup::parse()) with the separator the first row suggests: the
///    most frequent of comma, semicolon and tab in it (a comma for a tie with the comma and
///    for none, a semicolon for a tie with the tab). The table, the file and the rows go into
///    the options (SimulationOptions::setDragLookup() or setStabilityLookup()), whether or not
///    the file exists, and that is all. A failure gives "Failed to parse embedded CSV data in
///    <element>: <reason>" and step 3.
/// 3. Without rows, or when they did not parse: with a file, the table is read from it as
///    SimulationOptions::setDragLookupCsvPath() or setStabilityLookupCsvPath() reads it (the
///    rows the options hold stay what they are), and a failure gives "Failed to load <element>
///    from file '<file>': <reason>". Without a file, rows that did not parse give "<element>
///    has embedded data but no file reference. Data may not persist correctly.".
/// A relative file is resolved against the current directory of the process, as in OpenRocket.
///
/// A child that is no <row> is ignored with everything in it and without a warning; through
/// DelegatorHandler's bookkeeping slip the element then closes with that child's attributes, so
/// its file attribute is lost.
///
/// Deviations from OpenRocket:
/// - A table that holds a number that is not finite (a Mach number, an angle of attack or a
///   value that is a NaN or an infinity; MachAoALookup::findNonFinite()) is not taken (decision
///   U3). It counts as rows, or a file, that did not parse, with the reason "Illegal numeric
///   value '<number>' in column '<column>'", the number as Java prints a double. OpenRocket's
///   reader takes "NaN" and "Infinity" as numbers and stores the table, and a flight that
///   interpolates in it fails.
/// - A file is read only when it is a regular file of at most
///   CsvMachAoALookup::kMaxFileBytes bytes (see CsvMachAoALookup).
/// - OpenRocket's "Invalid file path in <element>: <file>" (a text that java.nio.file.Path
///   refuses: a NUL character, on Windows also characters such as '<' or '?') is never given:
///   std::filesystem::path takes any text, and such a file then fails when it is read.
/// - The file in the second warning is printed as std::filesystem::path spells it, without
///   repeated separators and without one at its end, which is what Java's Path prints on
///   POSIX. A Windows path that Java spells otherwise may differ.
class CsvLookupHandler final : public AbstractElementHandler
{
public:
    /// A handler that fills the drag table of @p options when @p isDrag, else the stability
    /// table, from rows that must have the columns @p requiredColumns. @p options must outlive
    /// the handler.
    CsvLookupHandler(SimulationOptions& options, std::vector<std::string> requiredColumns,
                     bool isDrag);

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

    [[nodiscard]] Result<void> endHandler(std::string_view element, const Attributes& attributes,
                                          std::string_view content, WarningSet& warnings) override;

    /// The rows read so far.
    [[nodiscard]] const std::vector<std::string>& getRows() const noexcept { return m_rows; }

    /// Reads the drag table (@p isDrag) or the stability table of @p options from the CSV file
    /// @p csvPath and stores it with the file, as SimulationOptions::setDragLookupCsvPath() or
    /// setStabilityLookupCsvPath() does, but for a table that holds a number that is not
    /// finite, which is not stored. Fails with the reader's Error, or with ErrorCode::PARSE and
    /// nonFiniteReason() for such a table; nothing changes then. The conditions handler reads
    /// the legacy <draglookupcsv> and <stabilitylookupcsv> elements with it.
    [[nodiscard]] static Result<void> loadFromFile(SimulationOptions&           options,
                                                   const std::filesystem::path& csvPath,
                                                   bool                         isDrag);

    /// Why the loader does not take @p table: "Illegal numeric value '<number>' in column
    /// '<column>'" for its first number that is not finite (MachAoALookup::findNonFinite(); the
    /// columns of a Mach number and of an angle of attack are "mach" and "aoa"), or nullopt for
    /// a table of finite numbers.
    [[nodiscard]] static std::optional<std::string> nonFiniteReason(const MachAoALookup& table);

    /// The separator of CSV rows, told from the first of @p rows (detectSeparator(); see the
    /// class comment); a comma without rows.
    [[nodiscard]] static char detectSeparator(std::span<const std::string> rows) noexcept;

private:
    SimulationOptions*       m_options;
    std::vector<std::string> m_requiredColumns;
    bool                     m_isDrag;
    std::vector<std::string> m_rows;
};

}  // namespace QtRocket
