// OpenRocket's RocketFigure, painted and hit-tested headless (FigurePaintProbe, OpenRocket
// 5f164fd0e), against QtRocket's RocketFigure, RocketFigurePainter and carets in the same state:
// what is drawn with which pen or brush, where in device pixels, and which components a click
// finds. The pins are RocketFigurePaintPins.h, generated from the probe's output.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <QBrush>
#include <QColor>
#include <QList>
#include <QPainterPath>
#include <QPen>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QRegion>
#include <QSize>
#include <QWidget>
#include <Qt>
#include <QtTypes>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/figure/CGCaret.h"
#include "QtRocket/gui/figure/CPCaret.h"
#include "QtRocket/gui/figure/RocketFigure.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AutomaticDimensions.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/LineStyle.h"
#include "RecordingPaintDevice.h"
#include "RocketFigurePaintPins.h"
#include "ScopedApplicationPalette.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenExamples.h"
#include "rocket/TestRockets.h"

namespace QtRocket::Gui
{

namespace
{

using Test::PaintPin;
using Test::PaintPinKind;
using Test::PaintScenarioPin;
using Test::RecordedPaint;

/// Device pixels: QtRocket's and Java's arithmetic agree to the last bits but for sin and cos.
constexpr double kPixelTolerance = 1e-6;

/// The tree indices of @p text ("3 4").
[[nodiscard]] std::vector<int> indicesOf(std::string_view text)
{
    std::vector<int> indices;
    int              value  = 0;
    bool             inside = false;
    for (const char character : text)
    {
        if (character >= '0' && character <= '9')
        {
            value  = (value * 10) + (character - '0');
            inside = true;
        }
        else if (inside)
        {
            indices.push_back(value);
            value  = 0;
            inside = false;
        }
    }
    if (inside)
    {
        indices.push_back(value);
    }
    return indices;
}

/// The design of a scenario, loaded as the probe loaded it (an example without presets, settled; a
/// test rocket, settled; a new design).
class ScenarioDesign
{
public:
    explicit ScenarioDesign(const PaintScenarioPin& pin)
    {
        if (pin.source == "example")
        {
            const QtRocket::Test::GoldenExample* example =
                QtRocket::Test::findGoldenExample(pin.design);
            if (example == nullptr)
            {
                ADD_FAILURE() << "no example " << pin.design;
                return;
            }
            Result<LoadedDocument> loaded = QtRocket::Test::loadSettledGoldenExample(
                *example, QtRocket::Test::ExampleSource::ORIGINAL,
                QtRocket::Test::DesignFileEnvironment::Presets::NONE);
            if (!loaded)
            {
                ADD_FAILURE() << loaded.error().message;
                return;
            }
            m_document = std::move(loaded->document);
            return;
        }
        if (pin.source == "new")
        {
            m_document = OpenRocketDocumentFactory::createNewRocket();
            return;
        }
        for (const QtRocket::Test::TestRocketMaker& maker : QtRocket::Test::testRocketMakers())
        {
            if (maker.method == pin.design)
            {
                m_testRocket = maker.make();
                EXPECT_TRUE(settleAutomaticDimensions(*m_testRocket).has_value());
                return;
            }
        }
        ADD_FAILURE() << "no design " << pin.design;
    }

    [[nodiscard]] bool isLoaded() const noexcept
    {
        return m_document != nullptr || m_testRocket != nullptr;
    }

    [[nodiscard]] Rocket& rocket() const
    {
        return m_testRocket != nullptr ? *m_testRocket : m_document->getRocket();
    }

private:
    std::unique_ptr<OpenRocketDocument> m_document;
    std::unique_ptr<Rocket>             m_testRocket;
};

/// A scenario's figure in the probe's state: RocketPanel.create2DPreviewFigure()'s steps at 96 dots
/// per inch, the carets where the probe had them (RocketPanel.updateExtras()), the CP caret added
/// first.
class ScenarioFigure
{
public:
    explicit ScenarioFigure(const PaintScenarioPin& pin) : m_design(pin)
    {
        if (!m_design.isLoaded())
        {
            return;
        }
        for (RocketComponent& component : m_design.rocket().subtree(true))
        {
            m_order.push_back(&component);
        }
        modify(pin.modification);
        m_preferences.setShowMarkers(pin.showMarkers);

        m_figure = std::make_unique<RocketFigure>(m_design.rocket(), m_preferences, 96.0, nullptr);
        m_figure->setView(pin.view);
        m_figure->setRotation(pin.rotation);
        std::vector<const RocketComponent*> selection;
        for (const int index : indicesOf(pin.selection))
        {
            selection.push_back(m_order.at(static_cast<std::size_t>(index)));
        }
        m_figure->setSelection(selection);
        m_cg.setPosition(pin.cgX, pin.cgY);
        m_cp.setPosition(pin.cpX, pin.cpY);
        m_figure->addRelativeExtra(m_cp);
        m_figure->addRelativeExtra(m_cg);

        m_figure->scaleToFit(QSize(1000, 700));
        m_figure->updateFigure();
        m_figure->resize(m_figure->canvasSize());
        m_figure->updateFigure();
    }

    [[nodiscard]] bool          isReady() const noexcept { return m_figure != nullptr; }
    [[nodiscard]] RocketFigure& figure() const { return *m_figure; }

    /// The tree index of @p component, -1 when it is not in the tree.
    [[nodiscard]] int treeIndex(const RocketComponent* component) const
    {
        const auto found = std::ranges::find(m_order, component);
        return found == m_order.end() ? -1 : static_cast<int>(found - m_order.begin());
    }

private:
    /// FigurePaintProbe's modifications.
    void modify(std::string_view modification) const
    {
        if (modification == "styled")
        {
            m_order.at(3)->setColor(Color(10, 20, 30, 255));
            m_order.at(3)->setLineStyle(LineStyle::DOTTED);
            m_order.at(4)->setLineStyle(LineStyle::DASHDOT);
            m_order.at(2)->setColor(Color(200, 100, 50, 0));
        }
        else if (modification == "hide3")
        {
            m_order.at(3)->setVisible(false);
        }
    }

    InMemoryPreferences           m_preferences;
    ScenarioDesign                m_design;
    std::vector<RocketComponent*> m_order;
    CGCaret                       m_cg{0, 0};
    CPCaret                       m_cp{0, 0};
    std::unique_ptr<RocketFigure> m_figure;
};

/// Everything the figure painted into a recording device, without the background (the first
/// record, a rectangle filled in the background's colour).
[[nodiscard]] std::vector<RecordedPaint> paintedRecords(RocketFigure& figure)
{
    Test::RecordingPaintDevice device(figure.size());
    figure.render(&device, QPoint(), QRegion(), QWidget::RenderFlags());
    std::vector<RecordedPaint> records = device.records();
    if (!records.empty() && records.front().pen.style() == Qt::NoPen &&
        records.front().brush.color() == figure.backgroundColor())
    {
        records.erase(records.begin());
    }
    return records;
}

[[nodiscard]] bool near(double actual, double expected) noexcept
{
    return std::abs(actual - expected) <= kPixelTolerance;
}

/// The pin's dashes in pixels as QtRocket's pen gives them: none for Java's SOLID (10, 0).
[[nodiscard]] std::vector<double> pinnedDashes(const PaintPin& pin)
{
    const auto dashes = std::span(pin.dashes).first(static_cast<std::size_t>(pin.dashCount));
    if (dashes.size() == 2 && dashes[0] == 10.0 && dashes[1] == 0.0)
    {
        return {};
    }
    return {dashes.begin(), dashes.end()};
}

/// The pen's dashes in pixels (its pattern times its width); none for a solid pen.
[[nodiscard]] std::vector<double> penDashes(const QPen& pen)
{
    std::vector<double> dashes;
    if (pen.style() == Qt::SolidLine)
    {
        return dashes;
    }
    for (const qreal dash : pen.dashPattern())
    {
        dashes.push_back(dash * pen.widthF());
    }
    return dashes;
}

/// Success when @p record drew what @p pin says Java drew: the outline's pen (or the fill's brush)
/// and the bounds of the shape in device pixels.
[[nodiscard]] ::testing::AssertionResult paintsAsJava(const RecordedPaint& record,
                                                      const PaintPin&      pin)
{
    const QColor expected(pin.red, pin.green, pin.blue, pin.alpha);
    const bool outline = pin.kind == PaintPinKind::SHAPE || pin.kind == PaintPinKind::MOTOR_OUTLINE;
    if (outline)
    {
        const QPen& pen = record.pen;
        if (record.brush.style() != Qt::NoBrush || pen.color() != expected ||
            pen.widthF() != pin.width || !pen.isCosmetic() || pen.capStyle() != Qt::FlatCap ||
            pen.joinStyle() != Qt::BevelJoin)
        {
            return ::testing::AssertionFailure()
                   << "pen " << pen.color().name(QColor::HexArgb).toStdString() << " width "
                   << pen.widthF() << ", expected " << expected.name(QColor::HexArgb).toStdString()
                   << " width " << pin.width;
        }
        const std::vector<double> dashes         = penDashes(pen);
        const std::vector<double> expectedDashes = pinnedDashes(pin);
        if (!std::ranges::equal(dashes, expectedDashes, near))
        {
            return ::testing::AssertionFailure() << "dashes differ";
        }
    }
    else if (record.pen.style() != Qt::NoPen || record.brush.style() != Qt::SolidPattern ||
             record.brush.color() != expected)
    {
        return ::testing::AssertionFailure()
               << "fill " << record.brush.color().name(QColor::HexArgb).toStdString()
               << ", expected " << expected.name(QColor::HexArgb).toStdString();
    }
    if (!record.antialiased)
    {
        return ::testing::AssertionFailure() << "not antialiased";
    }
    const QRectF bounds = record.devicePath().controlPointRect();
    if (!near(bounds.x(), pin.x) || !near(bounds.y(), pin.y) || !near(bounds.width(), pin.w) ||
        !near(bounds.height(), pin.h))
    {
        return ::testing::AssertionFailure()
               << "bounds (" << bounds.x() << ", " << bounds.y() << ", " << bounds.width() << ", "
               << bounds.height() << "), expected (" << pin.x << ", " << pin.y << ", " << pin.w
               << ", " << pin.h << ")";
    }
    return ::testing::AssertionSuccess();
}

/// The order a run of shapes of equal display order is compared in: Java's order within a run
/// follows its hash maps.
[[nodiscard]] auto boundsKey(double x, double y, double w, double h)
{
    const auto rounded = [](double value) { return std::round(value * 1000.0); };
    return std::tuple(rounded(x), rounded(y), rounded(w), rounded(h));
}

void sortRunOfPins(std::vector<const PaintPin*>& pins)
{
    std::ranges::stable_sort(pins, [](const PaintPin* a, const PaintPin* b) {
        return boundsKey(a->x, a->y, a->w, a->h) < boundsKey(b->x, b->y, b->w, b->h);
    });
}

void sortRunOfRecords(std::vector<const RecordedPaint*>& records)
{
    std::ranges::stable_sort(records, [](const RecordedPaint* a, const RecordedPaint* b) {
        const QRectF ra = a->devicePath().controlPointRect();
        const QRectF rb = b->devicePath().controlPointRect();
        return boundsKey(ra.x(), ra.y(), ra.width(), ra.height()) <
               boundsKey(rb.x(), rb.y(), rb.width(), rb.height());
    });
}

/// Compares the records with the pins that QtRocket draws, a run of shapes of equal display
/// order as a set, everything else in order.
void expectPaintsAsJava(const std::vector<RecordedPaint>& records, std::span<const PaintPin> pins)
{
    std::vector<const PaintPin*> drawn;
    for (const PaintPin& pin : pins)
    {
        if (pin.drawn)
        {
            drawn.push_back(&pin);
        }
    }
    ASSERT_EQ(records.size(), drawn.size());
    std::size_t start = 0;
    while (start < drawn.size())
    {
        std::size_t end = start + 1;
        while (drawn[start]->kind == PaintPinKind::SHAPE && end < drawn.size() &&
               drawn[end]->kind == PaintPinKind::SHAPE &&
               drawn[end]->displayOrder == drawn[start]->displayOrder)
        {
            ++end;
        }
        std::vector<const PaintPin*> runPins(drawn.begin() + static_cast<std::ptrdiff_t>(start),
                                             drawn.begin() + static_cast<std::ptrdiff_t>(end));
        std::vector<const RecordedPaint*> runRecords;
        for (std::size_t index = start; index < end; ++index)
        {
            runRecords.push_back(&records[index]);
        }
        sortRunOfPins(runPins);
        sortRunOfRecords(runRecords);
        for (std::size_t index = 0; index < runPins.size(); ++index)
        {
            EXPECT_TRUE(paintsAsJava(*runRecords[index], *runPins[index]))
                << "paint " << (start + index) << " (component " << runPins[index]->component
                << ", display order " << runPins[index]->displayOrder << ")";
        }
        start = end;
    }
}

/// The caret offsets of the pins, and which of them do not lie on a boundary of the caret's area
/// (the CG caret's quarters end on the axes, where Java's and Qt's insideness rules differ).
constexpr std::array<QPointF, 10> kCaretOffsets{
    QPointF(3, 3),   QPointF(-3, -3), QPointF(3, -3),  QPointF(-3, 3),  QPointF(6.6, 0),
    QPointF(0, 6.6), QPointF(7.5, 0), QPointF(5.8, 0), QPointF(0, 0.5), QPointF(4.5, 0)};
constexpr std::size_t kCgRobustOffsets = 7;

/// The records that fill a caret (in the CG or the CP colour), in the order painted.
[[nodiscard]] std::vector<const RecordedPaint*> caretFills(
    const std::vector<RecordedPaint>& records)
{
    std::vector<const RecordedPaint*> fills;
    for (const RecordedPaint& record : records)
    {
        const QColor color = record.brush.color();
        if (record.brush.style() == Qt::SolidPattern &&
            (color == ThemeColors::color(ThemeColors::Key::CG) ||
             color == ThemeColors::color(ThemeColors::Key::CP)))
        {
            fills.push_back(&record);
        }
    }
    return fills;
}

/// Compares the caret @p fill drew with Java's at the pinned offsets from its centre.
void expectCaretAsJava(const RecordedPaint& fill, const Test::CaretPin& caret)
{
    const QPainterPath area   = fill.devicePath();
    const QPointF      centre = area.controlPointRect().center();
    EXPECT_NEAR(centre.x(), caret.centreX, kPixelTolerance);
    EXPECT_NEAR(centre.y(), caret.centreY, kPixelTolerance);
    const bool        isCg  = fill.brush.color() == ThemeColors::color(ThemeColors::Key::CG);
    const std::size_t count = isCg ? kCgRobustOffsets : kCaretOffsets.size();
    for (std::size_t sample = 0; sample < count; ++sample)
    {
        EXPECT_EQ(area.contains(centre + kCaretOffsets.at(sample)), caret.samples.at(sample) == '1')
            << (isCg ? "CG" : "CP") << " caret, offset " << kCaretOffsets.at(sample).x() << ", "
            << kCaretOffsets.at(sample).y();
    }
}

/// Compares the drawn carets' areas with Java's.
void expectCaretsAsJava(const std::vector<RecordedPaint>& records,
                        std::span<const Test::CaretPin>   carets)
{
    const std::vector<const RecordedPaint*> fills = caretFills(records);
    ASSERT_EQ(fills.size(), carets.size());
    for (std::size_t index = 0; index < carets.size(); ++index)
    {
        expectCaretAsJava(*fills[index], carets[index]);
    }
}

class RocketFigurePaintGolden : public Test::GuiTest,
                                public ::testing::WithParamInterface<PaintScenarioPin>
{ };

TEST(RocketFigurePaintPinsGolden, ComeFromTheGoldensOpenRocket)
{
    EXPECT_EQ(Gui::Test::kRocketFigurePaintPinsCommit, "5f164fd0e05d6e188857b0185601293b7a6e9eaf");
    EXPECT_EQ(Gui::Test::kPaintScenarios.size(), 20U);
}

TEST_P(RocketFigurePaintGolden, ScalesAsOpenRocket)
{
    const PaintScenarioPin& pin = GetParam();
    const ScenarioFigure    shown(pin);
    ASSERT_TRUE(shown.isReady());
    const RocketFigure& figure = shown.figure();
    EXPECT_DOUBLE_EQ(figure.userScale(), pin.userScale);
    EXPECT_DOUBLE_EQ(figure.absoluteScale(), pin.absoluteScale);
    EXPECT_EQ(figure.subjectOrigin(), QPoint(pin.originX, pin.originY));
    EXPECT_EQ(figure.size(), QSize(pin.width, pin.height));
}

TEST_P(RocketFigurePaintGolden, PaintsAsOpenRocket)
{
    const PaintScenarioPin& pin = GetParam();
    // OpenRocket's figure in its light theme (FlatLaf Light).
    const Gui::Test::ScopedApplicationPalette light(Gui::Test::ScopedApplicationPalette::light());
    ASSERT_EQ(ThemeColors::currentTheme(), ThemeColors::Theme::LIGHT);
    const ScenarioFigure shown(pin);
    ASSERT_TRUE(shown.isReady());
    const std::vector<RecordedPaint> records = paintedRecords(shown.figure());
    expectPaintsAsJava(records, pin.paints);
    expectCaretsAsJava(records, pin.carets);
}

TEST_P(RocketFigurePaintGolden, FindsTheComponentsOpenRocketFinds)
{
    const PaintScenarioPin& pin = GetParam();
    const ScenarioFigure    shown(pin);
    ASSERT_TRUE(shown.isReady());
    // Java hit-tests the shapes of the last paint.
    std::ignore = paintedRecords(shown.figure());
    for (const Gui::Test::HitPin& hit : pin.hits)
    {
        std::vector<int> found;
        for (const RocketComponent* component : shown.figure().componentsAt(QPointF(hit.x, hit.y)))
        {
            found.push_back(shown.treeIndex(component));
        }
        EXPECT_EQ(found, indicesOf(hit.components)) << "at " << hit.x << ", " << hit.y;
    }
}

INSTANTIATE_TEST_SUITE_P(Scenarios, RocketFigurePaintGolden,
                         ::testing::ValuesIn(Test::kPaintScenarios),
                         [](const ::testing::TestParamInfo<PaintScenarioPin>& paramInfo) {
                             std::string name(paramInfo.param.name);
                             std::ranges::replace(name, '-', '_');
                             return name;
                         });

}  // namespace

}  // namespace QtRocket::Gui
