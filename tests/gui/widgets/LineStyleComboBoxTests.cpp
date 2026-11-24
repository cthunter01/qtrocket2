// The combo box of a line style (Java: SimulationPlotTypeSelector's lineStyleSelector with its
// LineStyleRenderer and LineStyleIcon).
//
// Expectations marked "probe" are OpenRocket's own results, printed by the Java probe
// tools/openrocket-gui-pins/plot-widgets/probe/PlotWidgetsProbe.java (OpenRocket 5f164fd0e, JDK 17;
// its transcript is plot-widgets/hand-pinned.txt): the line style combo box of a
// SimulationPlotTypeSelector, its items, what its renderer's icons paint at (3, 4) into a
// recording Graphics2D, and its events.

#include "QtRocket/gui/widgets/LineStyleComboBox.h"

#include <cstddef>
#include <string_view>
#include <vector>

#include <QColor>
#include <QGuiApplication>
#include <QIcon>
#include <QLineEdit>
#include <QList>
#include <QObject>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QSignalSpy>
#include <QSize>
#include <QString>
#include <QWidget>
#include <Qt>
#include <QtTypes>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/LineStyle.h"
#include "RecordingGuardHook.h"
#include "WidgetTestSupport.h"
#include "figure/RecordingPaintDevice.h"

namespace QtRocket::Gui
{

namespace
{

using LineStyleComboBoxTest = Test::GuiTest;
using Test::RecordedPaint;
using Test::RecordingGuardHook;
using Test::RecordingPaintDevice;
using Test::showActiveWith;
using Test::turnWheel;

/// What the icon of @p style paints at (3, 4) in @p size, in @p mode.
[[nodiscard]] std::vector<RecordedPaint> paintedStroke(LineStyle style, QIcon::Mode mode,
                                                       QSize size = LineStyleComboBox::kIconSize)
{
    RecordingPaintDevice device(QSize(120, 40));
    {
        QPainter painter(&device);
        LineStyleComboBox::strokeIcon(style).paint(&painter, QRect(QPoint(3, 4), size),
                                                   Qt::AlignCenter, mode);
    }
    return device.records();
}

/// probe: what LineStyleIcon paints of each style at (3, 4): a line from (3, 10) to (51, 10),
/// BasicStroke(2, CAP_BUTT, JOIN_BEVEL, 0, dashes, 0), the dashes in px.
struct Stroke
{
    LineStyle          style;
    std::string_view   text;
    std::vector<qreal> dashes;
};

[[nodiscard]] std::vector<Stroke> probeStrokes()
{
    return {
        {.style = LineStyle::SOLID, .text = "Solid", .dashes = {}},
        {.style = LineStyle::DASHED, .text = "Dashed", .dashes = {6.0, 4.0}},
        {.style = LineStyle::DOTTED, .text = "Dotted", .dashes = {2.0, 3.0}},
        {.style = LineStyle::DASHDOT, .text = "Dash-dotted", .dashes = {8.0, 3.0, 2.0, 3.0}},
    };
}

/// @p dashes in px as a QPen's dash pattern, in pen widths (2 px).
[[nodiscard]] QList<qreal> inPenWidths(const std::vector<qreal>& dashes)
{
    QList<qreal> pattern;
    for (const qreal length : dashes)
    {
        pattern.append(length / LineStyleComboBox::kStrokeWidth);
    }
    return pattern;
}

/// Checks the items of @p combo against the probe's: their names, each with an icon.
void expectTheProbesItems(const LineStyleComboBox& combo)
{
    const std::vector<Stroke> strokes = probeStrokes();
    ASSERT_EQ(combo.count(), static_cast<int>(strokes.size()));
    for (std::size_t index = 0; index < strokes.size(); ++index)
    {
        const Stroke& stroke = strokes.at(index);
        EXPECT_EQ(combo.itemText(static_cast<int>(index)), QString::fromUtf8(stroke.text));
        EXPECT_EQ(LineStyleComboBox::text(stroke.style), QString::fromUtf8(stroke.text));
        EXPECT_FALSE(combo.itemIcon(static_cast<int>(index)).isNull());
    }
}

TEST_F(LineStyleComboBoxTest, TheItemsAreTheStylesWithTheirNames)
{
    // probe: items: SOLID='Solid' | DASHED='Dashed' | DOTTED='Dotted' | DASHDOT='Dash-dotted';
    // selected=SOLID editable=false; each item's icon 48x12. initial DOTTED: selected=DOTTED.
    const LineStyleComboBox combo(LineStyle::SOLID, nullptr);
    expectTheProbesItems(combo);
    EXPECT_EQ(combo.selectedLineStyle(), LineStyle::SOLID);
    EXPECT_EQ(combo.iconSize(), QSize(48, 12));
    EXPECT_FALSE(combo.isEditable());
    EXPECT_EQ(combo.focusPolicy(), Qt::StrongFocus);
    EXPECT_EQ(LineStyleComboBox(LineStyle::DOTTED, nullptr).selectedLineStyle(), LineStyle::DOTTED);
}

/// Checks the pen of @p line: probe: BasicStroke(2, CAP_BUTT, JOIN_BEVEL, 0, dashes, 0) of
/// @p stroke in @p color.
void expectPen(const RecordedPaint& line, const Stroke& stroke, const QColor& color)
{
    EXPECT_EQ(line.pen.widthF(), 2.0) << stroke.text;
    EXPECT_EQ(line.pen.capStyle(), Qt::FlatCap) << stroke.text;
    EXPECT_EQ(line.pen.joinStyle(), Qt::BevelJoin) << stroke.text;
    EXPECT_EQ(line.pen.color(), color) << stroke.text;
}

/// Checks the dashes of @p line's pen: @p stroke's in pen widths, solid without (Java: no dashes
/// for SOLID), from the start.
void expectDashes(const RecordedPaint& line, const Stroke& stroke)
{
    EXPECT_EQ(line.pen.style(), stroke.dashes.empty() ? Qt::SolidLine : Qt::CustomDashLine)
        << stroke.text;
    EXPECT_EQ(line.pen.dashPattern(),
              stroke.dashes.empty() ? QPen().dashPattern() : inPenWidths(stroke.dashes))
        << stroke.text;
    EXPECT_EQ(line.pen.dashOffset(), 0.0) << stroke.text;
}

/// Checks the one line @p records hold: probe: drawLine(3, 10, 51, 10) with the stroke of
/// @p stroke in @p color.
void expectStroke(const std::vector<RecordedPaint>& records, const Stroke& stroke,
                  const QColor& color)
{
    ASSERT_EQ(records.size(), 1U) << stroke.text;
    const RecordedPaint& line = records.front();
    EXPECT_EQ(line.kind, RecordedPaint::Kind::LINE) << stroke.text;
    EXPECT_EQ(QPointF(line.devicePath().elementAt(0)), QPointF(3, 10)) << stroke.text;
    EXPECT_EQ(QPointF(line.devicePath().elementAt(1)), QPointF(51, 10)) << stroke.text;
    expectPen(line, stroke, color);
    expectDashes(line, stroke);
}

TEST_F(LineStyleComboBoxTest, EachIconIsTheStrokeOfItsStyle)
{
    // probe: the renderer's foreground, the list's (0,0,0 in FlatLaf Light): the palette's text
    // colour here.
    const QColor text = QGuiApplication::palette().color(QPalette::Active, QPalette::Text);
    for (const Stroke& stroke : probeStrokes())
    {
        expectStroke(paintedStroke(stroke.style, QIcon::Normal), stroke, text);
    }
}

TEST_F(LineStyleComboBoxTest, TheStrokeTakesTheColourOfTheIconsMode)
{
    const QPalette palette = QGuiApplication::palette();
    const Stroke   dashed  = probeStrokes().at(1);
    expectStroke(paintedStroke(dashed.style, QIcon::Selected), dashed,
                 palette.color(QPalette::Active, QPalette::HighlightedText));
    expectStroke(paintedStroke(dashed.style, QIcon::Disabled), dashed,
                 palette.color(QPalette::Disabled, QPalette::Text));
}

TEST_F(LineStyleComboBoxTest, ALargerIconScalesTheStroke)
{
    // A screen of twice the density draws the icon at 96 x 24 px.
    const std::vector<RecordedPaint> records =
        paintedStroke(LineStyle::DASHED, QIcon::Normal, QSize(96, 24));
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(QPointF(records.front().devicePath().elementAt(0)), QPointF(3, 16));
    EXPECT_EQ(QPointF(records.front().devicePath().elementAt(1)), QPointF(99, 16));
    EXPECT_EQ(records.front().pen.widthF(), 4.0);
    EXPECT_EQ(records.front().pen.dashPattern(), (QList<qreal>{3.0, 2.0}));
}

TEST_F(LineStyleComboBoxTest, EverySelectionIsToldOf)
{
    // probe: setSelectedLineStyle(DASHED): an action event; again: an action event.
    LineStyleComboBox combo(LineStyle::SOLID, nullptr);
    const QSignalSpy  selections(&combo, &LineStyleComboBox::lineStyleSelected);
    combo.setSelectedLineStyle(LineStyle::DASHED);
    EXPECT_EQ(combo.selectedLineStyle(), LineStyle::DASHED);
    EXPECT_EQ(selections.count(), 1);
    combo.setSelectedLineStyle(LineStyle::DASHED);
    EXPECT_EQ(selections.count(), 2);
    // The user's choice, also of the selected style.
    combo.setCurrentIndex(3);
    Q_EMIT combo.activated(3);
    EXPECT_EQ(combo.selectedLineStyle(), LineStyle::DASHDOT);
    EXPECT_EQ(selections.count(), 3);
    Q_EMIT combo.activated(3);
    EXPECT_EQ(selections.count(), 4);
}

TEST_F(LineStyleComboBoxTest, TheWheelChoosesOnlyWithTheFocus)
{
    QWidget window;
    auto*   combo = makeChild<LineStyleComboBox>(&window, LineStyle::DASHED);
    auto*   other = makeChild<QLineEdit>(&window);
    ASSERT_TRUE(showActiveWith(window, {combo, other}));
    other->setFocus();
    EXPECT_FALSE(turnWheel(*combo, -120));
    EXPECT_EQ(combo->selectedLineStyle(), LineStyle::DASHED);
}

TEST_F(LineStyleComboBoxTest, AListenersErrorIsReported)
{
    LineStyleComboBox combo(LineStyle::SOLID, nullptr);
    combo.setObjectName(QStringLiteral("lineStyleSelector"));
    QObject::connect(&combo, &LineStyleComboBox::lineStyleSelected, &combo,
                     [] { bug("a broken listener"); });
    const RecordingGuardHook hook;
    Q_EMIT combo.activated(1);
    ASSERT_EQ(hook.failures().size(), 1U);
    EXPECT_EQ(hook.failures().front().context, QStringLiteral("lineStyleSelector"));
}

}  // namespace

}  // namespace QtRocket::Gui
