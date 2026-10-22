// OpenRocket's panel of the freeform fin set (FreeformFinSetConfig with its FinPointFigure in a
// FinPointScrollPane, OpenRocket 5f164fd0e) made by FreeformPanelProbe
// (tools/openrocket-panel-pins) under Xvfb, in a dialog that is never shown, against QtRocket's in
// the same state. The scroll pane is 600 x 400 pixels at 96 dpi, fitted, the figure painted once
// (Java's hit tests need that). The probe printed, and this test replays:
// - "panel": the panel of every freeform fin set of the 16 examples (loaded without presets) and
//   the 13 test rockets, each after the design was settled: the tabs' titles and tooltips, the
//   General and Fin tabs' widgets (as FinPanelProbe prints them), the Shape tab's (the point
//   table's headers, cells and editable cells, the labels with their size relative to the default,
//   the buttons, the check box, the zoom box and the Point Actions menu) and the figure (fitting,
//   the scale, the x of the origin, the subject and content bounds, the auto zoom point);
// - "edit": steps on designs of the probe's own (a freeform fin set on a body tube 30 cm long with
//   a fixed radius of 2.1 cm, on a conical transition, on an ogive nose cone), each followed by
//   the fin points (getFinPoints()), the table's rows, selection and cells, the figure's selected
//   point, snap highlight, dragged point, scale and origin x, the edit history, the undo and redo
//   buttons and the table's actions, whether the rocket is frozen, the fin set's offset and
//   length. A step is one of: "hit <point> <dx> <dy>" and "hitAt <x> <y>" (the figure's hit tests
//   at the point's pixel plus an offset, or at a point of the fin frame), "table <row> <column>
//   <text>" (the text the cell editor commits), "select <row>", "insert <row>" and "delete <row>"
//   (the row selected, then the table's action when it is enabled), "press <point> [<dx> <dy>]"
//   and "pressAt <x> <y>", "drag <x> <y> [ctrl] [shift]", "release", "click <point> [ctrl]
//   [shift] [button3]" (mouse events of the left button, unless button3, on the scroll area at the
//   figure's pixels: the origin plus Math.round() of the point times the scale, the y negated),
//   "undo" and "redo" (the buttons), "live <b>" (the preference), "units <unit>" (the default
//   length unit) and "csv" (the CSV export's text);
// - "multi" and "multi edit": the same steps on a panel with a second freeform fin set selected
//   (Java: the config listener of the first), the points of both.
// The lines of a panel section are sorted, a step's are in the order printed. The pins are
// FreeformPanelPins.h, generated from the probe's output; this test reads the steps from them.
#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QList>
#include <QMenu>
#include <QModelIndex>
#include <QMouseEvent>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QPushButton>
#include <QString>
#include <QTabWidget>
#include <QTableView>
#include <QTest>
#include <QToolButton>
#include <QVariant>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "BodyPanelTestSupport.h"
#include "ConfigDialogTestSupport.h"
#include "FreeformPanelPins.h"
#include "GuiTestSupport.h"
#include "PanelProbeLines.h"
#include "PanelReplay.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/configdialog/FinPointTableModel.h"
#include "QtRocket/gui/configdialog/FreeformFinSetConfig.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/figure/FinPointEditHistory.h"
#include "QtRocket/gui/figure/FinPointFigure.h"
#include "QtRocket/gui/figure/FinPointScrollArea.h"
#include "QtRocket/gui/figure/ScaleSelector.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/DropdownButton.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/AutomaticDimensions.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/MathUtil.h"
#include "unit/DefaultUnitsGuard.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using FreeformPanelBuiltGolden = Test::GuiTest;
using Test::ConfigDialogServices;
using Test::FactoryPanel;
using Test::FreeformPanelPin;
using Test::javaDouble;
using Test::kFreeformPanelPins;
using Test::kFreeformPanelPinsCommit;
using Test::onBodyTube;
using Test::PanelReplay;
using Test::pinnedDesignDocument;
using Test::pinnedLineDifferences;
using Test::quoted;
using Test::quotedOrNull;

/// The probe's scroll pane, 600 x 400 pixels.
constexpr int kPaneWidth  = 600;
constexpr int kPaneHeight = 400;
/// The probe's DPI preference, 96 dots per inch in tenths.
constexpr int kDpiTenths = 960;

[[nodiscard]] std::string_view javaBool(bool value)
{
    return value ? "true" : "false";
}

/// The words of @p text separated by single spaces (Java's split(" ", -1)).
[[nodiscard]] std::vector<std::string_view> words(std::string_view text)
{
    std::vector<std::string_view> found;
    std::size_t                   start = 0;
    while (true)
    {
        const std::size_t space = text.find(' ', start);
        found.push_back(text.substr(
            start, space == std::string_view::npos ? std::string_view::npos : space - start));
        if (space == std::string_view::npos)
        {
            return found;
        }
        start = space + 1;
    }
}

[[nodiscard]] double number(std::string_view text)
{
    bool         parsed = false;
    const double value  = QtStrings::fromUtf8(text).toDouble(&parsed);
    QTROCKET_ASSERT(parsed);
    return value;
}

[[nodiscard]] int integer(std::string_view text)
{
    bool      parsed = false;
    const int value  = QtStrings::fromUtf8(text).toInt(&parsed);
    QTROCKET_ASSERT(parsed);
    return value;
}

/// Whether @p list holds @p word.
[[nodiscard]] bool hasWord(std::span<const std::string_view> list, std::string_view word)
{
    return std::ranges::find(list, word) != list.end();
}

// ------------------------------------------------------------------------------- the lines

/// The probe's buttonLine(): "button "<text>" enabled=<b> tip=<tooltip>".
[[nodiscard]] std::string buttonLine(const QAbstractButton& button)
{
    return std::format("button {} enabled={} tip={}", quoted(button.text()),
                       javaBool(button.isEnabled()), quotedOrNull(button.toolTip()));
}

/// The lines of the widgets of the tab @p index of @p tabs, each after @p prefix.
void addPrefixedTabLines(const QTabWidget& tabs, int index, std::string_view prefix,
                         std::vector<std::string>& lines)
{
    std::vector<std::string> tabLines;
    Test::addTabWidgetLines(*tabs.widget(index), {.labels = true, .labelTips = true}, tabLines);
    for (const std::string& line : tabLines)
    {
        lines.push_back(std::string(prefix) + line);
    }
}

/// The probe's tabsLines(): every tab's title and tooltip, the General and Fin tabs' widgets and
/// the buttons of the button row.
void addTabsLines(const FreeformFinSetConfig& panel, std::vector<std::string>& lines)
{
    const QTabWidget& tabs = panel.tabs();
    for (int t = 0; t < tabs.count(); ++t)
    {
        lines.push_back(
            std::format("tab{} {} tip={}", t, quoted(tabs.tabText(t)), quoted(tabs.tabToolTip(t))));
    }
    addPrefixedTabLines(tabs, 0, "general ", lines);
    addPrefixedTabLines(tabs, 2, "fintabs ", lines);
    lines.push_back("fintabs " + buttonLine(panel.autoCalcButton()));
    lines.push_back("row " + (panel.splitButton() != nullptr
                                  ? buttonLine(*panel.splitButton())
                                  : std::string("no button \"Split fins\"")));
    lines.push_back("row " + (panel.convertButton() != nullptr
                                  ? buttonLine(*panel.convertButton())
                                  : std::string("no button \"Convert to freeform\"")));
}

/// The probe's shapeWalk() line of a button: "button "<text>" enabled=<b> focusable=<b>
/// tip=<tooltip>".
[[nodiscard]] std::string shapeButtonLine(const QAbstractButton& button)
{
    return std::format("shape button {} enabled={} focusable={} tip={}", quoted(button.text()),
                       javaBool(button.isEnabled()), javaBool(button.focusPolicy() != Qt::NoFocus),
                       quotedOrNull(button.toolTip()));
}

/// The table's lines of the probe's shapeLines().
void addTableLines(const FreeformFinSetConfig& panel, std::vector<std::string>& lines)
{
    const FinPointTableModel& model = panel.pointTableModel();
    lines.push_back(std::format("shape table header {} {}",
                                quoted(FinPointTableModel::columnName(0)),
                                quoted(FinPointTableModel::columnName(1))));
    for (int row = 0; row < model.rowCount(); ++row)
    {
        lines.push_back(
            std::format("shape table row {:02} {} {} editable={},{}", row,
                        quoted(model.valueText(row, 0)), quoted(model.valueText(row, 1)),
                        javaBool(model.flags(model.index(row, 0)).testFlag(Qt::ItemIsEditable)),
                        javaBool(model.flags(model.index(row, 1)).testFlag(Qt::ItemIsEditable))));
    }
}

/// The probe's shapeLines(): the table, the labels, the buttons, the check box, the zoom box and
/// the Point Actions menu.
void addShapeLines(const FreeformFinSetConfig& panel, std::vector<std::string>& lines)
{
    addTableLines(panel, lines);
    const ScaleSelector& selector = panel.scaleSelector();
    for (const QAbstractButton* button :
         {static_cast<const QAbstractButton*>(&panel.finPointUndoButton()),
          static_cast<const QAbstractButton*>(&panel.finPointRedoButton()),
          static_cast<const QAbstractButton*>(selector.zoomInButton()),
          static_cast<const QAbstractButton*>(selector.zoomOutButton()),
          static_cast<const QAbstractButton*>(selector.zoomFitButton()),
          static_cast<const QAbstractButton*>(&panel.pointActionsButton())})
    {
        lines.push_back(shapeButtonLine(*button));
    }
    for (const QAction* item : panel.pointActionsButton().popupMenu().actions())
    {
        lines.push_back(std::format("shape menu {} enabled={}", quoted(item->text()),
                                    javaBool(item->isEnabled())));
    }
    const QCheckBox& live = panel.liveUpdateCheckBox();
    lines.push_back(std::format("shape check {} checked={} tip={}", quoted(live.text()),
                                javaBool(live.isChecked()), quotedOrNull(live.toolTip())));
    lines.push_back(std::format("shape combo {} editable={}",
                                quoted(selector.scaleCombo()->currentText()),
                                javaBool(selector.scaleCombo()->isEditable())));
    const double defaultSize = QApplication::font().pointSizeF();
    for (const StyledLabel* label : panel.shapeTab().findChildren<StyledLabel*>())
    {
        lines.push_back(std::format("shape label {} relative={}", quoted(label->text()),
                                    javaDouble(label->font().pointSizeF() - defaultSize)));
    }
}

[[nodiscard]] std::string rectangleText(const Rectangle2D& r)
{
    return std::format("({},{},{},{})", javaDouble(r.x), javaDouble(r.y), javaDouble(r.width),
                       javaDouble(r.height));
}

/// The probe's figureLines().
void addFigureLines(const FreeformFinSetConfig& panel, std::vector<std::string>& lines)
{
    const FinPointFigure& figure = panel.finPointFigure();
    lines.push_back(std::format(
        "figure fitting={} userScale={} scale={}", javaBool(panel.figureArea().isFitting()),
        javaDouble(figure.userScale()), javaDouble(figure.absoluteScale())));
    lines.push_back(std::format("figure originX={}", figure.subjectOrigin().x()));
    lines.push_back(std::format("figure subject={}", rectangleText(figure.subjectBounds())));
    lines.push_back(std::format("figure content={}", rectangleText(figure.contentBounds())));
    lines.push_back(std::format("figure autoZoomX={}", figure.autoZoomPoint().x()));
}

/// The probe's fit(): the scroll area 600 x 400 pixels, fitted.
void fit(const FreeformFinSetConfig& panel)
{
    panel.figureArea().resize(kPaneWidth, kPaneHeight);
    panel.figureArea().setFitting(true);
}

/// The points of @p fins as the probe prints them: "(x,y) (x,y) ...".
[[nodiscard]] std::string pointsText(const FreeformFinSet& fins)
{
    std::string text;
    for (const Coordinate& c : fins.getFinPoints())
    {
        if (!text.empty())
        {
            text += ' ';
        }
        text += std::format("({},{})", javaDouble(c.x), javaDouble(c.y));
    }
    return text;
}

/// The cells of the table: " "x" "y" ...".
[[nodiscard]] std::string cellsText(const FinPointTableModel& model)
{
    std::string text;
    for (int row = 0; row < model.rowCount(); ++row)
    {
        text +=
            std::format(" {} {}", quoted(model.valueText(row, 0)), quoted(model.valueText(row, 1)));
    }
    return text;
}

/// The probe's stateLines() of the panel and of @p main (and @p other).
void addStateLines(const FreeformFinSetConfig& panel, const FreeformFinSet& main,
                   const FreeformFinSet* other, std::vector<std::string>& lines)
{
    lines.push_back("points " + pointsText(main));
    if (other != nullptr)
    {
        lines.push_back("points other " + pointsText(*other));
    }
    const FinPointTableModel& model = panel.pointTableModel();
    lines.push_back(std::format("table rows={} selected={} cells{}", model.rowCount(),
                                panel.selectedPointRow(), cellsText(model)));
    const FinPointFigure& figure = panel.finPointFigure();
    lines.push_back(std::format("figure selected={} highlight={} drag={} scale={} originX={}",
                                figure.selectedIndex(), figure.highlightIndex(), panel.dragIndex(),
                                javaDouble(figure.absoluteScale()), figure.subjectOrigin().x()));
    const FinPointEditHistory& history = figure.editHistory();
    lines.push_back(std::format("history size={} position={} editing={} canUndo={} canRedo={}",
                                history.size(), history.position(),
                                javaBool(history.isEditInProgress()), javaBool(history.canUndo()),
                                javaBool(history.canRedo())));
    lines.push_back(std::format("buttons undo={} redo={} insert={} delete={}",
                                javaBool(panel.finPointUndoButton().isEnabled()),
                                javaBool(panel.finPointRedoButton().isEnabled()),
                                javaBool(panel.insertPointAction().isEnabled()),
                                javaBool(panel.deletePointAction().isEnabled())));
    lines.push_back(std::format("rocket frozen={} offset={} length={}",
                                javaBool(main.getRocket().isFrozen()),
                                javaDouble(main.getAxialOffset()), javaDouble(main.getLength())));
}

/// What the replay prints of the freeform fin sets of the examples and test rockets (the
/// probe's design()).
[[nodiscard]] Test::PanelReplayLines freeformLines()
{
    return {
        .isPanelKind =
            [](const RocketComponent& component) {
                return component.kind() == ComponentKind::FREEFORM_FIN_SET;
            },
        .className = [](const RocketComponent& /*component*/) -> std::string_view {
            return "FreeformFinSet";
        },
        .panel =
            [](const RocketComponentConfig& config, bool /*withLabels*/,
               std::vector<std::string>&    lines) {
                const auto& panel = dynamic_cast<const FreeformFinSetConfig&>(config);
                fit(panel);
                addTabsLines(panel, lines);
                addShapeLines(panel, lines);
                addFigureLines(panel, lines);
            },
        .state = [](const RocketComponent& /*component*/, std::string_view /*prefix*/,
                    bool /*fromDesign*/, std::vector<std::string>& /*lines*/) { },
    };
}

// ------------------------------------------------------------------------------- the differences

/// The pin @p pinned with QtRocket's differences by design:
/// - the Appearance tab is Milestone 5 (Comment is the fifth tab);
/// - the Point Actions menu has no "Scale Fin" (Milestone 5);
/// - the CSV lines end with '\n' (Java: "\r\n");
/// - a right click on a point shows the figure's menu (Java's popup, which cannot show in a
///   dialog that is not, throws an IllegalComponentStateException, after selecting the row).
[[nodiscard]] std::vector<std::string> withOurDifferences(std::span<const std::string_view> pinned)
{
    std::vector<std::string> lines;
    for (const std::string_view line : pinned)
    {
        if (line.starts_with("tab4 \"Appearance\"") ||
            line == R"(shape menu "Scale Fin" enabled=true)")
        {
            continue;
        }
        std::string ours(line);
        if (ours.starts_with("tab5 \"Comment\""))
        {
            ours.replace(0, std::string_view("tab5").size(), "tab4");
        }
        if (ours.starts_with("csv "))
        {
            for (std::size_t at = ours.find("\\r\\n"); at != std::string::npos;
                 at             = ours.find("\\r\\n"))
            {
                ours.replace(at, std::string_view("\\r\\n").size(), "\\n");
            }
        }
        if (ours == "popup IllegalComponentStateException")
        {
            ours = "popup shown";
        }
        lines.push_back(std::move(ours));
    }
    return lines;
}

/// Replaces the line @p javaLine of the section @p header (up to the next "== " line) of @p lines
/// by @p ourLine; the line must be there.
void replaceInSection(std::vector<std::string>& lines, std::string_view header,
                      std::string_view javaLine, std::string_view ourLine)
{
    bool inSection = false;
    for (std::string& line : lines)
    {
        if (line.starts_with("== "))
        {
            inSection = line == header;
        }
        else if (inSection && line == javaLine)
        {
            line = ourLine;
            return;
        }
    }
    ADD_FAILURE() << "the pin has no " << javaLine << " in " << header;
}

/// The step of "four and five" whose result differs from OpenRocket's: after the history's
/// undo and redo, Java's main fin set and its config listener share one point list
/// (FreeformFinSet.setPoints() keeps the list it is given and hands the main one's own list to the
/// listener), so moving the first point of both shifts the shared points twice, by 2 cm for the
/// 1 cm typed. Here each fin set keeps points of its own (FinPointSelection) and moves by 1 cm.
constexpr std::string_view kSharedPointsStep = "== multi edit | table 0 0 1";
constexpr std::array<std::pair<std::string_view, std::string_view>, 5> kSharedPointsLines{{
    {"points (0.0,0.0) (0.010000000000000004,0.06000000000000001) (0.0475,0.02) "
     "(0.03999999999999998,0.0)",
     "points (0.0,0.0) (0.020000000000000004,0.06000000000000001) (0.0575,0.02) "
     "(0.04999999999999999,0.0)"},
    {"points other (0.0,0.0) (0.010000000000000004,0.06000000000000001) (0.0475,0.02) "
     "(0.04000000000000001,0.0)",
     "points other (0.0,0.0) (0.020000000000000004,0.06000000000000001) (0.0575,0.02) "
     "(0.04999999999999999,0.0)"},
    {R"(table rows=4 selected=-1 cells "0" "0" "1" "6" "4.75" "2" "4" "0")",
     R"(table rows=4 selected=-1 cells "0" "0" "2" "6" "5.75" "2" "5" "0")"},
    {"figure selected=-1 highlight=-1 drag=-1 scale=6399.999999999999 originX=1757",
     "figure selected=-1 highlight=-1 drag=-1 scale=6399.999999999999 originX=1693"},
    {"rocket frozen=false offset=0.009999999999999995 length=0.039999999999999994",
     "rocket frozen=false offset=0.009999999999999995 length=0.049999999999999996"},
}};

/// withOurDifferences() of the pin @p pin, with the shared points of "four and five" (above).
[[nodiscard]] std::vector<std::string> expectedBuiltLines(const FreeformPanelPin& pin)
{
    std::vector<std::string> lines = withOurDifferences(pin.lines);
    if (pin.design == "four and five")
    {
        for (const auto& [javaLine, ourLine] : kSharedPointsLines)
        {
            replaceInSection(lines, kSharedPointsStep, javaLine, ourLine);
        }
    }
    return lines;
}

/// The differences between @p actual and @p expected, one per line ("-" pinned, "+" ours).
[[nodiscard]] std::string differences(const std::vector<std::string>& actual,
                                      const std::vector<std::string>& expected)
{
    const std::vector<std::string_view> views(expected.begin(), expected.end());
    return pinnedLineDifferences(actual, views);
}

// ------------------------------------------------------------------------------- the designs

/// The probe's freeform(): a freeform fin set with the points @p xy (x, y, x, y, ...), the
/// default outline when there are none.
[[nodiscard]] std::unique_ptr<FreeformFinSet> makeFreeform(std::initializer_list<double> xy = {})
{
    auto fins = std::make_unique<FreeformFinSet>();
    if (xy.size() > 0)
    {
        const std::vector<double> values(xy);
        std::vector<Coordinate>   points;
        for (std::size_t i = 0; i + 1 < values.size(); i += 2)
        {
            points.emplace_back(values[i], values[i + 1]);
        }
        fins->setPoints(points);
    }
    return fins;
}

/// @p fins placed at @p offset from @p method of its parent.
[[nodiscard]] std::unique_ptr<FreeformFinSet> placed(std::unique_ptr<FreeformFinSet> fins,
                                                     AxialMethod method, double offset)
{
    fins->setAxialMethod(method);
    fins->setAxialOffset(offset);
    return fins;
}

/// The probe's onTransition(): Rocket > Stage > a conical transition 10 cm long of the radii 2 and
/// 3 cm (not automatic) > @p fins.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> onTransition(std::unique_ptr<FreeformFinSet> fins)
{
    auto        rocket     = std::make_unique<Rocket>();
    AxialStage& stage      = rocket->addChild(std::make_unique<AxialStage>());
    auto        transition = std::make_unique<Transition>();
    transition->setShapeType(TransitionShape::CONICAL);
    transition->setForeRadiusAutomatic(false);
    transition->setAftRadiusAutomatic(false);
    transition->setForeRadius(0.02);
    transition->setAftRadius(0.03);
    transition->setLength(0.1);
    stage.addChild(std::move(transition)).addChild(std::move(fins));
    rocket->enableEvents();
    return OpenRocketDocumentFactory::createDocumentFromRocket(std::move(rocket));
}

/// The probe's onNoseCone(): Rocket > Stage > an ogive nose cone 15 cm long of the base radius
/// 2.5 cm (not automatic) > @p fins.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> onNoseCone(std::unique_ptr<FreeformFinSet> fins)
{
    auto        rocket = std::make_unique<Rocket>();
    AxialStage& stage  = rocket->addChild(std::make_unique<AxialStage>());
    auto        nose   = std::make_unique<NoseCone>();
    nose->setShapeType(TransitionShape::OGIVE);
    nose->setBaseRadiusAutomatic(false);
    nose->setBaseRadius(0.025);
    nose->setLength(0.15);
    stage.addChild(std::move(nose)).addChild(std::move(fins));
    rocket->enableEvents();
    return OpenRocketDocumentFactory::createDocumentFromRocket(std::move(rocket));
}

/// One fin set of @p xy on the probe's body tube.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> onBodyTubeFreeform(
    std::initializer_list<double> xy = {})
{
    auto fins = makeFreeform(xy);
    return onBodyTube([&fins](BodyTube& body) { body.addChild(std::move(fins)); });
}

/// Two fin sets of @p first and @p second on the probe's body tube.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> onBodyTubeTwo(
    std::initializer_list<double> first, std::initializer_list<double> second)
{
    auto a = makeFreeform(first);
    auto b = makeFreeform(second);
    return onBodyTube([&a, &b](BodyTube& body) {
        body.addChild(std::move(a));
        body.addChild(std::move(b));
    });
}

/// The probe's design @p design (its built() part).
[[nodiscard]] std::unique_ptr<OpenRocketDocument> builtDesign(std::string_view design)
{
    if (design == "body tube csv")
    {
        return onBodyTubeFreeform({0, 0, 0.0123456, 0.031, 0.08, 0.031, 0.06, 0});
    }
    if (design == "body tube middle")
    {
        auto fins = placed(makeFreeform(), AxialMethod::MIDDLE, 0.02);
        return onBodyTube([&fins](BodyTube& body) { body.addChild(std::move(fins)); });
    }
    if (design == "transition")
    {
        return onTransition(
            placed(makeFreeform({0, 0, 0.02, 0.04, 0.05, 0.03, 0.04, 0}), AxialMethod::BOTTOM, 0));
    }
    if (design == "nose cone")
    {
        return onNoseCone(
            placed(makeFreeform({0, 0, 0.01, 0.03, 0.04, 0.03, 0.05, 0}), AxialMethod::BOTTOM, 0));
    }
    if (design == "four and five")
    {
        return onBodyTubeTwo({}, {0, 0, 0.115, 0.072, 0.255, 0.072, 0.255, 0.037, 0.150, 0});
    }
    if (design == "five and four")
    {
        return onBodyTubeTwo({0, 0, 0.02, 0.05, 0.06, 0.05, 0.07, 0.02, 0.05, 0}, {});
    }
    // "body tube hits", "table", "insert and delete", "drags", ...: the default outline.
    return onBodyTubeFreeform();
}

// ------------------------------------------------------------------------------- the replay

/// @p text as the probe prints the CSV: UTF-8 with '\\' doubled, '\r' and '\n' as "\\r" and "\\n".
[[nodiscard]] std::string escapedCsv(const QString& text)
{
    std::string escaped;
    for (const char c : QtStrings::toUtf8(text))
    {
        switch (c)
        {
            case '\\':
                escaped += "\\\\";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\n':
                escaped += "\\n";
                break;
            default:
                escaped += c;
                break;
        }
    }
    return escaped;
}

/// The probe's edits() and multi() of a design of its own, the steps read from the pin.
class FreeformReplay
{
public:
    FreeformReplay(OpenRocketDocument& document, ConfigDialogServices& services)
      : m_document(&document), m_services(&services)
    {
        for (RocketComponent& component : document.getRocket().subtree(true))
        {
            m_components.push_back(&component);
        }
    }

    [[nodiscard]] std::vector<std::string>& lines() noexcept { return m_lines; }

    /// The section header @p header and what follows it.
    void section(std::string_view header)
    {
        m_lines.emplace_back(header);
        if (header.starts_with("== panel | "))
        {
            openPanel(header.substr(std::string_view("== panel | ").size()), false);
        }
        else if (header.starts_with("== multi | "))
        {
            openPanel(header.substr(std::string_view("== multi | ").size()), true);
        }
        else if (header.starts_with("== edit | "))
        {
            step(header.substr(std::string_view("== edit | ").size()));
        }
        else if (header.starts_with("== multi edit | "))
        {
            step(header.substr(std::string_view("== multi edit | ").size()));
        }
    }

    /// Whether the next step's pin shows the figure's menu (Java's popup).
    void expectPopup(bool expected) noexcept { m_expectPopup = expected; }

    void close()
    {
        m_services->services().preferences().putBoolean(
            GuiPreferenceKeys::kUpdateRocketWhileDraggingPoint, false);
        m_panel.reset();
    }

private:
    void openPanel(std::string_view rest, bool multi)
    {
        const std::vector<std::string_view> parts = words(rest);
        const Result<int> settled = settleAutomaticDimensions(m_document->getRocket());
        EXPECT_TRUE(settled.has_value());
        m_main = m_components.at(static_cast<std::size_t>(integer(parts.at(0))));
        std::vector<RocketComponent*> others;
        if (multi)
        {
            m_other = m_components.at(static_cast<std::size_t>(integer(parts.at(2))));
            others.push_back(m_other);
        }
        m_panel.emplace(*m_document, *m_services, *m_main, others);
        QObject::connect(&panel().figurePointMenu(), &QMenu::aboutToShow,
                         &panel().figurePointMenu(), [this] { m_popup = true; });
        fit(panel());
        if (!multi)
        {
            std::vector<std::string> sorted;
            addShapeLines(panel(), sorted);
            addFigureLines(panel(), sorted);
            std::ranges::sort(sorted);
            std::ranges::move(sorted, std::back_inserter(m_lines));
        }
        state();
    }

    [[nodiscard]] FreeformFinSetConfig& panel() const
    {
        if (!m_panel.has_value())
        {
            bug("No panel yet");
        }
        return **m_panel;
    }

    void state()
    {
        addStateLines(panel(), dynamic_cast<const FreeformFinSet&>(*m_main),
                      dynamic_cast<const FreeformFinSet*>(m_other), m_lines);
    }

    /// The figure's pixel of the point (@p x, @p y) of the fin frame.
    [[nodiscard]] QPoint modelPixel(double x, double y) const
    {
        const FinPointFigure& figure = panel().finPointFigure();
        const QPoint          origin = figure.subjectOrigin();
        const double          scale  = figure.absoluteScale();
        return {origin.x() + static_cast<int>(MathUtil::javaRound(x * scale)),
                origin.y() - static_cast<int>(MathUtil::javaRound(y * scale))};
    }

    /// The figure's pixel of the fin point @p index plus (@p dx, @p dy).
    [[nodiscard]] QPoint pointPixel(int index, int dx, int dy) const
    {
        const Coordinate c = dynamic_cast<const FreeformFinSet&>(*m_main).getFinPoints().at(
            static_cast<std::size_t>(index));
        return modelPixel(c.x, c.y) + QPoint(dx, dy);
    }

    [[nodiscard]] QWidget& figureWidget() const { return panel().finPointFigure(); }

    void step(std::string_view step)
    {
        const std::vector<std::string_view> w    = words(step);
        const std::string_view              verb = w.at(0);
        if (verb == "hit" || verb == "hitAt")
        {
            hit(w);
        }
        else if (verb == "table")
        {
            const std::size_t textStart =
                std::string_view("table ").size() + w.at(1).size() + 1 + w.at(2).size() + 1;
            const QString text = QtStrings::fromUtf8(step.substr(std::min(textStart, step.size())));
            FinPointTableModel& model = panel().pointTableModel();
            static_cast<void>(
                model.setData(model.index(integer(w.at(1)), integer(w.at(2))), text, Qt::EditRole));
        }
        else if (verb == "select")
        {
            panel().selectPointRow(integer(w.at(1)));
        }
        else if (verb == "insert" || verb == "delete")
        {
            action(verb, integer(w.at(1)));
        }
        else
        {
            mouseOrButtonStep(step, w);
        }
        state();
    }

    void hit(std::span<const std::string_view> w)
    {
        const QPoint pixel = w[0] == "hit" ? pointPixel(integer(w[1]), integer(w[2]), integer(w[3]))
                                           : modelPixel(number(w[1]), number(w[2]));
        const FinPointFigure& figure = panel().finPointFigure();
        m_lines.push_back(
            std::format("hit index={} segment={}", figure.indexAt(pixel), figure.segmentAt(pixel)));
    }

    void action(std::string_view verb, int row)
    {
        panel().selectPointRow(row);
        QAction& act = verb == "insert" ? panel().insertPointAction() : panel().deletePointAction();
        m_lines.push_back(std::format("action {} enabled={}", verb, javaBool(act.isEnabled())));
        act.trigger();
    }

    void mouseOrButtonStep(std::string_view step, std::span<const std::string_view> w)
    {
        const std::string_view verb = w[0];
        if (verb == "press" || verb == "pressAt")
        {
            m_last = verb == "press" ? pointPixel(integer(w[1]), w.size() > 3 ? integer(w[2]) : 0,
                                                  w.size() > 3 ? integer(w[3]) : 0)
                                     : modelPixel(number(w[1]), number(w[2]));
            QTest::mousePress(&figureWidget(), Qt::LeftButton, Qt::NoModifier, m_last);
        }
        else if (verb == "drag")
        {
            drag(modelPixel(number(w[1]), number(w[2])), modifiers(w.subspan(3)));
        }
        else if (verb == "release")
        {
            QTest::mouseRelease(&figureWidget(), Qt::LeftButton, Qt::NoModifier, m_last);
        }
        else if (verb == "click")
        {
            click(step, w);
        }
        else
        {
            otherStep(w);
        }
    }

    [[nodiscard]] static Qt::KeyboardModifiers modifiers(std::span<const std::string_view> w)
    {
        Qt::KeyboardModifiers mods;
        if (hasWord(w, "shift"))
        {
            mods |= Qt::ShiftModifier;
        }
        if (hasWord(w, "ctrl"))
        {
            mods |= Qt::ControlModifier;
        }
        return mods;
    }

    void drag(QPoint pixel, Qt::KeyboardModifiers mods)
    {
        m_last             = pixel;
        QWidget&    figure = figureWidget();
        QMouseEvent move(QEvent::MouseMove, QPointF(pixel), QPointF(figure.mapToGlobal(pixel)),
                         Qt::NoButton, Qt::LeftButton, mods);
        QApplication::sendEvent(&figure, &move);
    }

    void click(std::string_view step, std::span<const std::string_view> w)
    {
        const Qt::MouseButton button = step.contains("button3") ? Qt::RightButton : Qt::LeftButton;
        const QPoint          pixel  = pointPixel(integer(w[1]), 0, 0);
        m_last                       = pixel;
        m_popup                      = false;
        {
            // Offscreen, a popup warns that the platform cannot grab the keyboard or the mouse.
            // Emplaced, not initialised from a conditional: MSVC wants a copy there, and the
            // warnings cannot be copied or moved.
            std::optional<Test::OffscreenPopupWarnings> warnings;
            if (m_expectPopup)
            {
                warnings.emplace();
            }
            QTest::mouseClick(&figureWidget(), button, modifiers(w.subspan(2)), pixel);
        }
        if (m_popup)
        {
            panel().figurePointMenu().hide();
            m_lines.emplace_back("popup shown");
        }
    }

    void otherStep(std::span<const std::string_view> w)
    {
        const std::string_view verb = w[0];
        if (verb == "undo")
        {
            panel().finPointUndoButton().click();
        }
        else if (verb == "redo")
        {
            panel().finPointRedoButton().click();
        }
        else if (verb == "live")
        {
            m_services->services().preferences().putBoolean(
                GuiPreferenceKeys::kUpdateRocketWhileDraggingPoint, w[1] == "true");
        }
        else if (verb == "units")
        {
            EXPECT_TRUE(unitGroup(UnitGroupId::LENGTH).setDefaultUnit(w[1]));
        }
        else if (verb == "csv")
        {
            m_lines.push_back("csv \"" + escapedCsv(panel().pointTableModel().csvText()) + "\"");
        }
        else
        {
            ADD_FAILURE() << "unknown step " << w[0];
        }
    }

    OpenRocketDocument*                               m_document;
    ConfigDialogServices*                             m_services;
    std::vector<RocketComponent*>                     m_components;
    RocketComponent*                                  m_main{nullptr};
    RocketComponent*                                  m_other{nullptr};
    std::optional<FactoryPanel<FreeformFinSetConfig>> m_panel;
    std::vector<std::string>                          m_lines;
    QPoint                                            m_last;
    bool                                              m_popup{false};
    bool                                              m_expectPopup{false};
};

/// The replay of the built design of @p pin, the steps from its section headers.
[[nodiscard]] std::vector<std::string> replayBuilt(const FreeformPanelPin& pin,
                                                   ConfigDialogServices&   services)
{
    const QtRocket::Test::DefaultUnitsGuard   units;
    const std::unique_ptr<OpenRocketDocument> document = builtDesign(pin.design);
    FreeformReplay                            replay(*document, services);
    for (std::size_t i = 0; i < pin.lines.size(); ++i)
    {
        if (pin.lines[i].starts_with("== "))
        {
            replay.expectPopup(i + 1 < pin.lines.size() &&
                               pin.lines[i + 1] == "popup IllegalComponentStateException");
            replay.section(pin.lines[i]);
        }
    }
    replay.close();
    return std::move(replay.lines());
}

/// The pins of @p source ("built") or of the others.
[[nodiscard]] std::vector<FreeformPanelPin> pinsOf(bool built)
{
    std::vector<FreeformPanelPin> pins;
    std::ranges::copy_if(
        kFreeformPanelPins, std::back_inserter(pins),
        [built](const FreeformPanelPin& pin) { return (pin.source == "built") == built; });
    return pins;
}

/// Services at the probe's 96 dots per inch.
class ProbeServices : public ConfigDialogServices
{
public:
    ProbeServices() { services().preferences().putInt(GuiPreferenceKeys::kDpi, kDpiTenths); }
};

// ------------------------------------------------------------------------------- the tests

class FreeformPanelGolden : public Test::GuiTest,
                            public ::testing::WithParamInterface<FreeformPanelPin>
{ };

class FreeformPanelEditsGolden : public Test::GuiTest,
                                 public ::testing::WithParamInterface<FreeformPanelPin>
{ };

TEST(FreeformPanelPinsGolden, ComeFromTheGoldensOpenRocket)
{
    EXPECT_EQ(kFreeformPanelPinsCommit, "5f164fd0e05d6e188857b0185601293b7a6e9eaf");
    EXPECT_EQ(kFreeformPanelPins.size(), 19U);
    EXPECT_EQ(pinsOf(false).size(), 3U);
    EXPECT_EQ(pinsOf(true).size(), 16U);
}

TEST_P(FreeformPanelGolden, ShowsAsOpenRocket)
{
    const FreeformPanelPin&                   pin = GetParam();
    const ProbeServices                       services;
    const std::unique_ptr<OpenRocketDocument> document =
        pinnedDesignDocument(pin.source, pin.design);
    ASSERT_NE(document, nullptr) << "no design " << pin.design;
    PanelReplay replay(*document, services, freeformLines());
    replay.panels();
    EXPECT_EQ(differences(replay.lines(), withOurDifferences(pin.lines)), "");
}

INSTANTIATE_TEST_SUITE_P(Designs, FreeformPanelGolden, ::testing::ValuesIn(pinsOf(false)),
                         [](const ::testing::TestParamInfo<FreeformPanelPin>& param) {
                             return std::string(param.param.testName);
                         });

TEST_P(FreeformPanelEditsGolden, EditAsOpenRocket)
{
    const FreeformPanelPin& pin = GetParam();
    ProbeServices           services;
    EXPECT_EQ(differences(replayBuilt(pin, services), expectedBuiltLines(pin)), "");
}

INSTANTIATE_TEST_SUITE_P(Designs, FreeformPanelEditsGolden, ::testing::ValuesIn(pinsOf(true)),
                         [](const ::testing::TestParamInfo<FreeformPanelPin>& param) {
                             return std::string(param.param.testName);
                         });

TEST_F(FreeformPanelBuiltGolden, OpenRocketMovesSharedPointsTwiceAfterTheHistorysRedo)
{
    // The pinned difference of FinPointSelection (kSharedPointsLines): Java's 2 cm shift.
    const std::span pins(kFreeformPanelPins);
    const auto      found = std::ranges::find_if(
        pins, [](const FreeformPanelPin& pin) { return pin.design == "four and five"; });
    ASSERT_NE(found, pins.end());
    const auto step = std::ranges::find(found->lines, kSharedPointsStep);
    ASSERT_NE(step, found->lines.end());
    EXPECT_EQ(*std::next(step), kSharedPointsLines.front().first);
}

TEST_F(FreeformPanelBuiltGolden, OpenRocketEndsItsCsvLinesWithCrLf)
{
    // The pinned difference of the CSV export (FinPointTableModel::csvText()).
    const std::span pins(kFreeformPanelPins);
    const auto      found = std::ranges::find_if(
        pins, [](const FreeformPanelPin& pin) { return pin.design == "body tube csv"; });
    ASSERT_NE(found, pins.end());
    EXPECT_NE(std::ranges::find(found->lines,
                                std::string_view(R"(csv "X / cm, Y / cm, \r\n0, 0, \r\n1.23, 3.1, )"
                                                 R"(\r\n8, 3.1, \r\n6, 0, \r\n")")),
              found->lines.end());
}

}  // namespace

}  // namespace QtRocket::Gui
