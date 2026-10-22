// OpenRocket's design view numbers and information overlay, computed and painted headless
// (RocketInfoProbe, OpenRocket 5f164fd0e: RocketPanel.updateExtras() without the background
// simulation, RocketInfoContextHelper.computePhysics(), RocketInfo.paint()), against QtRocket's
// DesignPhysics and RocketInfo in the same state: the numbers, the texts, the carets' positions in
// five views and what the overlay draws, in which font and colour, on which row and against which
// margin. The pins are RocketInfoPins.h, generated from the probe's output.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/figure/DesignPhysics.h"
#include "QtRocket/gui/figure/RocketFigure.h"
#include "QtRocket/gui/figure/RocketInfo.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/AutomaticDimensions.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Error.h"
#include "RecordingPaintDevice.h"
#include "RocketInfoPins.h"
#include "ScopedApplicationPalette.h"
#include "aero/BarrowmanTestRockets.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenExamples.h"
#include "rocket/TestRockets.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui
{

namespace
{

using Test::CaretPositionPin;
using Test::InfoPaintColour;
using Test::InfoPaintFont;
using Test::InfoPaintKind;
using Test::InfoPaintPin;
using Test::joinTextRuns;
using Test::RecordedPaint;
using Test::RecordingPaintDevice;
using Test::RocketInfoScenarioPin;

/// QtRocket's numbers and Java's: the same arithmetic but for the order of the sums (the
/// components' order, plan section 6.4) and the last bits of sin and cos.
constexpr double kRelativeTolerance = 1e-9;
/// Rows of the layout: Java computes them in float.
constexpr double kRowTolerance = 1e-4;
/// The visible rectangle the probe painted into.
constexpr QRect kVisible{0, 0, 1000, 700};

/// Whether @p actual is @p expected within kRelativeTolerance (of at least 1), both NaN included.
void expectSameNumber(double actual, double expected, std::string_view what)
{
    if (std::isnan(expected))
    {
        EXPECT_TRUE(std::isnan(actual)) << what << ": " << actual;
        return;
    }
    EXPECT_NEAR(actual, expected, kRelativeTolerance * std::max(1.0, std::abs(expected))) << what;
}

/// The design of a scenario as the probe made it: an example loaded without presets and settled,
/// a TestRockets maker settled, or one of the probe's own designs (special:...), with the units
/// and preferences the scenario changes.
class ScenarioDesign
{
public:
    explicit ScenarioDesign(const RocketInfoScenarioPin& pin)
    {
        if (pin.source == "example")
        {
            loadExample(pin.design);
        }
        else if (pin.source == "testrocket")
        {
            for (const QtRocket::Test::TestRocketMaker& maker : QtRocket::Test::testRocketMakers())
            {
                if (maker.method == pin.design)
                {
                    m_rocket = maker.make();
                    EXPECT_TRUE(settleAutomaticDimensions(*m_rocket).has_value());
                }
            }
        }
        else
        {
            makeSpecial(pin.design);
        }
        if (m_rocket == nullptr && m_document == nullptr)
        {
            ADD_FAILURE() << "no design " << pin.name;
        }
    }

    [[nodiscard]] bool isLoaded() const noexcept
    {
        return m_rocket != nullptr || m_document != nullptr;
    }

    [[nodiscard]] Rocket& rocket() const
    {
        return m_rocket != nullptr ? *m_rocket : m_document->getRocket();
    }

    [[nodiscard]] const InMemoryPreferences& preferences() const noexcept { return m_preferences; }

private:
    void loadExample(std::string_view file)
    {
        const QtRocket::Test::GoldenExample* example = QtRocket::Test::findGoldenExample(file);
        if (example == nullptr)
        {
            return;
        }
        Result<LoadedDocument> loaded = QtRocket::Test::loadSettledGoldenExample(
            *example, QtRocket::Test::ExampleSource::ORIGINAL,
            QtRocket::Test::DesignFileEnvironment::Presets::NONE);
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
        m_document = std::move(loaded->document);
    }

    /// The probe's own designs (RocketInfoProbe.main()).
    void makeSpecial(std::string_view name)
    {
        if (name == "alpha-discontinuity")
        {
            // RocketInfoContextHelperTest.displayPhysicsIncludesGeometryWarnings's design.
            QtRocket::Test::TestEstesAlphaIII alpha;
            alpha.nose->setAftRadius(alpha.body->getOuterRadius() * 1.25);
            EXPECT_TRUE(settleAutomaticDimensions(*alpha.rocket).has_value());
            m_rocket = std::move(alpha.rocket);
        }
        else if (name == "body-tube-only")
        {
            // RocketInfoContextHelperTest.bodyTubeOnlyUsesSameFiniteCpAndStabilityInBothViews's
            // design, with the rocket's own configuration.
            m_rocket          = std::make_unique<Rocket>();
            AxialStage& stage = m_rocket->addChild(std::make_unique<AxialStage>());
            stage.addChild(std::make_unique<BodyTube>(1.0, 0.125));
            m_rocket->enableEvents();
        }
        else if (name == "new-design")
        {
            m_document = OpenRocketDocumentFactory::createNewRocket();
        }
        else if (name == "boosters-on-boosters-only-inner" ||
                 name == "boosters-on-boosters-toggled")
        {
            // A stage below two inactive ones, selected without an event and as
            // StageSelector.StageAction selects it (the rocket's event then moves the inner
            // boosters to where the selection puts them, in OpenRocket as here).
            QtRocket::Test::TestBoostersOnBoostersRocket nested;
            EXPECT_TRUE(settleAutomaticDimensions(*nested.rocket).has_value());
            FlightConfiguration& configuration = nested.rocket->getSelectedConfiguration();
            configuration.setOnlyStage(
                QtRocket::Test::TestBoostersOnBoostersRocket::kInnerStageNumber);
            if (name == "boosters-on-boosters-toggled")
            {
                nested.rocket->fireComponentChangeEvent(
                    ComponentChangeEvent::kAeromassChange | ComponentChangeEvent::kMotorChange |
                        ComponentChangeEvent::kTreeChangeChildren,
                    configuration.getId());
            }
            m_rocket = std::move(nested.rocket);
        }
        else
        {
            makeSimpleVariant(name);
        }
    }

    /// "A simple model rocket" with the CP parameters of the pin (DesignPhysics), other units or
    /// other preferences.
    void makeSimpleVariant(std::string_view name)
    {
        constexpr std::array kVariants{std::string_view("simple-cp-parameters"),
                                       std::string_view("simple-mach-only"),
                                       std::string_view("simple-imperial"),
                                       std::string_view("simple-stability-percent"),
                                       std::string_view("simple-no-secondary"),
                                       std::string_view("simple-stability-in-and-cal")};
        if (std::ranges::find(kVariants, name) == kVariants.end())
        {
            return;
        }
        constexpr std::array kUnitVariants{std::string_view("simple-imperial"),
                                           std::string_view("simple-stability-percent"),
                                           std::string_view("simple-stability-in-and-cal")};
        if (std::ranges::find(kUnitVariants, name) != kUnitVariants.end())
        {
            // Put UnitGroup.java's defaults, in which the test began, back at the end.
            m_units.emplace();
        }
        applySimpleVariant(name);
        loadExample("A simple model rocket.ork");
    }

    /// The units or preferences of the variant @p name.
    void applySimpleVariant(std::string_view name)
    {
        if (name == "simple-imperial")
        {
            UnitGroup::setDefaultImperialUnits();
        }
        else if (name == "simple-stability-percent")
        {
            setDefaultUnitOf(UnitGroupId::STABILITY, "%");
        }
        else if (name == "simple-no-secondary")
        {
            m_preferences.setDisplaySecondaryStability(false);
        }
        else if (name == "simple-stability-in-and-cal")
        {
            setDefaultUnitOf(UnitGroupId::SECONDARY_STABILITY, "cal");
            setDefaultUnitOf(UnitGroupId::STABILITY, "in");
        }
    }

    /// Makes the unit @p unit the default of the group @p group.
    static void setDefaultUnitOf(UnitGroupId group, std::string_view unit)
    {
        EXPECT_TRUE(unitGroup(group).setDefaultUnit(unit)) << unit;
    }

    std::optional<QtRocket::Test::DefaultUnitsGuard> m_units;
    std::unique_ptr<OpenRocketDocument>              m_document;
    std::unique_ptr<Rocket>                          m_rocket;
    InMemoryPreferences                              m_preferences;
};

/// A scenario's design shown in a RocketFigure at 96 dots per inch with its DesignPhysics, the
/// CP parameters of the pin set.
class ScenarioView
{
public:
    explicit ScenarioView(const RocketInfoScenarioPin& pin) : m_design(pin)
    {
        if (!m_design.isLoaded())
        {
            return;
        }
        m_figure  = std::make_unique<RocketFigure>(m_design.rocket(), m_design.preferences(), 96.0,
                                                   nullptr);
        m_physics = std::make_unique<DesignPhysics>(*m_figure, m_design.preferences());
        m_physics->setCPMach(pin.cpMach);
        m_physics->setCPAOA(pin.cpAOA);
        m_physics->setCPTheta(pin.cpTheta);
        m_physics->setCPRoll(pin.cpRoll);
    }

    ScenarioView(const ScenarioView&)            = delete;
    ScenarioView& operator=(const ScenarioView&) = delete;
    ScenarioView(ScenarioView&&)                 = delete;
    ScenarioView& operator=(ScenarioView&&)      = delete;
    ~ScenarioView()
    {
        m_physics.reset();
        m_figure.reset();
    }

    [[nodiscard]] bool               isReady() const noexcept { return m_physics != nullptr; }
    [[nodiscard]] const Preferences& preferences() const noexcept { return m_design.preferences(); }
    [[nodiscard]] RocketFigure&      figure() const { return *m_figure; }
    [[nodiscard]] DesignPhysics&     physics() const { return *m_physics; }

private:
    ScenarioDesign                 m_design;
    std::unique_ptr<RocketFigure>  m_figure;
    std::unique_ptr<DesignPhysics> m_physics;
};

/// The theme colour of @p colour.
[[nodiscard]] ThemeColors::Key keyOf(InfoPaintColour colour)
{
    switch (colour)
    {
        case InfoPaintColour::TEXT:
            return ThemeColors::Key::TEXT;
        case InfoPaintColour::TEXT_DIM:
            return ThemeColors::Key::TEXT_DIM;
        case InfoPaintColour::ERROR:
            return ThemeColors::Key::ERROR;
        case InfoPaintColour::CG:
            return ThemeColors::Key::CG;
        case InfoPaintColour::CP:
            return ThemeColors::Key::CP;
    }
    return ThemeColors::Key::TEXT;
}

/// The overlay's frame for the probe's visible rectangle with the application's font: the left
/// and right margins, the first and the bottom row, the line.
struct OverlayFrame
{
    double x1;
    double x2;
    double y1;
    double y2;
    double line;
};

[[nodiscard]] OverlayFrame overlayFrame(const RocketInfo::Fonts&    fonts,
                                        const RecordingPaintDevice& device)
{
    const QFontMetricsF metrics(fonts.regular, &device);
    const double        line = metrics.lineSpacing() + metrics.descent();
    return {.x1   = kVisible.x() + RocketInfo::kMargin,
            .x2   = kVisible.x() + kVisible.width() - RocketInfo::kMargin,
            .y1   = kVisible.y() + line,
            .y2   = kVisible.y() + kVisible.height() - RocketInfo::kMargin,
            .line = line};
}

/// What @p info paints into the probe's visible rectangle: the texts and the fills.
[[nodiscard]] std::vector<RecordedPaint> paintOverlay(RocketInfo&           info,
                                                      RecordingPaintDevice& device)
{
    device.clear();
    {
        QPainter painter(&device);
        painter.setFont(QApplication::font());
        info.paint(painter, 1.0, kVisible);
    }
    return joinTextRuns(device.records());
}

/// Compares the row of the text at @p position with @p pin's: the warnings stand on the rows
/// above the bottom one, everything else on the rows below the top one (the other distance
/// depends on the font).
void expectRowAsJava(QPointF position, const InfoPaintPin& pin, const OverlayFrame& frame)
{
    const bool   fromBottom = pin.colour == InfoPaintColour::ERROR;
    const double row        = fromBottom ? (frame.y2 - position.y()) / frame.line
                                         : (position.y() - frame.y1) / frame.line;
    EXPECT_NEAR(row, fromBottom ? pin.rowFromBottom : pin.row, kRowTolerance) << pin.text;
}

/// Compares the text @p record with @p pin.
void expectTextAsJava(const RecordedPaint& record, const InfoPaintPin& pin,
                      const RocketInfo::Fonts& fonts, const OverlayFrame& frame,
                      const RecordingPaintDevice& device)
{
    EXPECT_EQ(QtStrings::toUtf8(record.text), std::string(pin.text));
    const QFont& font = pin.font == InfoPaintFont::SMALL ? fonts.small : fonts.regular;
    EXPECT_EQ(record.font.pointSizeF(), font.pointSizeF()) << pin.text;
    EXPECT_EQ(record.pen.color(), QColor(pin.red, pin.green, pin.blue)) << pin.text;
    EXPECT_EQ(record.pen.color(),
              ThemeColors::themeColor(keyOf(pin.colour), ThemeColors::Theme::LIGHT));
    const QPointF position = record.transform.map(record.position);
    expectRowAsJava(position, pin, frame);
    const double width   = QFontMetricsF(font, &device).tightBoundingRect(record.text).width();
    const bool   atLeft  = std::abs(position.x() - frame.x1) < 1e-6;
    const bool   atRight = std::abs(position.x() + width - frame.x2) < 1e-6;
    EXPECT_EQ(atLeft, pin.leftAligned) << pin.text;
    EXPECT_EQ(atRight, pin.rightAligned) << pin.text;
}

/// Compares the caret fill @p record with @p pin: its colour and its row (the centre of its
/// bounds).
void expectCaretAsJava(const RecordedPaint& record, const InfoPaintPin& pin,
                       const OverlayFrame& frame)
{
    EXPECT_EQ(record.kind, RecordedPaint::Kind::PATH);
    EXPECT_EQ(record.brush.color(), QColor(pin.red, pin.green, pin.blue));
    const QRectF bounds = record.devicePath().boundingRect();
    EXPECT_NEAR((bounds.center().y() - frame.y1) / frame.line, pin.row, kRowTolerance);
    // Radius 7 / 1.7 pixels.
    EXPECT_NEAR(bounds.width(), 2 * 7.0 / RocketInfo::kCaretScale, 1e-6);
}

/// Compares what @p info paints with @p pins; @p preferences are the overlay's.
void expectPaintAsJava(RocketInfo& info, const Preferences& preferences,
                       std::span<const InfoPaintPin> pins)
{
    RecordingPaintDevice             device(kVisible.size());
    const std::vector<RecordedPaint> records = paintOverlay(info, device);
    const RocketInfo::Fonts          fonts   = RocketInfo::fonts(QApplication::font(), preferences);
    const OverlayFrame               frame   = overlayFrame(fonts, device);
    ASSERT_EQ(records.size(), pins.size());
    for (std::size_t index = 0; index < pins.size(); ++index)
    {
        const InfoPaintPin& pin = pins[index];
        SCOPED_TRACE(index);
        if (pin.kind == InfoPaintKind::TEXT)
        {
            ASSERT_EQ(records[index].kind, RecordedPaint::Kind::TEXT);
            expectTextAsJava(records[index], pin, fonts, frame, device);
        }
        else
        {
            expectCaretAsJava(records[index], pin, frame);
        }
    }
}

/// Compares the numbers of @p physics and its overlay with @p pin.
void expectNumbersAsJava(const DesignPhysics& physics, const RocketInfoScenarioPin& pin)
{
    const DesignPhysics::Physics& numbers = physics.physics();
    expectSameNumber(numbers.length, pin.length, "length");
    expectSameNumber(numbers.diameter, pin.diameter, "diameter");
    expectSameNumber(numbers.cp.x, pin.cp[0], "CP x");
    expectSameNumber(numbers.cp.y, pin.cp[1], "CP y");
    expectSameNumber(numbers.cp.z, pin.cp[2], "CP z");
    expectSameNumber(numbers.cp.weight, pin.cp[3], "CNa");
    expectSameNumber(numbers.cg.x, pin.cg[0], "CG x");
    expectSameNumber(numbers.cg.y, pin.cg[1], "CG y");
    expectSameNumber(numbers.cg.z, pin.cg[2], "CG z");
    expectSameNumber(numbers.cg.weight, pin.cg[3], "mass");
    expectSameNumber(numbers.massWithoutMotors, pin.massWithoutMotors, "mass without motors");

    const RocketInfo& info = physics.info();
    expectSameNumber(info.massWithMotors(), pin.massWithMotors, "info mass with motors");
    expectSameNumber(info.massWithoutMotors(), pin.massWithoutMotors, "info mass without motors");
    expectSameNumber(info.length(), pin.length, "info length");
    expectSameNumber(info.diameter(), pin.diameter, "info diameter");
    expectSameNumber(info.cg(), pin.infoCg, "info CG");
    expectSameNumber(info.cp(), pin.infoCp, "info CP");
    expectSameNumber(info.mach(), pin.mach, "Mach");
    expectSameNumber(info.aoa(), pin.aoa, "angle of attack");
    expectSameNumber(info.theta(), pin.theta, "wind direction");
    EXPECT_EQ(info.hasMotors(), pin.hasMotors);
}

/// Compares the texts of @p info (getStabilityCombined(), getCg(), getCp(), the warnings) with
/// @p pin.
void expectTextsAsJava(const RocketInfo& info, const RocketInfoScenarioPin& pin)
{
    EXPECT_EQ(QtStrings::toUtf8(info.getStabilityCombined()), std::string(pin.stability));
    EXPECT_EQ(QtStrings::toUtf8(info.getCg()), std::string(pin.cgText));
    EXPECT_EQ(QtStrings::toUtf8(info.getCp()), std::string(pin.cpText));
    std::vector<std::string> warnings;
    for (const Warning& warning : info.warnings())
    {
        warnings.push_back(warning.toString());
    }
    EXPECT_EQ(warnings, std::vector<std::string>(pin.warnings.begin(), pin.warnings.end()));
}

/// Shows @p view's figure in each view and rotation of @p pin and compares the carets.
void expectCaretsAsJava(const ScenarioView& view, const RocketInfoScenarioPin& pin)
{
    for (const CaretPositionPin& caret : pin.carets)
    {
        SCOPED_TRACE(caret.rotation);
        view.figure().setView(caret.view);
        view.figure().setRotation(caret.rotation);
        EXPECT_NEAR(view.figure().rotation(true), caret.offsetRotation, 1e-15);
        expectSameNumber(view.physics().cgCaret().x(), caret.cgX, "CG caret x");
        expectSameNumber(view.physics().cgCaret().y(), caret.cgY, "CG caret y");
        expectSameNumber(view.physics().cpCaret().x(), caret.cpX, "CP caret x");
        expectSameNumber(view.physics().cpCaret().y(), caret.cpY, "CP caret y");
    }
}

class RocketInfoGolden : public Test::GuiTest,
                         public ::testing::WithParamInterface<RocketInfoScenarioPin>
{ };

TEST(RocketInfoPinsGolden, ComeFromTheGoldensOpenRocket)
{
    EXPECT_EQ(Gui::Test::kRocketInfoPinsCommit, "5f164fd0e05d6e188857b0185601293b7a6e9eaf");
    EXPECT_EQ(Gui::Test::kRocketInfoScenarios.size(), 40U);
}

TEST_P(RocketInfoGolden, ComputesAsOpenRocket)
{
    const RocketInfoScenarioPin& pin = GetParam();
    const ScenarioView           view(pin);
    ASSERT_TRUE(view.isReady());
    {
        SCOPED_TRACE("numbers");
        expectNumbersAsJava(view.physics(), pin);
    }
    {
        SCOPED_TRACE("texts");
        expectTextsAsJava(view.physics().info(), pin);
    }
    {
        SCOPED_TRACE("carets");
        expectCaretsAsJava(view, pin);
    }
}

TEST_P(RocketInfoGolden, PaintsAsOpenRocket)
{
    const RocketInfoScenarioPin&              pin = GetParam();
    const Gui::Test::ScopedApplicationPalette light(Gui::Test::ScopedApplicationPalette::light());
    const ScenarioView                        view(pin);
    ASSERT_TRUE(view.isReady());
    RocketInfo& info = view.physics().info();
    {
        SCOPED_TRACE("warnings shown");
        expectPaintAsJava(info, view.preferences(), pin.paint);
    }
    if (!pin.paintWarningsHidden.empty())
    {
        SCOPED_TRACE("warnings hidden");
        view.physics().setShowWarnings(false);
        expectPaintAsJava(info, view.preferences(), pin.paintWarningsHidden);
    }
}

INSTANTIATE_TEST_SUITE_P(Scenarios, RocketInfoGolden,
                         ::testing::ValuesIn(Gui::Test::kRocketInfoScenarios),
                         [](const ::testing::TestParamInfo<RocketInfoScenarioPin>& param) {
                             return std::string(param.param.testName);
                         });

}  // namespace

}  // namespace QtRocket::Gui
