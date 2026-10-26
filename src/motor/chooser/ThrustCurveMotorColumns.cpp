#include "QtRocket/motor/chooser/ThrustCurveMotorColumns.h"

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

#include "QtRocket/motor/DesignationComparator.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/unit/Value.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/JavaCollator.h"

namespace QtRocket
{

namespace
{

/// The width OpenRocket gives a column unless it says otherwise.
constexpr int kDefaultWidth = 50;

/// The width of the manufacturer column.
constexpr int kManufacturerWidth = 100;

/// The manufacturer of @p set, which a set has once it holds a motor.
[[nodiscard]] const Manufacturer& manufacturerOf(const ThrustCurveMotorSet& set)
{
    const Manufacturer* manufacturer = set.getManufacturer();
    QTROCKET_ASSERT(manufacturer != nullptr);
    return *manufacturer;
}

/// The NAME column's value: the designation or the common name, as the name column says.
[[nodiscard]] const std::string& nameOf(const ThrustCurveMotorSet& set,
                                        const Preferences&         preferences)
{
    return preferences.getMotorNameColumn() ? set.getDesignation() : set.getCommonName();
}

/// A dimension as the DIAMETER and LENGTH columns hold it: a Value in the motor dimension unit
/// group's default unit.
[[nodiscard]] Value dimension(double value)
{
    return {value, unitGroup(UnitGroupId::MOTOR_DIMENSIONS)};
}

/// Long.compareTo.
[[nodiscard]] int compareLongs(std::int64_t a, std::int64_t b) noexcept
{
    if (a < b)
    {
        return -1;
    }
    return a > b ? 1 : 0;
}

}  // namespace

std::string_view getTitleKey(ThrustCurveMotorColumns column)
{
    switch (column)
    {
        case ThrustCurveMotorColumns::MANUFACTURER:
            return "TCurveMotorCol.MANUFACTURER";
        case ThrustCurveMotorColumns::NAME:
            return "TCurveMotorCol.NAME";
        case ThrustCurveMotorColumns::TOTAL_IMPULSE:
            return "TCurveMotorCol.TOTAL_IMPULSE";
        case ThrustCurveMotorColumns::CASEINFO:
            return "TCurveMotorCol.CASEINFO";
        case ThrustCurveMotorColumns::DIAMETER:
            return "TCurveMotorCol.DIAMETER";
        case ThrustCurveMotorColumns::LENGTH:
            return "TCurveMotorCol.LENGTH";
    }
    QTROCKET_UNREACHABLE();
}

int getWidth(ThrustCurveMotorColumns column)
{
    switch (column)
    {
        case ThrustCurveMotorColumns::MANUFACTURER:
            return kManufacturerWidth;
        case ThrustCurveMotorColumns::NAME:
        case ThrustCurveMotorColumns::TOTAL_IMPULSE:
        case ThrustCurveMotorColumns::CASEINFO:
        case ThrustCurveMotorColumns::DIAMETER:
        case ThrustCurveMotorColumns::LENGTH:
            return kDefaultWidth;
    }
    QTROCKET_UNREACHABLE();
}

std::string getValueText(ThrustCurveMotorColumns column, const ThrustCurveMotorSet& set,
                         const Preferences& preferences)
{
    // Every column's getValue() needs the manufacturer in OpenRocket's model (an empty set is a
    // NullPointerException there): refuse an empty set in every column alike.
    const Manufacturer& manufacturer = manufacturerOf(set);
    switch (column)
    {
        case ThrustCurveMotorColumns::MANUFACTURER:
            return manufacturer.getDisplayName();
        case ThrustCurveMotorColumns::NAME:
            return nameOf(set, preferences);
        case ThrustCurveMotorColumns::TOTAL_IMPULSE:
            return std::format("{}", set.getTotalImpulse());
        case ThrustCurveMotorColumns::CASEINFO:
            return set.getCaseInfo();
        case ThrustCurveMotorColumns::DIAMETER:
            return dimension(set.getDiameter()).toString();
        case ThrustCurveMotorColumns::LENGTH:
            return dimension(set.getLength()).toString();
    }
    QTROCKET_UNREACHABLE();
}

int compareValues(ThrustCurveMotorColumns column, const ThrustCurveMotorSet& a,
                  const ThrustCurveMotorSet& b, const Preferences& preferences)
{
    const Manufacturer& manufacturerA = manufacturerOf(a);
    const Manufacturer& manufacturerB = manufacturerOf(b);
    switch (column)
    {
        case ThrustCurveMotorColumns::MANUFACTURER:
            return JavaCollator::compare(manufacturerA.getDisplayName(),
                                         manufacturerB.getDisplayName());
        case ThrustCurveMotorColumns::NAME:
            return DesignationComparator::compare(nameOf(a, preferences), nameOf(b, preferences));
        case ThrustCurveMotorColumns::TOTAL_IMPULSE:
            return compareLongs(a.getTotalImpulse(), b.getTotalImpulse());
        case ThrustCurveMotorColumns::CASEINFO:
            return JavaCollator::compare(a.getCaseInfo(), b.getCaseInfo());
        case ThrustCurveMotorColumns::DIAMETER:
            return dimension(a.getDiameter()).compareTo(dimension(b.getDiameter()));
        case ThrustCurveMotorColumns::LENGTH:
            return dimension(a.getLength()).compareTo(dimension(b.getLength()));
    }
    QTROCKET_UNREACHABLE();
}

}  // namespace QtRocket
