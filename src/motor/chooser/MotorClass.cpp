#include "QtRocket/motor/chooser/MotorClass.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <string>
#include <string_view>

#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// A class's names and range (MotorClass's constructor arguments).
struct MotorClassData
{
    std::string_view enumName;
    std::string_view className;
    double           min;
    double           max;
};

/// Indexed by the enum's declaration order (MotorClass.java:13-31).
constexpr std::array<MotorClassData, kAllMotorClasses.size()> kClasses{{
    {.enumName = "A18", .className = "1/8A", .min = 0, .max = 0.3125},
    {.enumName = "A14", .className = "1/4A", .min = 0.3125, .max = 0.625},
    {.enumName = "A12", .className = "1/2A", .min = 0.625, .max = 1.25},
    {.enumName = "A", .className = "A", .min = 1.25, .max = 2.5},
    {.enumName = "B", .className = "B", .min = 2.5, .max = 5},
    {.enumName = "C", .className = "C", .min = 5, .max = 10},
    {.enumName = "D", .className = "D", .min = 10, .max = 20},
    {.enumName = "E", .className = "E", .min = 20, .max = 40},
    {.enumName = "F", .className = "F", .min = 40, .max = 80},
    {.enumName = "G", .className = "G", .min = 80, .max = 160},
    {.enumName = "H", .className = "H", .min = 160, .max = 320},
    {.enumName = "I", .className = "I", .min = 320, .max = 640},
    {.enumName = "J", .className = "J", .min = 640, .max = 1280},
    {.enumName = "K", .className = "K", .min = 1280, .max = 2560},
    {.enumName = "L", .className = "L", .min = 2560, .max = 5120},
    {.enumName = "M", .className = "M", .min = 5120, .max = 10240},
    {.enumName = "N", .className = "N", .min = 10240, .max = 20480},
    {.enumName = "O", .className = "O", .min = 20480, .max = 40960},
    {.enumName  = "OVER",
     .className = "> O",
     .min       = 40960,
     .max       = std::numeric_limits<double>::max()},
}};

/// getMotorClass()'s tolerance on a class's upper end.
constexpr double kEpsilon = 0.0000001;

/// The impulse from which getMotorClass() rounds to whole Ns.
constexpr double kRoundingImpulse = 100;

[[nodiscard]] const MotorClassData& data(MotorClass motorClass)
{
    const auto index = static_cast<std::size_t>(motorClass);
    QTROCKET_ASSERT(index < kClasses.size());
    return kClasses.at(index);
}

[[nodiscard]] const UnitGroup& impulseUnits()
{
    return unitGroup(UnitGroupId::IMPULSE);
}

}  // namespace

std::string_view enumName(MotorClass motorClass)
{
    return data(motorClass).enumName;
}

std::string_view className(MotorClass motorClass)
{
    return data(motorClass).className;
}

double getMin(MotorClass motorClass)
{
    return data(motorClass).min;
}

double getMax(MotorClass motorClass)
{
    return data(motorClass).max;
}

std::int64_t getPercent(MotorClass motorClass, double impulse)
{
    const MotorClassData& range = data(motorClass);
    // 0% looks stupid: below 1 is 1 (Java's "if (percent < 1) percent = 1", which keeps NaN, as
    // Math.max does).
    const double percent =
        MathUtil::javaMax(((impulse - range.min) / (range.max - range.min)) * 100, 1.0);
    return MathUtil::javaRound(percent);
}

std::string getDescription(MotorClass motorClass, double impulse)
{
    if (motorClass == MotorClass::OVER)
    {
        return "Over O";
    }
    return std::format("{}% {}", getPercent(motorClass, impulse), className(motorClass));
}

std::string getClassDescription(MotorClass motorClass)
{
    if (motorClass == MotorClass::OVER)
    {
        return std::format("Over O-class (over {})", impulseUnits().toStringUnit(40960));
    }
    const MotorClassData& range = data(motorClass);
    return std::format("Class {} ({} - {})", range.className,
                       impulseUnits().toStringUnit(range.min),
                       impulseUnits().toStringUnit(range.max));
}

MotorClass getMotorClass(double impulse)
{
    // Round large values so 640.1 Ns (which is displayed as 640 Ns) is counted as I-class
    if (impulse >= kRoundingImpulse)
    {
        impulse = std::nearbyint(impulse);
    }
    for (const MotorClass motorClass : kAllMotorClasses)
    {
        if (impulse <= getMax(motorClass) + kEpsilon)
        {
            return motorClass;
        }
    }
    bug("Could not find motor class for impulse " + Strings::javaDoubleToString(impulse));
}

}  // namespace QtRocket
