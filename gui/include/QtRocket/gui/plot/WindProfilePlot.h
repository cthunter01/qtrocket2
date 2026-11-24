#pragma once

#include <memory>
#include <utility>
#include <vector>

#include <QLineF>
#include <QObject>
#include <QPointF>
#include <QPolygonF>
#include <QRgb>
#include <QString>
#include <QWidget>

#include <rocketplot/PlotWidget.h>

#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/util/Signal.h"

class QCheckBox;

namespace QtRocket
{
class Unit;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The wind editor's profile of the levels (OpenRocket's WindProfilePanel and its
/// WindProfileVisualization, swing gui/simulation/WindProfilePanel.java), on a rocketplot
/// PlotWidget (plotWidget()) above the check box "Show wind direction vectors"
/// (showDirectionsCheckBox(), on to begin with). The areas after plot/ use it through this class
/// alone (the wind editor, dialogs/simulation/MultiLevelWindEditDialog).
///
/// It shows the levels of a MultiLevelPinkNoiseWindModel sorted by altitude (Java sorts a copy
/// for drawing), each at its speed (x) and altitude (y) in the units of the editor's table
/// (setUnits()):
/// - a grey line through the levels in that order (Java: Color.GRAY lines between consecutive
///   levels), then a dot per level: blue and 6 pixels across, the selected level's
///   (setSelectedLevel(), the table's selected row) red and 10 pixels across with a black ring
///   (Java: fillOval and drawOval), as a ScatterSeries with a colour and a size per point and a
///   hollow black circle over the selected one;
/// - while the check box is on, an arrow from each dot pointing downwind (where the wind blows to:
///   kArrowLength pixels and a filled head reaching kArrowReach pixels from the dot, Java's
///   drawWindArrow()), (40, 40, 199) and the selected level's (199, 40, 40), as an IconAnnotation
///   drawn at its size whatever the zoom (arrowLine() and arrowHead() give its shape);
/// - over each dot a tool tip (an IconAnnotation that draws nothing, twice the dot's size across:
///   Java's detection within twice the dot's radius) with Java's text (toolTipText()):
///   "<html>Altitude MSL: <altitude><br>Speed: <speed><br>Direction: <direction><br>Deviation:
///   <deviation></html>", each value with its unit in the table's units.
/// The axes are labelled "Wind speed (<unit>)" and "Altitude MSL (<unit>)" or "Altitude AGL
/// (<unit>)" (WindProfilePanel.lbl.*, through rocketplot::literalText()), the altitude's by the
/// model's altitude reference, and show [0, 1.1 times the highest speed] and [0, 1.1 times the
/// highest altitude] (Java: the ranges extended by 10 %) in those units (see axisRange() for the
/// ranges Java cannot draw). The plot follows every change of the model (its changed() signal:
/// Java repaints on every change of the table) and of the units, the selection and the check box.
/// The model must stay at its address while the plot shows it; the plot learns that it has gone
/// from its change signal and from then on keeps what it showed (hasModel()).
///
/// Deviations from OpenRocket (the user's decision U-M4-6: the profile is a rocketplot): rocketplot
/// draws its own ticks, tick labels and grid, where Java draws one tick at the highest speed and
/// altitude, labelled in the table's units, and arrow heads at the ends of its axes; the plot
/// takes the palette's light or dark look (PlotTheme's rule) where Java's is white, the levels'
/// colours staying Java's; no gesture zooms or pans (PlotInputBindings::setNoInteraction(): Java's
/// panel has none). Levels below sea level are shown: the altitude axis reaches 1.1 times the
/// lowest altitude below 0 (Java's axis starts at 0 and draws those levels below it, outside the
/// plot area), and with no level above 0 it ends at 0 (Java's ends at 1.1 times the highest
/// altitude, a negative one, and turns the profile upside down). An axis whose range would be
/// empty (no wind, levels only at 0 m, no levels) shows [0, 1] in its unit (Java divides by zero
/// and draws the dots on the axis).
class WindProfilePlot final : public QWidget
{
    Q_OBJECT

public:
    using Level = MultiLevelPinkNoiseWindModel::LevelWindModel;

    /// The units of the editor's table (MultiLevelWindTable's getAltitudeUnit(), getSpeedUnit(),
    /// getDirectionUnit() and getStdDeviationUnit()); none is null.
    struct Units
    {
        const Unit* altitude  = nullptr;
        const Unit* speed     = nullptr;
        const Unit* direction = nullptr;
        const Unit* deviation = nullptr;
    };

    /// How far the axes reach beyond the highest values (Java: "Extend axis ranges by 10%").
    static constexpr double kRangeExtension = 1.1;
    /// The dots' diameters, pixels (Java: fillOval 6 x 6, the selected 10 x 10).
    static constexpr double kDotSize         = 6;
    static constexpr double kSelectedDotSize = 10;
    /// The colours: Color.BLUE, Color.RED, Color.GRAY, Color.BLACK (the selected dot's ring) and
    /// Java's DIRECTION_COLOR and SELECTED_DIRECTION_COLOR.
    static constexpr QRgb kLevelColor             = 0xff0000ff;
    static constexpr QRgb kSelectedLevelColor     = 0xffff0000;
    static constexpr QRgb kLineColor              = 0xff808080;
    static constexpr QRgb kRingColor              = 0xff000000;
    static constexpr QRgb kDirectionColor         = 0xff2828c7;
    static constexpr QRgb kSelectedDirectionColor = 0xffc72828;
    /// The arrow's line, pixels (Java: directionVectorLength), and how far its head reaches from
    /// the dot (the line and Java's arrowSize - 1).
    static constexpr double kArrowLength = 15;
    static constexpr double kArrowReach  = 24;
    /// The arrow head's size, pixels (Java: arrowSize), its sides 30 degrees off the arrow.
    static constexpr double kArrowHeadSize = 10;
    /// The side of an arrow's icon, centred on its dot, pixels.
    static constexpr double kArrowIconSide = 2 * (kArrowReach + 1);

    /// The profile of @p model in the default units of the table's columns.
    WindProfilePlot(MultiLevelPinkNoiseWindModel& model, QWidget* parent);
    ~WindProfilePlot() override;
    WindProfilePlot(const WindProfilePlot&)            = delete;
    WindProfilePlot& operator=(const WindProfilePlot&) = delete;
    WindProfilePlot(WindProfilePlot&&)                 = delete;
    WindProfilePlot& operator=(WindProfilePlot&&)      = delete;

    /// The plot (objectName "windProfileVisualization", Java's WindProfileVisualization), for
    /// plot/ and the tests; the areas after plot/ never name it.
    [[nodiscard]] rocketplot::PlotWidget& plotWidget() const noexcept { return *m_plot; }

    /// "Show wind direction vectors" (objectName "showDirectionsCheckBox").
    [[nodiscard]] QCheckBox& showDirectionsCheckBox() const noexcept { return *m_showDirections; }

    [[nodiscard]] const Units& units() const noexcept { return m_units; }
    /// Shows the profile in @p units from now on.
    /// @throws BugError when a unit is null
    void setUnits(const Units& units);

    /// The level shown selected, or null.
    [[nodiscard]] const Level* selectedLevel() const noexcept { return m_selected.lock().get(); }
    /// Shows @p level selected (none for an empty pointer, or once the level has gone).
    void setSelectedLevel(std::weak_ptr<const Level> level);

    /// Whether the model is still there (see the class comment).
    [[nodiscard]] bool hasModel() const noexcept;

    /// Draws the profile of the model's levels again (what every change does).
    void refresh();

    /// The tool tip of @p level (Java: formatTooltipText()), the altitude named by @p reference.
    [[nodiscard]] static QString toolTipText(const Level& level, const Units& units,
                                             WindModel::AltitudeReference reference);
    /// The axes' labels: "Wind speed (<unit>)" and "Altitude MSL (<unit>)" or "Altitude AGL
    /// (<unit>)" (as given to rocketplot::literalText()).
    [[nodiscard]] static QString speedAxisLabel(const Unit& unit);
    [[nodiscard]] static QString altitudeAxisLabel(const Unit&                  unit,
                                                   WindModel::AltitudeReference reference);
    /// The range of an axis whose values, in SI units, are @p lowest to @p highest, in @p unit:
    /// from 0 or kRangeExtension times a lowest value below 0, to 0 or kRangeExtension times a
    /// highest value above 0; [0, 1] for a range that would be empty (see the class comment).
    [[nodiscard]] static std::pair<double, double> axisRange(double lowest, double highest,
                                                             const Unit& unit);

    /// The arrow of a level whose wind blows from @p direction (radians clockwise from north),
    /// drawn from @p dot at @p scale pixels per pixel of Java's drawing: its line, kArrowLength
    /// long, and its head, a triangle whose tip is kArrowReach from @p dot (Java's
    /// drawWindArrow() and drawFilledArrowHead(), y downward).
    [[nodiscard]] static QLineF    arrowLine(double direction, QPointF dot, double scale);
    [[nodiscard]] static QPolygonF arrowHead(double direction, QPointF dot, double scale);

private:
    /// The model's levels by altitude.
    [[nodiscard]] std::vector<const Level*> sortedLevels() const;
    void                                    updateAxes(const std::vector<const Level*>& levels);
    void                                    addLevels(const std::vector<const Level*>& levels);

    MultiLevelPinkNoiseWindModel* m_model;
    Signal<>::ScopedConnection    m_connection;
    rocketplot::PlotWidget*       m_plot;
    QCheckBox*                    m_showDirections;
    Units                         m_units;
    std::weak_ptr<const Level>    m_selected;
};

}  // namespace QtRocket::Gui
