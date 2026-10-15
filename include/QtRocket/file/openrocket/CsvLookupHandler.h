#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/file/openrocket/DesignFiles.h"
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
/// the table must have ("cd"; "cn", "cm" and "cp"), which of the two tables it is and the
/// directory of the design. The work is done when the element closes (endHandler()):
/// 1. The file attribute, trimmed, is the CSV file the table came from, when it is not blank.
/// 2. With rows (the <row> children, each trimmed, a blank one dropped), the table is parsed
///    from them (CsvMachAoALookup::parse()) with the separator the first row suggests: the
///    most frequent of comma, semicolon and tab in it (a comma for a tie with the comma and
///    for none, a semicolon for a tie with the tab). The table, the file and the rows go into
///    the options (SimulationOptions::setDragLookup() or setStabilityLookup()), whether or not
///    the file exists, and that is all: the file is a name that is kept (nameOfDesign()) and
///    is not opened. A failure gives "Failed to parse embedded CSV data in <element>:
///    <reason>" and step 3.
/// 3. Without rows, or when they did not parse: with a file, the table is read from it as
///    SimulationOptions::setDragLookupCsvPath() or setStabilityLookupCsvPath() reads it (the
///    rows the options hold stay what they are), when it is a file the design may read (see
///    below), and a failure gives "Failed to load <element> from file '<file>': <reason>".
///    Without a file, rows that did not parse give "<element> has embedded data but no file
///    reference. Data may not persist correctly.".
///
/// A child that is no <row> is ignored with everything in it and without a warning; through
/// DelegatorHandler's bookkeeping slip the element then closes with that child's attributes, so
/// its file attribute is lost.
///
/// WHICH FILE A DESIGN MAY READ (the user's decision of 2026-10-08): a design reads a lookup
/// table only from inside its own directory, by the rule of DesignFiles::fileOfDesign() (which
/// this class names fileOfDesign() too): a relative name is resolved against the design's
/// directory, the current directory of the process plays no part, a name that is not relative
/// or that leaves the directory through ".." is refused, a design without a directory reads no
/// file at all, and on Windows only a name of a device is refused. A refusal gives the warning
/// LoaderWarnings::lookupFileOutsideDesign(), or LoaderWarnings::lookupFileOfDeviceName() for a
/// device; no file is opened and the options keep what they have. The legacy elements
/// <draglookupcsv> and <stabilitylookupcsv>, which the conditions handler reads, follow the
/// same rule.
///
/// Deviations from OpenRocket:
/// - The rule above. Its consequences: a file is named against the design's directory where
///   OpenRocket names it against the current directory (so the path the options keep for a
///   relative name is another one); a design that names its table by an absolute path, as
///   every design does that OpenRocket saved, gets its table from the rows OpenRocket embeds
///   beside the path, and a warning in the place of a table when it has no rows.
/// - With rows, a relative name of a design without a directory is not kept (OpenRocket
///   resolves it against the current directory); see nameOfDesign().
/// - A table that holds a number that is not finite (a Mach number, an angle of attack or a
///   value that is a NaN or an infinity) is not taken (decision U3), and neither is one with
///   a number outside the range a simulation holds it to (decision U9; SimulationInputRanges:
///   a Mach number from 0 to 100, an angle of attack within half a turn, a coefficient of at
///   most a million, a centre of pressure within 1000 m). It counts as rows, or a file, that
///   did not parse, with the reason "Illegal numeric value '<number>' in column '<column>'",
///   the number as Java prints a double. OpenRocket's reader takes "NaN", "Infinity" and
///   every number and stores the table, and a flight that interpolates in such a table fails.
/// - A file is read only when it is a regular file of at most
///   CsvMachAoALookup::kMaxFileBytes bytes (see CsvMachAoALookup).
/// - OpenRocket's "Invalid file path in <element>: <file>" (a text that java.nio.file.Path
///   refuses: a NUL character, on Windows also characters such as '<' or '?') is never given:
///   std::filesystem::path takes any text, and such a file then fails when it is read.
/// - The file in the second warning is printed as std::filesystem::path spells it, without
///   repeated separators and without one at its end, which is what Java's Path prints on
///   POSIX. A Windows path that Java spells otherwise may differ.
/// - The constructor takes the design's directory, which Java's has no need of.
class CsvLookupHandler final : public AbstractElementHandler
{
public:
    /// Why a design may not read a file it names (DesignFiles::Refusal).
    using Refusal = DesignFiles::Refusal;

    /// Whether fileOfDesign() refuses the name of a device (DesignFiles::kRefusesDeviceNames).
    static constexpr bool kRefusesDeviceNames = DesignFiles::kRefusesDeviceNames;

    /// A handler that fills the drag table of @p options when @p isDrag, else the stability
    /// table, from rows that must have the columns @p requiredColumns, for a design whose
    /// directory is @p designDirectory (DocumentLoadingContext::getDesignDirectory(); nullopt
    /// for a design without one, which reads no file). @p options must outlive the handler.
    CsvLookupHandler(SimulationOptions& options, std::vector<std::string> requiredColumns,
                     bool isDrag, std::optional<std::filesystem::path> designDirectory);

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

    /// DesignFiles::fileOfDesign(): the file a design whose directory is @p designDirectory
    /// names as @p file, or why the design may not read it.
    [[nodiscard]] static std::expected<std::filesystem::path, Refusal> fileOfDesign(
        const std::optional<std::filesystem::path>& designDirectory,
        const std::filesystem::path&                file)
    {
        return DesignFiles::fileOfDesign(designDirectory, file);
    }

    /// The warning for a file that fileOfDesign() refused for @p refusal: @p element is the
    /// element that names it and @p file the name as the design has it.
    [[nodiscard]] static std::string refusalWarning(Refusal refusal, std::string_view element,
                                                    std::string_view file);

    /// The path the options keep as the file of a table that is in the design as rows, for a
    /// design whose directory is @p designDirectory and that names @p file beside the rows.
    /// The file is never opened: the path is a name, which a save writes back.
    /// - A name that is not relative is kept as it is, as in OpenRocket.
    /// - A relative name is resolved against the design's directory, also when it leaves the
    ///   directory (OpenRocket: against the current directory of the process).
    /// - A relative name of a design without a directory is not kept (nullopt): there is
    ///   nothing it would be relative to.
    [[nodiscard]] static std::optional<std::filesystem::path> nameOfDesign(
        const std::optional<std::filesystem::path>& designDirectory,
        const std::filesystem::path&                file);

    /// DesignFiles::isDeviceName(): whether Windows reserves @p name, one element of a path,
    /// for a device.
    [[nodiscard]] static bool isDeviceName(std::string_view name)
    {
        return DesignFiles::isDeviceName(name);
    }

    /// Reads the drag table (@p isDrag) or the stability table of @p options from the CSV file
    /// @p csvPath and stores it with the file, as SimulationOptions::setDragLookupCsvPath() or
    /// setStabilityLookupCsvPath() does, but for a table that holds a number that is not
    /// finite or lies outside its range, which is not stored. Fails with the reader's Error, or
    /// with ErrorCode::PARSE and refusedNumberReason() for such a table; nothing changes then.
    /// @p csvPath is read as it is
    /// given: for a file that a design names, hand in what fileOfDesign() gives, and nothing
    /// that it refuses. The conditions handler reads the legacy <draglookupcsv> and
    /// <stabilitylookupcsv> elements with the two.
    [[nodiscard]] static Result<void> loadFromFile(SimulationOptions&           options,
                                                   const std::filesystem::path& csvPath,
                                                   bool                         isDrag);

    /// Why the loader does not take @p table: "Illegal numeric value '<number>' in column
    /// '<column>'" for its first number that is not finite or that lies outside the range a
    /// simulation holds it to (SimulationInputRanges::findRefusedLookupNumber(); the columns
    /// of a Mach number and of an angle of attack are "mach" and "aoa"), or nullopt for a
    /// table that a simulation may read.
    [[nodiscard]] static std::optional<std::string> refusedNumberReason(const MachAoALookup& table);

    /// The separator of CSV rows, told from the first of @p rows (detectSeparator(); see the
    /// class comment); a comma without rows.
    [[nodiscard]] static char detectSeparator(std::span<const std::string> rows) noexcept;

private:
    SimulationOptions*                   m_options;
    std::vector<std::string>             m_requiredColumns;
    bool                                 m_isDrag;
    std::optional<std::filesystem::path> m_designDirectory;
    std::vector<std::string>             m_rows;
};

}  // namespace QtRocket
