#pragma once

#include <limits>

namespace QtRocket
{

/// The range one axis of a plot shows (OpenRocket's swing gui/plot/Axis): the smallest and the
/// largest of the bounds added since the last reset(), both NaN while none was. PlotConfiguration
/// keeps one per axis and widens it to its data (PlotConfiguration::fitAxes()).
///
/// A value type: a copy is Java's clone().
///
/// Placement: OpenRocket keeps Axis in its swing package; it is part of the core's plot model
/// here (plot/, the Qt-free model of the simulation plots), so that the automatic ranges are
/// computed and compared without Qt. Renamed, as "Axis" is the name of the GUI's axis widgets.
class PlotAxisBounds
{
public:
    /// Widens the range to hold @p value (Java: addBound()): @p value becomes the minimum when it
    /// is smaller or the minimum is NaN, and the maximum when it is larger or the maximum is NaN.
    /// So a NaN @p value changes nothing once a bound was added (it only replaces a NaN bound),
    /// and a -0.0 does not replace a minimum of 0.0 (nor a 0.0 a maximum of -0.0).
    void addBound(double value) noexcept;

    /// The smallest bound, NaN while none was added.
    [[nodiscard]] double getMinValue() const noexcept { return m_minValue; }
    /// The largest bound, NaN while none was added.
    [[nodiscard]] double getMaxValue() const noexcept { return m_maxValue; }
    /// getMaxValue() - getMinValue(): NaN while no bound was added.
    [[nodiscard]] double getRangeLength() const noexcept { return m_maxValue - m_minValue; }

    /// Forgets the bounds: both NaN again.
    void reset() noexcept;

private:
    double m_minValue{std::numeric_limits<double>::quiet_NaN()};
    double m_maxValue{std::numeric_limits<double>::quiet_NaN()};
};

}  // namespace QtRocket
