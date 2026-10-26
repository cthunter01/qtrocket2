#pragma once

#include <array>
#include <string>
#include <string_view>

namespace QtRocket
{

class Preferences;
class ThrustCurveMotorSet;

/// The columns of the motor chooser's table (OpenRocket's swing enum ThrustCurveMotorColumns,
/// gui/dialogs/motor/thrustcurve/ThrustCurveMotorColumns.java), in declaration order, which is the
/// order of the table's columns. A row is a motor set; each column has a value, shown as its text
/// (getValueText()), and the comparator the table sorts the column with (compareValues()).
///
/// | Column        | Value                                        | Sorted by                     |
/// |---------------|----------------------------------------------|-------------------------------|
/// | MANUFACTURER  | the manufacturer's display name              | JavaCollator (TERTIARY)       |
/// | NAME          | the designation or the common name (*)       | DesignationComparator         |
/// | TOTAL_IMPULSE | ThrustCurveMotorSet::getTotalImpulse(), Ns   | as integers                   |
/// | CASEINFO      | the case info                                | JavaCollator (TERTIARY)       |
/// | DIAMETER      | the diameter as a Value of the motor         | Value::compareTo              |
/// | LENGTH        | the length, likewise                         | Value::compareTo              |
///
/// (*) the designation when Preferences::getMotorNameColumn() says so (the default), else the
/// common name. The two Values are in the default unit of the motor dimension unit group at the
/// time they are made ("29 mm", "1.14 in"), and so is their order: by the unit's name, then by the
/// value in it. The total impulse is written as plain digits, without a unit.
///
/// Java's getToolTipText() is not ported: nothing calls it (the table has no row tooltips).
///
/// The functions below throw BugError for a value outside the enum, and for a set without motors
/// (whose manufacturer is null: a NullPointerException in OpenRocket).
enum class ThrustCurveMotorColumns
{
    MANUFACTURER,
    NAME,
    TOTAL_IMPULSE,
    CASEINFO,
    DIAMETER,
    LENGTH,
};

/// ThrustCurveMotorColumns.values(): the table's columns, in order.
inline constexpr std::array<ThrustCurveMotorColumns, 6> kAllThrustCurveMotorColumns{
    ThrustCurveMotorColumns::MANUFACTURER,  ThrustCurveMotorColumns::NAME,
    ThrustCurveMotorColumns::TOTAL_IMPULSE, ThrustCurveMotorColumns::CASEINFO,
    ThrustCurveMotorColumns::DIAMETER,      ThrustCurveMotorColumns::LENGTH,
};

/// The key of the column's title in OpenRocket's messages.properties (the title field, which
/// getTitle() translates): "TCurveMotorCol.MANUFACTURER" ("Manufacturer"), "TCurveMotorCol.NAME"
/// ("Name"), "TCurveMotorCol.TOTAL_IMPULSE" ("Total Impulse"), "TCurveMotorCol.CASEINFO" ("Case"),
/// "TCurveMotorCol.DIAMETER" ("Diameter"), "TCurveMotorCol.LENGTH" ("Length").
[[nodiscard]] std::string_view getTitleKey(ThrustCurveMotorColumns column);

/// The column's preferred width in pixels (getWidth()): 100 for MANUFACTURER, 50 for the others.
[[nodiscard]] int getWidth(ThrustCurveMotorColumns column);

/// The text of the column's value for @p set (getValue(set).toString()): what the table shows and
/// what the search compares (MotorRowFilter).
[[nodiscard]] std::string getValueText(ThrustCurveMotorColumns    column,
                                       const ThrustCurveMotorSet& set,
                                       const Preferences&         preferences);

/// The column's comparator (getComparator()) applied to the values of @p a and @p b: negative,
/// zero or positive as @p a's value sorts before, with or after @p b's.
[[nodiscard]] int compareValues(ThrustCurveMotorColumns column, const ThrustCurveMotorSet& a,
                                const ThrustCurveMotorSet& b, const Preferences& preferences);

}  // namespace QtRocket
