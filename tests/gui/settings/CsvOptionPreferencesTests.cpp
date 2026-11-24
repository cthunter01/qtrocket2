// The stored values of OpenRocket's CSV option panel and the export tab's ticks, with
// OpenRocket's keys and texts (the export scout's Swing probe of CsvOptionPanel, swing-probe.txt:
// "PREF ..." and "DECIMALS stored ..." lines) and the fixes of OpenRocket's bugs 50, 51 and 52.

#include "QtRocket/gui/settings/CsvOptionPreferences.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/file/CsvExport.h"
#include "QtRocket/gui/settings/SettingsPreferences.h"
#include "QtRocket/gui/util/CsvOptionValues.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/util/BugError.h"
#include "TestTempDir.h"

namespace QtRocket::Gui
{

namespace
{

using CsvOptionPreferencesTest = Test::GuiTest;

/// The simulation data export's base class name.
constexpr std::string_view kExportPanel = CsvOptionPreferences::kSimulationExportPanel;

/// The values of the export tab when nothing is stored.
[[nodiscard]] CsvOptionValues exportDefaults()
{
    CsvOptionValues values;
    values.commentOptions = {true, true, true};
    return values;
}

/// The value stored under @p key of @p preferences, or "<absent>".
[[nodiscard]] std::string stored(const Preferences& preferences, std::string_view key)
{
    return preferences.get(key).value_or("<absent>");
}

/// The field separator read back from a store holding @p text.
[[nodiscard]] QString separatorRead(std::string_view text)
{
    InMemoryPreferences preferences;
    preferences.put("ExportFieldSeparator", text);
    return CsvOptionPreferences::load(preferences, kExportPanel, 3).fieldSeparator;
}

/// The decimal places read back from a store holding @p text.
[[nodiscard]] int decimalPlacesRead(std::string_view text)
{
    InMemoryPreferences preferences;
    preferences.put("ExportDecimalPlaces", text);
    return CsvOptionPreferences::load(preferences, kExportPanel, 3).decimalPlaces;
}

}  // namespace

TEST_F(CsvOptionPreferencesTest, AnEmptyStoreGivesThePanelsDefaults)
{
    // CsvOptionPanel: ",", TextUtil.DEFAULT_DECIMAL_PLACES, exponential, "#", every option ticked.
    const InMemoryPreferences preferences;
    EXPECT_EQ(CsvOptionPreferences::load(preferences, kExportPanel, 3), exportDefaults());
    EXPECT_EQ(CsvOptionPreferences::load(preferences, kExportPanel, 3).decimalPlaces, 3);
    // The simulation table's export has no comment options.
    EXPECT_TRUE(
        CsvOptionPreferences::load(preferences, CsvOptionPreferences::kSimulationTableCsvExport, 0)
            .commentOptions.empty());
}

TEST_F(CsvOptionPreferencesTest, StoresUnderOpenRocketsKeysWithItsTexts)
{
    // The Swing probe's store after the separator "x" and the comment character "//" (its
    // "PREF ..." lines).
    InMemoryPreferences preferences;
    CsvOptionValues     values = exportDefaults();
    values.fieldSeparator      = QStringLiteral("x");
    values.commentCharacter    = QStringLiteral("//");
    CsvOptionPreferences::store(preferences, kExportPanel, values);
    EXPECT_EQ(stored(preferences, "ExportCommentCharacter"), "//");
    EXPECT_EQ(stored(preferences, "ExportDecimalPlaces"), "3");
    EXPECT_EQ(stored(preferences, "ExportExponentialNotation"), "true");
    EXPECT_EQ(stored(preferences, "ExportFieldSeparator"), "x");
    EXPECT_EQ(stored(preferences, "csvOptions.SimulationExportPanel.0"), "true");
    EXPECT_EQ(stored(preferences, "csvOptions.SimulationExportPanel.1"), "true");
    EXPECT_EQ(stored(preferences, "csvOptions.SimulationExportPanel.2"), "true");
    EXPECT_EQ(preferences.keys().size(), 7U);
    EXPECT_TRUE(preferences.childrenNames().empty());
    EXPECT_EQ(CsvOptionPreferences::load(preferences, kExportPanel, 3), values);

    // Each panel its own comment options; the format settings are shared.
    CsvOptionValues other;
    other.fieldSeparator      = QStringLiteral(";");
    other.decimalPlaces       = 15;
    other.exponentialNotation = false;
    other.commentCharacter    = QStringLiteral("%");
    other.commentOptions      = {false, true};
    CsvOptionPreferences::store(preferences, "CAExportPanel", other);
    EXPECT_EQ(stored(preferences, "csvOptions.CAExportPanel.0"), "false");
    EXPECT_EQ(stored(preferences, "csvOptions.CAExportPanel.1"), "true");
    EXPECT_EQ(stored(preferences, "ExportDecimalPlaces"), "15");
    EXPECT_EQ(stored(preferences, "ExportExponentialNotation"), "false");
    EXPECT_EQ(CsvOptionPreferences::load(preferences, "CAExportPanel", 2), other);
    EXPECT_EQ(CsvOptionPreferences::load(preferences, kExportPanel, 3).commentOptions,
              (std::vector<bool>{true, true, true}));
}

TEST_F(CsvOptionPreferencesTest, TheSeparatorIsStoredAndReadAsItself)
{
    // The Swing probe's "SEPARATOR stored=..." cases as the store reads them: the separator
    // itself, a space and a tab as such (never "SPACE" or "TAB": storePreferences() writes
    // getSeparatorString()).
    EXPECT_EQ(separatorRead(","), QStringLiteral(","));
    EXPECT_EQ(separatorRead(";"), QStringLiteral(";"));
    EXPECT_EQ(separatorRead(" "), QStringLiteral(" "));
    EXPECT_EQ(separatorRead("\t"), QStringLiteral("\t"));
    EXPECT_EQ(separatorRead("x"), QStringLiteral("x"));
    // Bug 50: a separator of several characters whole (Java's box keeps "<").
    EXPECT_EQ(separatorRead("<->"), QStringLiteral("<->"));
    // An empty one leaves the box at ","; a hand-written "SPACE" stays the text, which the
    // panel's box shows as its SPACE item.
    EXPECT_EQ(separatorRead(""), QStringLiteral(","));
    EXPECT_EQ(separatorRead("SPACE"), QStringLiteral("SPACE"));
    // Not UTF-8 by hand: what QtRocket reads as text.
    EXPECT_EQ(separatorRead("\xC2\xA7"), QStringLiteral(u"\u00A7"));

    InMemoryPreferences preferences;
    CsvOptionValues     values = exportDefaults();
    values.fieldSeparator      = QStringLiteral("\t");
    CsvOptionPreferences::store(preferences, kExportPanel, values);
    EXPECT_EQ(stored(preferences, "ExportFieldSeparator"), "\t");
}

TEST_F(CsvOptionPreferencesTest, StoredDecimalPlacesAreClampedToTheSpinnersRange)
{
    // Bug 51: the Swing probe's "DECIMALS stored -1 / 16 / 20 -> IllegalArgumentException".
    EXPECT_EQ(decimalPlacesRead("-1"), 0);
    EXPECT_EQ(decimalPlacesRead("0"), 0);
    EXPECT_EQ(decimalPlacesRead("15"), 15);
    EXPECT_EQ(decimalPlacesRead("16"), 15);
    EXPECT_EQ(decimalPlacesRead("20"), 15);
    EXPECT_EQ(decimalPlacesRead("-2147483648"), 0);
    // Not an int: Java's getInt() gives the default.
    EXPECT_EQ(decimalPlacesRead("three"), 3);
    EXPECT_EQ(decimalPlacesRead("2.5"), 3);
}

TEST_F(CsvOptionPreferencesTest, TheOtherValuesAreReadAsJavaReadsThem)
{
    InMemoryPreferences preferences;
    // The Swing probe's "COMMENT '//' -> '//'": any comment starter, the empty one too.
    preferences.put("ExportCommentCharacter", "//");
    EXPECT_EQ(CsvOptionPreferences::load(preferences, kExportPanel, 3).commentCharacter,
              QStringLiteral("//"));
    preferences.put("ExportCommentCharacter", "");
    EXPECT_EQ(CsvOptionPreferences::load(preferences, kExportPanel, 3).commentCharacter, QString());
    // java.util.prefs' getBoolean(): "true" or "false" without regard to case, else the default.
    preferences.put("ExportExponentialNotation", "FALSE");
    preferences.put("csvOptions.SimulationExportPanel.0", "False");
    preferences.put("csvOptions.SimulationExportPanel.1", "no");
    preferences.put("csvOptions.SimulationExportPanel.2", "false");
    const CsvOptionValues values = CsvOptionPreferences::load(preferences, kExportPanel, 3);
    EXPECT_FALSE(values.exponentialNotation);
    EXPECT_EQ(values.commentOptions, (std::vector<bool>{false, true, false}));
}

TEST_F(CsvOptionPreferencesTest, TheValuesGoThroughThePreferencesFile)
{
    // The separators and starters that an INI file could lose (a space, a tab, a comma, a
    // semicolon) come back from the file.
    const QtRocket::Test::TempDir directory;
    const QString                 file = QtStrings::fromPath(directory.resolve("prefs.ini"));
    for (const QString& separator :
         {QStringLiteral(" "), QStringLiteral("\t"), QStringLiteral(","), QStringLiteral(";"),
          QStringLiteral("<->"), QStringLiteral(" ; ")})
    {
        CsvOptionValues values  = exportDefaults();
        values.fieldSeparator   = separator;
        values.commentCharacter = QStringLiteral(";");
        {
            SettingsPreferences preferences(file);
            CsvOptionPreferences::store(preferences, kExportPanel, values);
            ASSERT_TRUE(preferences.sync().has_value());
        }
        const SettingsPreferences reopened(file);
        EXPECT_EQ(CsvOptionPreferences::load(reopened, kExportPanel, 3), values)
            << QtStrings::toUtf8(separator);
    }
}

TEST_F(CsvOptionPreferencesTest, TheCoresExportTakesTheExportTabsValues)
{
    // SimulationExportPanel.doExport(): its panel's values and options 0, 1 and 2.
    CsvOptionValues values;
    values.fieldSeparator            = QStringLiteral("\t");
    values.decimalPlaces             = 15;
    values.exponentialNotation       = false;
    values.commentCharacter          = QStringLiteral("!!");
    values.commentOptions            = {false, true, false};
    const CsvExport::Options options = CsvOptionPreferences::toCsvExportOptions(values);
    EXPECT_EQ(options.fieldSeparator, "\t");
    EXPECT_EQ(options.decimalPlaces, 15);
    EXPECT_FALSE(options.exponentialNotation);
    EXPECT_EQ(options.commentStarter, "!!");
    EXPECT_FALSE(options.simulationComments);
    EXPECT_TRUE(options.fieldComments);
    EXPECT_FALSE(options.eventComments);

    // The defaults are the core's (the dialog's defaults).
    const CsvExport::Options defaults = CsvOptionPreferences::toCsvExportOptions(exportDefaults());
    const CsvExport::Options reference;
    EXPECT_EQ(defaults.fieldSeparator, reference.fieldSeparator);
    EXPECT_EQ(defaults.decimalPlaces, reference.decimalPlaces);
    EXPECT_EQ(defaults.exponentialNotation, reference.exponentialNotation);
    EXPECT_EQ(defaults.commentStarter, reference.commentStarter);
    EXPECT_EQ(defaults.simulationComments, reference.simulationComments);
    EXPECT_EQ(defaults.fieldComments, reference.fieldComments);
    EXPECT_EQ(defaults.eventComments, reference.eventComments);

    // Not the export tab's three options.
    const CsvOptionValues tableExport;
    EXPECT_THROW(static_cast<void>(CsvOptionPreferences::toCsvExportOptions(tableExport)),
                 BugError);
}

TEST_F(CsvOptionPreferencesTest, TheExportTabsTicksAreInTheNodeExports)
{
    // SwingPreferences.isFlightDataTypeExportSelected(): node "exports", the type's name,
    // false by default (the Swing probe: "EXPORTS NODE KEYS (none selected yet): 0").
    InMemoryPreferences   preferences;
    const FlightDataType& altitude = FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE);
    EXPECT_FALSE(CsvOptionPreferences::isFlightDataTypeExportSelected(preferences, altitude));
    // Reading creates no node.
    EXPECT_EQ(preferences.findNode("exports"), nullptr);

    CsvOptionPreferences::setFlightDataTypeExportSelected(preferences, altitude, true);
    CsvOptionPreferences::setFlightDataTypeExportSelected(
        preferences, FlightDataType::builtin(FlightDataTypeId::TYPE_TIME), false);
    const Preferences* const exports = preferences.findNode("exports");
    ASSERT_NE(exports, nullptr);
    EXPECT_EQ(stored(*exports, "Altitude"), "true");
    EXPECT_EQ(stored(*exports, "Time"), "false");
    EXPECT_EQ(exports->keys(), (std::vector<std::string>{"Altitude", "Time"}));
    EXPECT_TRUE(preferences.keys().empty());
    EXPECT_TRUE(CsvOptionPreferences::isFlightDataTypeExportSelected(preferences, altitude));
    EXPECT_TRUE(CsvOptionPreferences::isFlightDataTypeExportSelected(preferences, "Altitude"));
    EXPECT_FALSE(CsvOptionPreferences::isFlightDataTypeExportSelected(preferences, "altitude"));
}

TEST_F(CsvOptionPreferencesTest, AKeyLongerThanJavasLimitIsStoredAndRead)
{
    // Bug 52: java.util.prefs refuses keys of more than 80 characters (MAX_KEY_LENGTH), so that
    // OpenRocket's export threw for such a type's name; QtRocket has no limit (also in its file).
    const std::string name(81, 'x');
    const std::string longer =
        "Custom expression with a long name: the drag force over the dynamic pressure / area " +
        name;
    InMemoryPreferences memory;
    CsvOptionPreferences::setFlightDataTypeExportSelected(memory, longer, true);
    EXPECT_TRUE(CsvOptionPreferences::isFlightDataTypeExportSelected(memory, longer));

    const QtRocket::Test::TempDir directory;
    const QString                 file = QtStrings::fromPath(directory.resolve("prefs.ini"));
    {
        SettingsPreferences preferences(file);
        CsvOptionPreferences::setFlightDataTypeExportSelected(preferences, longer, true);
        ASSERT_TRUE(preferences.sync().has_value());
    }
    const SettingsPreferences reopened(file);
    EXPECT_TRUE(CsvOptionPreferences::isFlightDataTypeExportSelected(reopened, longer));
}

}  // namespace QtRocket::Gui
