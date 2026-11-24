#include "QtRocket/gui/settings/CsvOptionPreferences.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

#include <QString>

#include "QtRocket/file/CsvExport.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/CsvOptionValues.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The key of the comment option @p index of the panel of @p baseClassName.
[[nodiscard]] std::string optionKey(std::string_view baseClassName, std::size_t index)
{
    return GuiPreferenceKeys::csvOptionKey(baseClassName, static_cast<int>(index));
}

}  // namespace

CsvOptionValues CsvOptionPreferences::load(const Preferences& preferences,
                                           std::string_view baseClassName, std::size_t optionCount)
{
    const CsvOptionValues defaults;
    CsvOptionValues       values;
    // FieldSeparatorComboBox.loadFromExportPreferences(): an empty text leaves the box at its
    // first item; any other is the separator, whole (bug 50: Java keeps its first character).
    const std::string separator = preferences.getString(PreferenceKeys::kExportFieldSeparator,
                                                        QtStrings::toUtf8(defaults.fieldSeparator));
    values.fieldSeparator =
        separator.empty() ? defaults.fieldSeparator : QtStrings::fromUtf8(separator);
    // Bug 51: clamped to the spinner's range, where Java's SpinnerNumberModel throws.
    values.decimalPlaces =
        std::clamp(preferences.getInt(PreferenceKeys::kExportDecimalPlaces, defaults.decimalPlaces),
                   kMinDecimalPlaces, kMaxDecimalPlaces);
    values.exponentialNotation = preferences.getBoolean(PreferenceKeys::kExportExponentialNotation,
                                                        defaults.exponentialNotation);
    values.commentCharacter    = QtStrings::fromUtf8(preferences.getString(
        PreferenceKeys::kExportCommentCharacter, QtStrings::toUtf8(defaults.commentCharacter)));
    values.commentOptions.reserve(optionCount);
    for (std::size_t index = 0; index < optionCount; ++index)
    {
        values.commentOptions.push_back(preferences.getBoolean(
            optionKey(baseClassName, index), GuiPreferenceKeys::kCsvOptionDefault));
    }
    return values;
}

void CsvOptionPreferences::store(Preferences& preferences, std::string_view baseClassName,
                                 const CsvOptionValues& values)
{
    preferences.putString(PreferenceKeys::kExportFieldSeparator,
                          QtStrings::toUtf8(values.fieldSeparator));
    preferences.putInt(PreferenceKeys::kExportDecimalPlaces, values.decimalPlaces);
    preferences.putBoolean(PreferenceKeys::kExportExponentialNotation, values.exponentialNotation);
    preferences.putString(PreferenceKeys::kExportCommentCharacter,
                          QtStrings::toUtf8(values.commentCharacter));
    for (std::size_t index = 0; index < values.commentOptions.size(); ++index)
    {
        preferences.putBoolean(optionKey(baseClassName, index), values.commentOptions[index]);
    }
}

CsvExport::Options CsvOptionPreferences::toCsvExportOptions(const CsvOptionValues& values)
{
    if (values.commentOptions.size() != kSimulationExportOptionCount)
    {
        bug("The simulation data export has three comment options");
    }
    return CsvExport::Options{
        .fieldSeparator      = QtStrings::toUtf8(values.fieldSeparator),
        .decimalPlaces       = values.decimalPlaces,
        .exponentialNotation = values.exponentialNotation,
        .commentStarter      = QtStrings::toUtf8(values.commentCharacter),
        .simulationComments  = values.commentOptions[kSimulationCommentsOption],
        .fieldComments       = values.commentOptions[kFieldDescriptionsOption],
        .eventComments       = values.commentOptions[kFlightEventsOption],
    };
}

bool CsvOptionPreferences::isFlightDataTypeExportSelected(const Preferences& preferences,
                                                          std::string_view   typeName)
{
    const Preferences* const exports = preferences.findNode(GuiPreferenceKeys::kExportsNode);
    return exports != nullptr
               ? exports->getBoolean(typeName, GuiPreferenceKeys::kExportSelectedDefault)
               : GuiPreferenceKeys::kExportSelectedDefault;
}

bool CsvOptionPreferences::isFlightDataTypeExportSelected(const Preferences&    preferences,
                                                          const FlightDataType& type)
{
    return isFlightDataTypeExportSelected(preferences, type.getName());
}

void CsvOptionPreferences::setFlightDataTypeExportSelected(Preferences&     preferences,
                                                           std::string_view typeName, bool selected)
{
    preferences.getNode(GuiPreferenceKeys::kExportsNode).putBoolean(typeName, selected);
}

void CsvOptionPreferences::setFlightDataTypeExportSelected(Preferences&          preferences,
                                                           const FlightDataType& type,
                                                           bool                  selected)
{
    setFlightDataTypeExportSelected(preferences, type.getName(), selected);
}

}  // namespace QtRocket::Gui
