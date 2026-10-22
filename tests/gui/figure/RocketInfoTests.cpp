// Tests of figure/RocketInfo.h: its texts, its fonts and colours, and its layout as rules about the
// items' places (no pixels: every width is the font's own). What OpenRocket's RocketInfo writes
// and draws for the examples and the test rockets is compared in rocket_info_golden_tests.cpp.

#include "QtRocket/gui/figure/RocketInfo.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QLatin1Char>
#include <QPainter>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/figure/Caret.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/StabilityUnits.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "RecordingPaintDevice.h"
#include "ScopedApplicationPalette.h"
#include "rocket/TestRockets.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui
{

namespace
{

using RocketInfoTest = Test::GuiTest;
using Item           = RocketInfo::Item;
using Test::joinTextRuns;
using Test::RecordedPaint;
using Test::RecordingPaintDevice;
using Test::ScopedApplicationPalette;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
/// The visible rectangle of the layouts: not at the origin, so that the margins count.
constexpr QRect kVisible{20, 10, 1000, 700};
/// Positions are computed from the same numbers: they agree to the last bits.
constexpr double kTolerance = 1e-9;

/// The configuration of @p rocket that has motors.
[[nodiscard]] const FlightConfiguration& configurationWithMotors(const Rocket& rocket)
{
    for (const FlightConfigurationId& id : rocket.getIds())
    {
        if (rocket.getFlightConfiguration(id).hasMotors())
        {
            return rocket.getFlightConfiguration(id);
        }
    }
    ADD_FAILURE() << "no configuration with motors";
    return rocket.getSelectedConfiguration();
}

/// The overlay's frame for @p fonts on @p device, as RocketInfo.paint() computes it.
struct Frame
{
    double x1{0.0};
    double x2{0.0};
    double y1{0.0};
    double y2{0.0};
    double line{0.0};
};

[[nodiscard]] Frame frameOf(const RocketInfo::Fonts& fonts, const RecordingPaintDevice& device)
{
    const QFontMetricsF metrics(fonts.regular, &device);
    const double        line = metrics.lineSpacing() + metrics.descent();
    return {.x1   = kVisible.x() + 8.0,
            .x2   = kVisible.x() + kVisible.width() - 8.0,
            .y1   = kVisible.y() + line,
            .y2   = kVisible.y() + kVisible.height() - 8.0,
            .line = line};
}

/// The text of @p item as UTF-8.
[[nodiscard]] std::string textOf(const Item& item)
{
    return QtStrings::toUtf8(item.text);
}

/// The item of kind @p kind.
[[nodiscard]] const Item& itemOfKind(const std::vector<Item>& items, Item::Kind kind)
{
    const auto found =
        std::ranges::find_if(items, [kind](const Item& item) { return item.kind == kind; });
    if (found == items.end())
    {
        ADD_FAILURE() << "no caret";
        return items.front();
    }
    return *found;
}

/// Alpha III's overlay with numbers set: its configuration with motors.
struct AlphaInfo
{
    QtRocket::Test::TestEstesAlphaIII alpha;
    InMemoryPreferences               preferences;
    RocketInfo                        info{configurationWithMotors(*alpha.rocket), preferences};

    AlphaInfo()
    {
        info.setLength(0.273);
        info.setDiameter(0.0248);
        info.setCG(0.185);
        info.setCP(0.218);
        info.setMassWithMotors(0.0423);
        info.setMassWithoutMotors(0.0265);
    }
};

/// AlphaInfo's layout in kVisible on a recording device in the light palette, with the fonts,
/// the frame and the regular font's space.
struct AlphaLayout
{
    ScopedApplicationPalette light{ScopedApplicationPalette::light()};
    AlphaInfo                overlay;
    RecordingPaintDevice     device{QSize(1100, 800)};
    QFont                    ui{QApplication::font()};
    RocketInfo::Fonts        fonts{RocketInfo::fonts(ui, overlay.preferences)};
    Frame                    frame{frameOf(fonts, device)};
    std::vector<Item>        items{overlay.info.layout(kVisible, ui, &device)};
    int space{QFontMetrics(fonts.regular, &device).horizontalAdvance(QLatin1Char(' '))};

    /// The widest of the CG and the CP value, and a space.
    [[nodiscard]] double unitWidth() const
    {
        return std::max(items.at(5).width, items.at(6).width) + space;
    }
    /// The carets, labels and values: from the carets' x to the right margin.
    [[nodiscard]] double block() const
    {
        return unitWidth() + std::max(items.at(8).width, items.at(9).width) + RocketInfo::kCaretGap;
    }
};

/// Whether @p item stands @p row lines below the first row (its baseline, a caret's centre).
void expectOnTopRow(const Item& item, double row, const Frame& frame)
{
    EXPECT_NEAR(item.position.y(), frame.y1 + (row * frame.line), kTolerance) << textOf(item);
}

/// Whether @p item ends at @p x (its position and its width).
void expectEndsAt(const Item& item, double x)
{
    EXPECT_NEAR(item.position.x() + item.width, x, kTolerance) << textOf(item);
}

/// Whether the item @p index of @p layout is @p text at the left margin on the row @p index, in
/// the regular font and the text colour, measured as its visual bounds.
void expectLeftText(const AlphaLayout& layout, std::size_t index, const std::string& text)
{
    const Item& item = layout.items.at(index);
    EXPECT_EQ(textOf(item), text);
    EXPECT_EQ(item.position.x(), layout.frame.x1);
    expectOnTopRow(item, static_cast<double>(index), layout.frame);
    EXPECT_EQ(item.font, Item::FontRole::REGULAR);
    EXPECT_EQ(item.colour, ThemeColors::color(ThemeColors::Key::TEXT));
    const QRectF bounds =
        QFontMetricsF(layout.fonts.regular, &layout.device).tightBoundingRect(item.text);
    EXPECT_EQ(item.width, bounds.width());
    EXPECT_EQ(item.bounds, bounds.translated(item.position));
}

/// Whether the warning line @p line stands @p rowFromBottom lines above the bottom row, centred
/// on the axis of the widest line, @p widest wide, in the error colour.
void expectWarningLine(const Item& line, double rowFromBottom, double widest, const Frame& frame)
{
    EXPECT_EQ(line.colour, ThemeColors::color(ThemeColors::Key::ERROR)) << textOf(line);
    EXPECT_NEAR(line.position.y(), frame.y2 - (rowFromBottom * frame.line), kTolerance)
        << textOf(line);
    EXPECT_NEAR(line.position.x() + (line.width / 2), frame.x2 - (widest / 2), kTolerance)
        << textOf(line);
}

/// Whether the text record @p record draws the text item @p item.
void expectTextPainted(const RecordedPaint& record, const Item& item,
                       const RocketInfo::Fonts& fonts)
{
    ASSERT_EQ(record.kind, RecordedPaint::Kind::TEXT);
    EXPECT_EQ(record.text, item.text);
    EXPECT_EQ(record.transform.map(record.position), item.position);
    EXPECT_EQ(record.pen.color(), item.colour);
    const QFont& font = item.font == Item::FontRole::SMALL ? fonts.small : fonts.regular;
    EXPECT_EQ(record.font.pointSizeF(), font.pointSizeF());
}

/// Whether @p record fills the caret item @p item: a mark of radius 7 / 1.7 about its position,
/// in its colour.
void expectCaretPainted(const RecordedPaint& record, const Item& item)
{
    ASSERT_EQ(record.kind, RecordedPaint::Kind::PATH);
    EXPECT_EQ(record.brush.color(), item.colour);
    const QRectF bounds = record.devicePath().boundingRect();
    EXPECT_NEAR(bounds.center().x(), item.position.x(), kTolerance);
    EXPECT_NEAR(bounds.center().y(), item.position.y(), kTolerance);
    EXPECT_NEAR(bounds.width(), 2 * Caret::kRadius / RocketInfo::kCaretScale, kTolerance);
}

/// Whether @p record draws @p item.
void expectPainted(const RecordedPaint& record, const Item& item, const RocketInfo::Fonts& fonts)
{
    if (item.kind == Item::Kind::TEXT)
    {
        expectTextPainted(record, item, fonts);
    }
    else
    {
        expectCaretPainted(record, item);
    }
}

TEST_F(RocketInfoTest, StartsWithTheConfigurationAndTheDefaultMachNumber)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    InMemoryPreferences                     preferences;
    preferences.setDefaultMach(0.5);
    const RocketInfo info(alpha.rocket->getSelectedConfiguration(), preferences);
    EXPECT_EQ(info.mach(), 0.5);
    EXPECT_TRUE(std::isnan(info.aoa()));
    EXPECT_TRUE(std::isnan(info.theta()));
    EXPECT_EQ(info.cg(), 0.0);
    EXPECT_EQ(info.cp(), 0.0);
    EXPECT_EQ(info.length(), 0.0);
    EXPECT_EQ(info.diameter(), 0.0);
    EXPECT_EQ(info.massWithMotors(), 0.0);
    EXPECT_EQ(info.massWithoutMotors(), 0.0);
    EXPECT_TRUE(info.warnings().empty());
    EXPECT_TRUE(info.isShowWarnings());
    EXPECT_FALSE(info.customTextColor().isValid());
    EXPECT_EQ(QtStrings::toUtf8(info.rocketName()), alpha.rocket->getName());
    EXPECT_FALSE(info.hasMotors());
}

TEST_F(RocketInfoTest, TakesTheNameAndTheMotorsOfTheCurrentConfiguration)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    const InMemoryPreferences               preferences;
    RocketInfo                 info(alpha.rocket->getSelectedConfiguration(), preferences);
    const FlightConfiguration& motors = configurationWithMotors(*alpha.rocket);
    info.setCurrentConfig(motors);
    EXPECT_TRUE(info.hasMotors());
    const std::string original = alpha.rocket->getName();
    alpha.rocket->setName("Renamed");
    // Read when the configuration is set, not at each paint (a deviation: no configuration is
    // kept).
    EXPECT_EQ(QtStrings::toUtf8(info.rocketName()), original);
    info.setCurrentConfig(motors);
    EXPECT_EQ(QtStrings::toUtf8(info.rocketName()), "Renamed");
}

TEST_F(RocketInfoTest, WritesTheStabilityInBothUnitsUnlessOneWillDo)
{
    AlphaInfo                  overlay;
    RocketInfo&                info          = overlay.info;
    const FlightConfiguration& configuration = configurationWithMotors(*overlay.alpha.rocket);
    const auto                 primary       = StabilityUnits::stabilityUnits(configuration);
    const auto                 secondary = StabilityUnits::secondaryStabilityUnits(configuration);
    const double               margin    = 0.218 - 0.185;
    const std::string          calibers  = primary->getDefaultUnit().toStringUnit(margin);
    const std::string          percent   = secondary->getDefaultUnit().toStringUnit(margin);
    EXPECT_EQ(QtStrings::toUtf8(info.getStabilityCombined()), calibers + " / " + percent);

    // Switched off in the preferences.
    overlay.preferences.setDisplaySecondaryStability(false);
    EXPECT_EQ(QtStrings::toUtf8(info.getStabilityCombined()), calibers);
    overlay.preferences.setDisplaySecondaryStability(true);

    // No number: the primary unit's "N/A" alone.
    info.setCP(kNaN);
    EXPECT_EQ(QtStrings::toUtf8(info.getStabilityCombined()), "N/A");
    info.setCP(0.218);

    // The same unit both ways: once.
    {
        const QtRocket::Test::DefaultUnitsGuard units;
        ASSERT_TRUE(unitGroup(UnitGroupId::STABILITY).setDefaultUnit("%"));
        info.setCurrentConfig(configuration);
        EXPECT_EQ(QtStrings::toUtf8(info.getStabilityCombined()), percent);
    }
}

TEST_F(RocketInfoTest, WritesTheCgAndTheCpInTheDefaultLengthUnit)
{
    const AlphaInfo   overlay;
    const RocketInfo& info = overlay.info;
    EXPECT_EQ(QtStrings::toUtf8(info.getCg()), "18.5 cm");
    EXPECT_EQ(QtStrings::toUtf8(info.getCp()), "21.8 cm");
    const Unit* metres = unitGroup(UnitGroupId::LENGTH).getUnit("m");
    ASSERT_NE(metres, nullptr);
    EXPECT_EQ(QtStrings::toUtf8(info.getCg(*metres)), "0.185 m");
    EXPECT_EQ(QtStrings::toUtf8(info.getCp(*metres)), "0.218 m");
    const Unit* grams = unitGroup(UnitGroupId::MASS).getUnit("g");
    ASSERT_NE(grams, nullptr);
    EXPECT_EQ(QtStrings::toUtf8(info.getMassWithMotors(*grams)), "42.3 g");
    {
        const QtRocket::Test::DefaultUnitsGuard units;
        UnitGroup::setDefaultImperialUnits();
        EXPECT_EQ(QtStrings::toUtf8(info.getCp()), "8.583 in");
    }
}

TEST_F(RocketInfoTest, FontsFollowTheUiFontAndThePreference)
{
    InMemoryPreferences preferences;
    QFont               ui(QApplication::font());
    ui.setPointSizeF(12.0);
    ui.setBold(true);

    // getRocketInfoFontSize(): the UI size less 2, plus 3 per step of RocketInfoFontSize.
    RocketInfo::Fonts fonts = RocketInfo::fonts(ui, preferences);
    EXPECT_EQ(fonts.regular.pointSizeF(), 10.0);
    EXPECT_EQ(fonts.small.pointSizeF(), 8.5);
    EXPECT_EQ(fonts.bold.pointSizeF(), 11.5);
    EXPECT_FALSE(fonts.regular.bold());
    EXPECT_FALSE(fonts.small.bold());
    EXPECT_TRUE(fonts.bold.bold());
    EXPECT_EQ(fonts.regular.family(), ui.family());
    preferences.putChoice(PreferenceKeys::kRocketInfoFontSize, 1);
    EXPECT_EQ(RocketInfo::fonts(ui, preferences).regular.pointSizeF(), 13.0);
    preferences.putChoice(PreferenceKeys::kRocketInfoFontSize, 2);
    EXPECT_EQ(RocketInfo::fonts(ui, preferences).regular.pointSizeF(), 16.0);
    // Out of range: the default (small).
    preferences.putChoice(PreferenceKeys::kRocketInfoFontSize, 3);
    EXPECT_EQ(RocketInfo::fonts(ui, preferences).regular.pointSizeF(), 10.0);
    preferences.putChoice(PreferenceKeys::kRocketInfoFontSize, 0);

    // The UI size is an int of at least 8, and so is the overlay's.
    ui.setPointSizeF(12.75);
    EXPECT_EQ(RocketInfo::fonts(ui, preferences).regular.pointSizeF(), 10.0);
    ui.setPointSizeF(6.0);
    fonts = RocketInfo::fonts(ui, preferences);
    EXPECT_EQ(fonts.regular.pointSizeF(), 8.0);
    EXPECT_EQ(fonts.small.pointSizeF(), 6.5);
    // A pixel-sized UI font counts 0.75 points per pixel.
    ui.setPixelSize(20);
    EXPECT_EQ(RocketInfo::fonts(ui, preferences).regular.pointSizeF(), 13.0);
}

TEST_F(RocketInfoTest, LaysOutTheMainInfoAtTheTopLeft)
{
    const AlphaLayout layout;
    ASSERT_EQ(layout.items.size(), 13U);
    // The name, the length and diameter, the masses, a line apart from the margin.
    expectLeftText(layout, 0, "Estes Alpha III / Code Verification Rocket");
    expectLeftText(layout, 1, "Length 27.3 cm, max. diameter 2.48 cm");
    expectLeftText(layout, 2, "Mass with no motors 26.5 g");
    expectLeftText(layout, 3, "Mass with motors 42.3 g");
}

TEST_F(RocketInfoTest, LaysOutTheStabilityAtTheTopRight)
{
    const AlphaLayout layout;
    ASSERT_EQ(layout.items.size(), 13U);
    const Frame& frame = layout.frame;
    // The values end at the margin.
    const Item& stability = layout.items.at(4);
    const Item& cg        = layout.items.at(5);
    const Item& cp        = layout.items.at(6);
    EXPECT_EQ(textOf(stability), QtStrings::toUtf8(layout.overlay.info.getStabilityCombined()));
    EXPECT_EQ(textOf(cg), "18.5 cm");
    EXPECT_EQ(textOf(cp), "21.8 cm");
    expectEndsAt(stability, frame.x2);
    expectEndsAt(cg, frame.x2);
    expectEndsAt(cp, frame.x2);
    // The labels a space before the widest value (the stability's before its own).
    const Item& stabilityLabel = layout.items.at(7);
    const Item& cgLabel        = layout.items.at(8);
    const Item& cpLabel        = layout.items.at(9);
    EXPECT_EQ(textOf(stabilityLabel), "Stability:");
    EXPECT_EQ(textOf(cgLabel), "CG:");
    EXPECT_EQ(textOf(cpLabel), "CP:");
    expectEndsAt(stabilityLabel, frame.x2 - stability.width - layout.space);
    expectEndsAt(cgLabel, frame.x2 - layout.unitWidth());
    expectEndsAt(cpLabel, frame.x2 - layout.unitWidth());
    // On the first three rows, in the regular font and the text colour.
    expectOnTopRow(stability, 0, frame);
    expectOnTopRow(stabilityLabel, 0, frame);
    expectOnTopRow(cg, 1, frame);
    expectOnTopRow(cgLabel, 1, frame);
    expectOnTopRow(cp, 2, frame);
    expectOnTopRow(cpLabel, 2, frame);
    EXPECT_EQ(cpLabel.font, Item::FontRole::REGULAR);
    EXPECT_EQ(cpLabel.colour, ThemeColors::color(ThemeColors::Key::TEXT));
}

TEST_F(RocketInfoTest, LaysOutTheCaretsAndTheConditions)
{
    const AlphaLayout layout;
    ASSERT_EQ(layout.items.size(), 13U);
    const Frame& frame = layout.frame;
    // The carets 10 pixels before the widest label, 0.3 lines above their rows.
    const Item& cgCaret = layout.items.at(10);
    const Item& cpCaret = layout.items.at(11);
    EXPECT_EQ(cgCaret.kind, Item::Kind::CG_CARET);
    EXPECT_EQ(cpCaret.kind, Item::Kind::CP_CARET);
    EXPECT_EQ(cgCaret.colour, ThemeColors::color(ThemeColors::Key::CG));
    EXPECT_EQ(cpCaret.colour, ThemeColors::color(ThemeColors::Key::CP));
    EXPECT_NEAR(cgCaret.position.x(), frame.x2 - layout.block(), kTolerance);
    EXPECT_NEAR(cpCaret.position.x(), cgCaret.position.x(), kTolerance);
    expectOnTopRow(cgCaret, 0.7, frame);
    expectOnTopRow(cpCaret, 1.7, frame);
    EXPECT_NEAR(cgCaret.bounds.width(), 2 * Caret::kRadius / RocketInfo::kCaretScale, kTolerance);
    EXPECT_EQ(cgCaret.bounds.center(), cgCaret.position);

    // The conditions, small and dim, centred under the block.
    const Item& conditions = layout.items.at(12);
    EXPECT_EQ(textOf(conditions), "at M=0.300 \u200B");
    EXPECT_EQ(conditions.font, Item::FontRole::SMALL);
    EXPECT_EQ(conditions.colour, ThemeColors::color(ThemeColors::Key::TEXT_DIM));
    ASSERT_GT(layout.block(), conditions.width);
    EXPECT_NEAR(conditions.position.x(), frame.x2 - ((layout.block() + conditions.width) / 2),
                kTolerance);
    expectOnTopRow(conditions, 3, frame);
}

TEST_F(RocketInfoTest, ShowsTheMassWithMotorsOnlyWithMotors)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    const InMemoryPreferences               preferences;
    const RocketInfo        info(alpha.rocket->getSelectedConfiguration(), preferences);
    const std::vector<Item> items = info.layout(kVisible, QApplication::font(), nullptr);
    ASSERT_EQ(items.size(), 12U);
    EXPECT_EQ(textOf(items.at(2)), "Mass with no motors 0 g");
    EXPECT_EQ(items.at(3).text, info.getStabilityCombined());
}

TEST_F(RocketInfoTest, PlacesTheConditionsUnderTheBlock)
{
    AlphaInfo   overlay;
    RocketInfo& info = overlay.info;
    info.setMach(0.8);
    info.setAOA(5 * std::numbers::pi / 180);
    info.setTheta(std::numbers::pi / 6);
    const RecordingPaintDevice device(QSize(1100, 800));
    const QFont                ui    = QApplication::font();
    const Frame                frame = frameOf(RocketInfo::fonts(ui, overlay.preferences), device);
    const std::vector<Item>    items = info.layout(kVisible, ui, &device);
    const Item&                conditions = items.back();
    EXPECT_EQ(textOf(conditions), "at M=0.800 \u200B \u03B1=5\u00B0 \u0398=30\u00B0");
    // Centred under the block of the carets, labels and values when that is wider, else ending
    // at the margin.
    const Item&  cgCaret   = itemOfKind(items, Item::Kind::CG_CARET);
    const double block     = frame.x2 - cgCaret.position.x();
    const double expectedX = block > conditions.width ? frame.x2 - ((block + conditions.width) / 2)
                                                      : frame.x2 - conditions.width;
    EXPECT_NEAR(conditions.position.x(), expectedX, kTolerance);
    // Conditions much longer than the values end at the margin.
    info.setMach(0.123456789);
    info.setAOA(1.23456789);
    info.setTheta(2.3456789);
    const std::vector<Item> wider = info.layout(kVisible, ui, &device);
    ASSERT_GT(wider.back().width, block);
    EXPECT_NEAR(wider.back().position.x() + wider.back().width, frame.x2, kTolerance);
}

TEST_F(RocketInfoTest, CentresTheWarningsAtTheBottomRightOrCountsThem)
{
    const ScopedApplicationPalette light(ScopedApplicationPalette::light());
    AlphaInfo                      overlay;
    RocketInfo&                    info = overlay.info;
    WarningSet                     warnings;
    warnings.add(Warning::kDiameterDiscontinuity);
    warnings.add(Warning::fromString("A warning of some length to be wider than the others"));
    info.setWarnings(warnings);
    const RecordingPaintDevice device(QSize(1100, 800));
    const QFont                ui    = QApplication::font();
    const Frame                frame = frameOf(RocketInfo::fonts(ui, overlay.preferences), device);

    std::vector<Item> items = info.layout(kVisible, ui, &device);
    ASSERT_EQ(items.size(), 16U);
    const Item& title  = items.at(13);
    const Item& first  = items.at(14);
    const Item& second = items.at(15);
    EXPECT_EQ(textOf(title), "Warning:");
    EXPECT_EQ(textOf(first), Warning::kDiameterDiscontinuity.toString());
    EXPECT_EQ(textOf(second), "A warning of some length to be wider than the others");
    const double widest = std::max({title.width, first.width, second.width});
    expectWarningLine(title, 2, widest, frame);
    expectWarningLine(first, 1, widest, frame);
    expectWarningLine(second, 0, widest, frame);

    // Hidden: counted, on the bottom row, ending at the margin.
    info.setShowWarnings(false);
    items = info.layout(kVisible, ui, &device);
    ASSERT_EQ(items.size(), 14U);
    EXPECT_EQ(textOf(items.back()), "2 warning(s)");
    EXPECT_NEAR(items.back().position.y(), frame.y2, kTolerance);
    EXPECT_NEAR(items.back().position.x() + items.back().width, frame.x2, kTolerance);
}

TEST_F(RocketInfoTest, ACustomTextColourReplacesAllButTheWarnings)
{
    const ScopedApplicationPalette light(ScopedApplicationPalette::light());
    AlphaInfo                      overlay;
    RocketInfo&                    info = overlay.info;
    WarningSet                     warnings;
    warnings.add(Warning::kDiameterDiscontinuity);
    info.setWarnings(warnings);
    const QColor custom(10, 20, 30);
    info.setCustomTextColor(custom);
    for (const Item& item : info.layout(kVisible, QApplication::font(), nullptr))
    {
        if (item.kind != Item::Kind::TEXT)
        {
            continue;
        }
        const bool warning = textOf(item) == "Warning:" ||
                             textOf(item) == Warning::kDiameterDiscontinuity.toString();
        EXPECT_EQ(item.colour, warning ? ThemeColors::color(ThemeColors::Key::ERROR) : custom)
            << textOf(item);
    }
    info.setCustomTextColor(QColor());
    EXPECT_EQ(info.layout(kVisible, QApplication::font(), nullptr).front().colour,
              ThemeColors::color(ThemeColors::Key::TEXT));
}

TEST_F(RocketInfoTest, PaintsItsLayout)
{
    const ScopedApplicationPalette light(ScopedApplicationPalette::light());
    AlphaInfo                      overlay;
    RocketInfo&                    info = overlay.info;
    WarningSet                     warnings;
    warnings.add(Warning::kDiameterDiscontinuity);
    info.setWarnings(warnings);
    RecordingPaintDevice device(QSize(1100, 800));
    const QFont          ui = QApplication::font();
    {
        QPainter painter(&device);
        painter.setFont(ui);
        info.paint(painter, 123.0, kVisible);
    }
    const std::vector<Item>          items   = info.layout(kVisible, ui, &device);
    const RocketInfo::Fonts          fonts   = RocketInfo::fonts(ui, overlay.preferences);
    const std::vector<RecordedPaint> records = joinTextRuns(device.records());
    ASSERT_EQ(records.size(), items.size());
    for (std::size_t index = 0; index < items.size(); ++index)
    {
        SCOPED_TRACE(index);
        expectPainted(records.at(index), items.at(index), fonts);
    }
}

}  // namespace

}  // namespace QtRocket::Gui
