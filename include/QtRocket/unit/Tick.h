#pragma once

#include <string>

#include "QtRocket/util/Strings.h"

namespace QtRocket
{

/// One tick mark of an axis scale (OpenRocket's Tick): where it lies in SI units and in the unit
/// that produced it, whether it is a major tick, and whether it is notable (worth a label).
struct Tick
{
    double value{};      ///< position in SI units
    double unitValue{};  ///< position in the producing unit's own scale
    bool   major{};
    bool   notable{};

    /// Tick.toString(): "Tick[value=1.5,major]", "Tick[value=0.1,minor,notable]", the value
    /// written as Java writes a double.
    [[nodiscard]] std::string toString() const
    {
        std::string text = "Tick[value=" + Strings::javaDoubleToString(value);
        text += major ? ",major" : ",minor";
        if (notable)
        {
            text += ",notable";
        }
        text += "]";
        return text;
    }
};

}  // namespace QtRocket
