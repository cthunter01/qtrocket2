#pragma once

#include <string>

#include "QtRocket/util/DecimalFormat.h"

namespace QtRocket
{

/// The number formats of the simulation plots' tool tips (OpenRocket's swing
/// utils/DecimalFormatter): a DecimalFormat with a given number of decimals that shows more of
/// them for a value too small for that many.
///
/// Placement: OpenRocket keeps DecimalFormatter in its swing package; it is in the core's util/
/// here, beside DecimalFormat, so that the core's plot model (plot/SimulationPlotModel) writes
/// the tool tips.
///
/// Deviation from OpenRocket: a value so small that 1 / value is infinite (a subnormal below
/// about 5.6e-309) makes Java's digit count overflow, and df() throws NegativeArraySizeException
/// (an OpenRocket crash, found while porting: a tool tip of the plot over such a value fails);
/// here the format gets 340 more decimals (kMaximumFractionDigits, the most fraction digits
/// DecimalFormat writes for a double), so that it shows the value's leading digits.
class DecimalFormatter
{
public:
    DecimalFormatter() = delete;

    /// The most fraction digits DecimalFormat writes for a double (setMaximumFractionDigits()).
    static constexpr int kMaximumFractionDigits = 340;

    /// DecimalFormatter.df(): a format with @p decimals decimals for @p value, "#." followed by
    /// @p decimals '#' (or '0' with @p displayZeros, so that trailing zeros are shown: "8.00"
    /// rather than "8"), or "#" when @p decimals is not positive. When |@p value| is not 0 and
    /// below 0.1^(@p decimals - 1) (0.1 for 2 decimals), the format gets floor(log10(1 /
    /// |@p value|)) more decimals, so that its leading digits show: df(0.00345, 2, false) is
    /// "#.####", which formats 0.00345 as "0.0034". (The javadoc's "0.0035" is not what Java
    /// prints: the double 0.00345 lies below 0.00345.)
    [[nodiscard]] static DecimalFormat df(double value, int decimals, bool displayZeros);

    /// df(@p value, @p decimals, @p displayZeros).format(@p value): how OpenRocket's plots write a
    /// number.
    [[nodiscard]] static std::string format(double value, int decimals, bool displayZeros);
};

}  // namespace QtRocket
