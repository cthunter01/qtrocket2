#include "QtRocket/motor/chooser/MotorFilterSliders.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket::MotorFilterSliders
{

namespace
{

/// How far below a stop's actual diameter a mount's diameter must lie for the stop not to fit.
constexpr double kMountDiameterSlack = 0.0005;

/// The length slider's mm per m.
constexpr double kMillimetresPerMetre = 1000;

[[nodiscard]] const MotorDiameter& stopAt(std::size_t stop)
{
    QTROCKET_ASSERT(stop < kMotorDiameters.size());
    return kMotorDiameters.at(stop);
}

}  // namespace

std::vector<std::string> diameterLabels()
{
    const Unit&              unit = unitGroup(UnitGroupId::MOTOR_DIMENSIONS).getDefaultUnit();
    std::vector<std::string> labels;
    labels.reserve(kMotorDiameters.size());
    for (const MotorDiameter& stop : kMotorDiameters)
    {
        // Round the labels, because for imperial units, the labels can otherwise overlap
        double       diameter        = unit.toUnit(stop.nominal);
        const double diameterRounded = unit.round(diameter * 10) / 10;
        diameter                     = unit.fromUnit(diameterRounded);
        std::string formatted        = unit.toString(diameter);
        // Remove the leading zero for numbers between 0 and 1
        if (diameterRounded > 0 && diameterRounded < 1)
        {
            formatted.erase(0, 1);
        }
        labels.push_back(std::move(formatted));
    }
    labels.back() = "+";
    return labels;
}

double minimumDiameter(std::size_t stop)
{
    return stopAt(stop).actual;
}

std::optional<double> maximumDiameter(std::size_t stop)
{
    const MotorDiameter& diameter = stopAt(stop);
    if (stop == kMotorDiameters.size() - 1)
    {
        return std::nullopt;
    }
    return diameter.actual;
}

int limitDiameterStop(double mountDiameter)
{
    // find the next largest diameter
    std::size_t stop = 0;
    while (stop < kMotorDiameters.size() &&
           !(mountDiameter < kMotorDiameters.at(stop).actual - kMountDiameterSlack))
    {
        stop++;
    }
    if (stop >= kMotorDiameters.size())
    {
        stop--;
    }
    return static_cast<int>(stop) - 1;
}

int limitLengthStop(double mountLength)
{
    // (int) Math.min(1000, Math.round(1000 * mountLength)): Java's (int) of a long keeps the low
    // 32 bits, as the conversion does in C++20.
    const std::int64_t rounded = MathUtil::javaRound(kMillimetresPerMetre * mountLength);
    return static_cast<int>(std::min<std::int64_t>(kLengthSliderMaximum, rounded));
}

}  // namespace QtRocket::MotorFilterSliders
