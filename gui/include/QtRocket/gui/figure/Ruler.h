#pragma once

#include <utility>
#include <vector>

#include <QObject>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QWidget>
#include <Qt>

class QPaintEvent;

namespace QtRocket
{
class Unit;
}

namespace QtRocket::Gui
{

class ScaleFigure;

/// A ruler along the top or the left edge of a ScaleScrollArea's viewport (OpenRocket's
/// ScaleScrollPane.Ruler): 20 pixels thick, with the ticks of the ruler's unit
/// (Unit::getTicks() with at least 3 pixels between minor ticks and 30 between major ones) over
/// the part of the figure that is visible, each at its pixel in the figure. A major tick is 10
/// pixels long with its value (Unit::toString(), without the unit) beside it, in bold when it is
/// notable; a notable minor tick is 6 pixels long, a minor one 3. The horizontal ruler counts x
/// from the figure's origin rightwards, the vertical one y upwards.
///
/// The ruler is a fixed child of the scroll area: it draws the figure's pixel p at p plus the
/// figure's position in the viewport (the negative scroll offset), as Java's ruler in its
/// scrolling header viewport does.
///
/// Deviations from OpenRocket: nothing is drawn while the figure's scale is not a positive
/// finite number or the visible range would make Unit::getTicks() fail (a BugError there); the
/// labels are 9 pixels high (Java's font size 9 is in Java2D units, pixels), in the widget's
/// font family rather than OpenRocket's bundled one.
class Ruler : public QWidget
{
    Q_OBJECT

public:
    /// ScaleScrollPane.RULER_SIZE, MINOR_TICKS and MAJOR_TICKS (pixels).
    static constexpr int kSize       = 20;
    static constexpr int kMinorTicks = 3;
    static constexpr int kMajorTicks = 30;
    /// The tick lengths: RULER_SIZE / 2, / 3 and / 6.
    static constexpr int kMajorLength   = kSize / 2;
    static constexpr int kNotableLength = kSize / 3;
    static constexpr int kMinorLength   = kSize / 6;
    /// The labels' pixel size (Java: font size 9).
    static constexpr int kLabelPixelSize = 9;

    /// One tick: its pixel in the figure's coordinates (x for the horizontal ruler, y for the
    /// vertical one), its length, and for a major tick its label, bold when notable.
    struct Mark
    {
        int     position{};
        int     length{};
        QString label;
        bool    bold{};

        [[nodiscard]] bool operator==(const Mark&) const = default;
    };

    /// A ruler of @p orientation for @p figure (which must outlive it), in the default length
    /// unit.
    Ruler(Qt::Orientation orientation, ScaleFigure* figure, QWidget* parent);

    [[nodiscard]] Qt::Orientation orientation() const noexcept { return m_orientation; }

    /// The unit of the ticks and labels.
    [[nodiscard]] const Unit& unit() const noexcept { return *m_unit; }
    /// Sets the unit and repaints; @p unit must outlive the ruler (a unit group's unit does).
    void setUnit(const Unit& unit);

    /// The figure's pixels this ruler covers: the figure's visible part in the viewport (the
    /// figure's parent), from the left or top edge to the right or bottom one; the whole figure
    /// when it has no parent.
    [[nodiscard]] std::pair<int, int> visibleRange() const;

    /// marks() of the figure's current scale and origin over visibleRange().
    [[nodiscard]] std::vector<Mark> marks() const;

    /// The ticks of @p unit from the figure's pixel @p startPx to @p endPx (Ruler.paintComponent
    /// and drawTick): the values at those pixels for an origin at @p origin and @p scale pixels
    /// per metre (x = (px - origin.x) / scale, y = (origin.y - px) / scale; the vertical ruler
    /// passes them to getTicks() in reverse order, increasing), the ticks at least 3 / scale
    /// and 30 / scale apart, each at (int)(value * scale + 0.5) + origin.x, or origin.y less
    /// that. Empty when the scale is not a positive finite number, or when the values or the
    /// distances are not finite or the range spans more than a billion minor distances.
    [[nodiscard]] static std::vector<Mark> marks(Qt::Orientation orientation, const Unit& unit,
                                                 double scale, QPoint origin, int startPx,
                                                 int endPx);

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    Qt::Orientation    m_orientation;
    const ScaleFigure* m_figure;
    const Unit*        m_unit;
};

}  // namespace QtRocket::Gui
