#include "QtRocket/motor/chooser/ImpulseClass.h"

#include <array>
#include <cstddef>
#include <limits>
#include <string_view>

#include "QtRocket/util/BugError.h"

namespace QtRocket
{

namespace
{

/// A class's name and range (ImpulseClass's constructor arguments).
struct ImpulseClassData
{
    std::string_view name;
    double           low;
    double           high;
};

/// Indexed by the enum's declaration order (ImpulseClass.java:7-21).
constexpr std::array<ImpulseClassData, kAllImpulseClasses.size()> kClasses{{
    {.name = "A", .low = -1.0, .high = 2.5},
    {.name = "B", .low = 2.5, .high = 5.0},
    {.name = "C", .low = 5.0, .high = 10.0},
    {.name = "D", .low = 10.0, .high = 20.0},
    {.name = "E", .low = 20.0, .high = 40.0},
    {.name = "F", .low = 40.0, .high = 80.0},
    {.name = "G", .low = 80.0, .high = 160.0},
    {.name = "H", .low = 160.0, .high = 320.0},
    {.name = "I", .low = 320.0, .high = 640.0},
    {.name = "J", .low = 640.0, .high = 1280.0},
    {.name = "K", .low = 1280.0, .high = 2560.0},
    {.name = "L", .low = 2560.0, .high = 5120.0},
    {.name = "M", .low = 5120.0, .high = 10240.0},
    {.name = "N", .low = 10240.0, .high = 20480.0},
    {.name = "O", .low = 20480.0, .high = std::numeric_limits<double>::max()},
}};

[[nodiscard]] const ImpulseClassData& data(ImpulseClass impulseClass)
{
    const auto index = static_cast<std::size_t>(impulseClass);
    QTROCKET_ASSERT(index < kClasses.size());
    return kClasses.at(index);
}

}  // namespace

std::string_view toString(ImpulseClass impulseClass)
{
    return data(impulseClass).name;
}

double getLow(ImpulseClass impulseClass)
{
    return data(impulseClass).low;
}

double getHigh(ImpulseClass impulseClass)
{
    return data(impulseClass).high;
}

}  // namespace QtRocket
