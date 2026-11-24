#pragma once

#include <cstddef>
#include <string_view>

#include "QtRocket/file/CsvExport.h"
#include "QtRocket/gui/util/CsvOptionValues.h"

namespace QtRocket
{
class FlightDataType;
class Preferences;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The stored values of OpenRocket's CSV option panel (components/CsvOptionPanel: what its
/// constructor reads from the preferences and storePreferences() writes back) and the export
/// tab's ticks of the flight data types (SwingPreferences.isFlightDataTypeExportSelected() and
/// setFlightDataTypeExportSelected(), SwingPreferences.java:846-854), with the keys and stored
/// texts of OpenRocket:
/// - "ExportFieldSeparator" (the core's PreferenceKeys::kExportFieldSeparator): the separator
///   itself, "," by default (FieldSeparatorComboBox.loadFromExportPreferences());
/// - "ExportDecimalPlaces": an int, TextUtil.DEFAULT_DECIMAL_PLACES (3) by default;
/// - "ExportExponentialNotation": a boolean, true by default;
/// - "ExportCommentCharacter": the comment starter, "#" by default;
/// - each comment option of the panel of the class B (its getSimpleName(), the base class
///   name: "SimulationExportPanel", "SimulationTableCSVExport") under "csvOptions.B.<index>"
///   (GuiPreferenceKeys::csvOptionKey()), a boolean, true by default;
/// - each flight data type's tick in the node "exports" (GuiPreferenceKeys::kExportsNode) under
///   the type's English name (FlightDataType::getName(), Java's getName()), false by default.
/// The first four are shared by every panel: the simulation data's export, the simulation
/// table's, and the field separator of the wind levels' CSV import (Java:
/// FieldSeparatorComboBox.loadFromExportPreferences()).
///
/// Reading is as the panel's constructor reads (load()): every value is the stored one, or the
/// default when the key is absent or does not parse (Java's getInt() and getBoolean()), with the
/// two exceptions of the panel's field separator box: an empty stored separator reads as ","
/// (FieldSeparatorComboBox.loadFromPreferences() leaves its first item, ",", selected), and a
/// stored text that is the box's SPACE or TAB item text is returned as it is (only written by
/// hand: QtRocket stores the separator itself), which the panel then shows as that item, meaning
/// the space or the tab.
///
/// Deviations from OpenRocket (OpenRocket bugs, numbered as plan 11.4 and docs/gui-differences.md
/// number them):
/// - Bug 50: a stored separator of several characters ("<->") reads whole; Java's
///   FieldSeparatorComboBox.loadFromPreferences() keeps its first character ("<"), so that the
///   next export writes another separator than the one the user chose and stored. (The panel's
///   box shows the whole text: FieldSeparatorComboBox::loadFromPreferences().)
/// - Bug 51: a stored number of decimal places outside 0..15 (kMinDecimalPlaces, kMaxDecimalPlaces,
///   the panel's spinner) reads clamped to that range ("-1" as 0, "16" as 15); Java's
///   SpinnerNumberModel throws an IllegalArgumentException for it in the panel's constructor, so
///   that the simulation dialog with the export tab could no longer be opened.
/// - Bug 52: a key of any length is stored and read. java.util.prefs refuses a key longer than 80
///   characters (Preferences.MAX_KEY_LENGTH: put() throws an IllegalArgumentException, get()
///   gives the default), so that ticking a flight data type whose name is longer (a custom
///   expression's, a file's unknown type) made OpenRocket's export throw after the file question,
///   before anything was written.
class CsvOptionPreferences final
{
public:
    CsvOptionPreferences() = delete;

    /// The base class name of the simulation data export's panel (SimulationExportPanel.class's
    /// getSimpleName()), whose three comment options are kSimulationCommentsOption,
    /// kFieldDescriptionsOption and kFlightEventsOption.
    static constexpr std::string_view kSimulationExportPanel = "SimulationExportPanel";
    /// The base class name of the simulation table's CSV export (SimulationTableCSVExport.class),
    /// which has no comment options.
    static constexpr std::string_view kSimulationTableCsvExport = "SimulationTableCSVExport";

    /// SimulationExportPanel.OPTION_SIMULATION_COMMENTS: "Include simulation description".
    static constexpr std::size_t kSimulationCommentsOption = 0;
    /// SimulationExportPanel.OPTION_FIELD_DESCRIPTIONS: "Include field descriptions".
    static constexpr std::size_t kFieldDescriptionsOption = 1;
    /// SimulationExportPanel.OPTION_FLIGHT_EVENTS: "Include flight events".
    static constexpr std::size_t kFlightEventsOption = 2;
    /// The number of the simulation data export's comment options.
    static constexpr std::size_t kSimulationExportOptionCount = 3;

    /// The range of the decimal places (the panel's SpinnerNumberModel(value, 0, 15, 1)).
    static constexpr int kMinDecimalPlaces = 0;
    static constexpr int kMaxDecimalPlaces = 15;

    /// The values stored in @p preferences for the panel of @p baseClassName with
    /// @p optionCount comment options (see the class comment: the defaults, the separator and the
    /// clamped decimal places).
    [[nodiscard]] static CsvOptionValues load(const Preferences& preferences,
                                              std::string_view   baseClassName,
                                              std::size_t        optionCount);

    /// CsvOptionPanel.storePreferences(): stores @p values, the separator as it is (" " and "\t"
    /// for SPACE and TAB) and each of its comment options under the key of @p baseClassName.
    static void store(Preferences& preferences, std::string_view baseClassName,
                      const CsvOptionValues& values);

    /// The core export's options for @p values of the simulation data export (Java:
    /// SimulationExportPanel.doExport()'s reading of its panel): the separator, the decimal
    /// places, the notation, the comment starter and the three comment options.
    /// @throws BugError when @p values does not have kSimulationExportOptionCount comment options
    [[nodiscard]] static CsvExport::Options toCsvExportOptions(const CsvOptionValues& values);

    /// SwingPreferences.isFlightDataTypeExportSelected(): whether the type named @p typeName
    /// (FlightDataType::getName()) is ticked in the export tab, from the node "exports" (unticked
    /// when the node or the key is absent; never creates the node).
    [[nodiscard]] static bool isFlightDataTypeExportSelected(const Preferences& preferences,
                                                             std::string_view   typeName);
    [[nodiscard]] static bool isFlightDataTypeExportSelected(const Preferences&    preferences,
                                                             const FlightDataType& type);

    /// SwingPreferences.setFlightDataTypeExportSelected(): stores whether the type named
    /// @p typeName is ticked, in the node "exports" (created).
    static void setFlightDataTypeExportSelected(Preferences& preferences, std::string_view typeName,
                                                bool selected);
    static void setFlightDataTypeExportSelected(Preferences&          preferences,
                                                const FlightDataType& type, bool selected);
};

}  // namespace QtRocket::Gui
