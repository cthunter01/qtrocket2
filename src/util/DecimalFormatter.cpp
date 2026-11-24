#include "QtRocket/util/DecimalFormatter.h"

#include <cmath>
#include <cstddef>
#include <string>

#include "QtRocket/util/DecimalFormat.h"

namespace QtRocket
{

DecimalFormat DecimalFormatter::df(double value, int decimals, bool displayZeros)
{
    if (decimals <= 0)
    {
        return DecimalFormat("#");
    }
    value              = std::abs(value);
    const char pattern = displayZeros ? '0' : '#';
    if (value < std::pow(0.1, decimals - 1) && value > 0)
    {
        const double decimalsValue = std::floor(std::log10(1 / value));
        // 1 / value overflows for the smallest subnormals; Java's cast then saturates and the
        // pattern's length overflows (see the class comment).
        decimals = decimalsValue <= kMaximumFractionDigits
                       ? static_cast<int>(decimalsValue) + decimals
                       : kMaximumFractionDigits + decimals;
    }
    return DecimalFormat("#." + std::string(static_cast<std::size_t>(decimals), pattern));
}

std::string DecimalFormatter::format(double value, int decimals, bool displayZeros)
{
    return df(value, decimals, displayZeros).format(value);
}

}  // namespace QtRocket
