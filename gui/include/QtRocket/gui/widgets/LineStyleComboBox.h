#pragma once

#include <QComboBox>
#include <QIcon>
#include <QObject>
#include <QPen>
#include <QSize>
#include <QString>

#include "QtRocket/util/LineStyle.h"

class QColor;
class QWheelEvent;
class QWidget;

namespace QtRocket::Gui
{

/// The combo box of a line style (OpenRocket's lineStyleSelector of SimulationPlotTypeSelector,
/// swing gui/simulation/: a JComboBox<LineStyle> with its LineStyleRenderer): the plot tab's line
/// style of each Y axis row. Its items are the styles in LineStyle's order (kAllLineStyles), each
/// shown by its name (text(): "Solid", "Dashed", "Dotted", "Dash-dotted") after a kIconSize icon of
/// its stroke (Java's LineStyleIcon): a line kStrokeWidth px wide across the middle of the icon,
/// with flat ends and the style's dashes (dashes(); solid without), in the text colour of the
/// application's palette (its highlighted text colour for a highlighted item, its disabled one
/// while disabled; Java: the renderer label's foreground).
///
/// Every setSelectedLineStyle() and every choice of the user's tells of it with
/// lineStyleSelected(), also when the style is the selected one already (Java's ActionEvent of a
/// JComboBox, which SimulationPlotTypeSelector.addLineStyleSelectionListener() hands out).
///
/// Deviations from OpenRocket: the keys and the wheel are EnumComboBox's (Up and Down choose
/// without opening the popup; the wheel chooses only while the combo box has the focus, which the
/// wheel does not give it); the gap between the icon and the text is the style's (Java: 8 px).
class LineStyleComboBox final : public QComboBox
{
    Q_OBJECT

public:
    /// LineStyleIcon's size, px.
    static constexpr QSize kIconSize{48, 12};
    /// LineStyleIcon's STROKE_WIDTH, px.
    static constexpr double kStrokeWidth = 2.0;

    /// A combo box of the line styles, @p initial selected (Java: SOLID for none).
    LineStyleComboBox(LineStyle initial, QWidget* parent);
    ~LineStyleComboBox() override;
    LineStyleComboBox(const LineStyleComboBox&)            = delete;
    LineStyleComboBox& operator=(const LineStyleComboBox&) = delete;
    LineStyleComboBox(LineStyleComboBox&&)                 = delete;
    LineStyleComboBox& operator=(LineStyleComboBox&&)      = delete;

    /// The selected style (Java: getSelectedItem()).
    [[nodiscard]] LineStyle selectedLineStyle() const;
    /// Selects @p style and tells of it (see the class comment).
    void setSelectedLineStyle(LineStyle style);

    /// The name of @p style (Java: LineStyle.toString(), trans.get() of its key).
    [[nodiscard]] static QString text(LineStyle style);
    /// The pen of @p style's stroke in @p color: kStrokeWidth wide, flat caps and bevel joins
    /// (Java: CAP_BUTT, JOIN_BEVEL), the style's dashes as a dash pattern (Java's dashes are
    /// lengths in px, Qt's in pen widths), solid for SOLID (Java: null dashes).
    [[nodiscard]] static QPen strokePen(LineStyle style, const QColor& color);
    /// The icon of @p style (see the class comment), drawn by a QIconEngine.
    [[nodiscard]] static QIcon strokeIcon(LineStyle style);

Q_SIGNALS:
    /// A style was selected (see the class comment).
    void lineStyleSelected();

protected:
    void wheelEvent(QWheelEvent* event) override;
};

}  // namespace QtRocket::Gui
