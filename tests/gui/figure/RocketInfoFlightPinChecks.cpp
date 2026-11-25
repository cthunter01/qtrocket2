#include "RocketInfoFlightPinChecks.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <QApplication>
#include <QColor>
#include <QFontMetricsF>
#include <QString>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/figure/BackgroundSimulation.h"
#include "QtRocket/gui/figure/RocketInfo.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/unit/UnitGroup.h"
#include "RocketInfoFlightPins.h"

namespace QtRocket::Gui::Test
{

namespace
{

/// Rows and columns are computed from the same numbers in Java (in float) and here.
constexpr double kRowTolerance    = 1e-6;
constexpr double kColumnTolerance = 1e-9;

/// The rows of the block's numbers above its bottom one (FlightPinText::rowFromBottom).
constexpr int kApogeeRow       = 2;
constexpr int kVelocityRow     = 1;
constexpr int kAccelerationRow = 0;

/// Whether @p actual is @p expected, NaN included (stored values round-trip exactly).
void expectSameValue(double actual, double expected, std::string_view what)
{
    if (std::isnan(expected))
    {
        EXPECT_TRUE(std::isnan(actual)) << what << ": " << actual;
        return;
    }
    EXPECT_EQ(actual, expected) << what;
}

/// Whether every number of @p summary is NaN (FlightData.NaN_DATA).
[[nodiscard]] bool isNotAvailable(const RocketInfo::FlightSummary& summary)
{
    return std::isnan(summary.maxAltitude) && std::isnan(summary.maxVelocity) &&
           std::isnan(summary.maxMachNumber) && std::isnan(summary.maxAcceleration);
}

/// Compares the flight data of @p info with @p overlay's: none, "N/A" or the shown simulation's
/// maxima (@p shownMaxima).
void expectFlightDataAsJava(const RocketInfo& info, const FlightPinOverlay& overlay,
                            const std::optional<std::array<double, 4>>& shownMaxima)
{
    const std::optional<RocketInfo::FlightSummary>& data = info.flightData();
    EXPECT_EQ(data.has_value(), overlay.shown != FlightPinShown::NONE);
    if (overlay.shown == FlightPinShown::NAN_DATA)
    {
        EXPECT_TRUE(isNotAvailable(data.value_or(RocketInfo::FlightSummary{})));
    }
    if (overlay.shown == FlightPinShown::DATA)
    {
        EXPECT_TRUE(shownMaxima.has_value()) << "no maxima of the shown simulation";
        expectFlightPinMaxima(data.value_or(RocketInfo::FlightSummary{}),
                              shownMaxima.value_or(std::array<double, 4>{}));
    }
}

/// Compares the status line of @p info with @p overlay's: the shown simulation's text when it is
/// ABORTED, else none.
void expectStatusAsJava(const RocketInfo& info, const FlightPinOverlay& overlay)
{
    const bool aborted = overlay.status.value_or(std::string()) == "ABORTED";
    EXPECT_EQ(info.abortedStatus().has_value(), aborted);
    EXPECT_EQ(QtStrings::toUtf8(info.abortedStatus().value_or(QString())),
              aborted ? overlay.statusText.value_or(std::string()) : std::string());
}

/// The theme colour of @p key's name.
[[nodiscard]] std::optional<ThemeColors::Key> colourKeyOf(std::string_view key)
{
    if (key == "FLIGHTDATA_TEXT_ACTIVE")
    {
        return ThemeColors::Key::FLIGHTDATA_TEXT_ACTIVE;
    }
    if (key == "FLIGHTDATA_TEXT_INACTIVE")
    {
        return ThemeColors::Key::FLIGHTDATA_TEXT_INACTIVE;
    }
    if (key == "ERROR")
    {
        return ThemeColors::Key::ERROR;
    }
    if (key == "TEXT")
    {
        return ThemeColors::Key::TEXT;
    }
    ADD_FAILURE() << "no theme colour " << key;
    return std::nullopt;
}

/// The overlay's frame for the probe's visible rectangle: the left margin, the bottom row, the
/// line.
struct BottomFrame
{
    double x1;
    double y2;
    double line;
};

[[nodiscard]] BottomFrame bottomFrameOf(const Preferences& preferences)
{
    const RocketInfo::Fonts fonts = RocketInfo::fonts(QApplication::font(), preferences);
    const QFontMetricsF     metrics(fonts.regular);
    return {.x1   = kFlightPinVisible.x() + RocketInfo::kMargin,
            .y2   = kFlightPinVisible.y() + kFlightPinVisible.height() - RocketInfo::kMargin,
            .line = metrics.lineSpacing() + metrics.descent()};
}

/// The values' column: 10 pixels right of the widest label of the block's @p items.
[[nodiscard]] std::optional<double> valueColumnOf(const std::vector<RocketInfo::Item>& items,
                                                  std::span<const FlightPinText>       texts,
                                                  const BottomFrame&                   frame)
{
    double            widest = 0;
    std::size_t       labels = 0;
    const std::size_t count  = std::min(items.size(), texts.size());
    for (std::size_t index = 0; index < count; ++index)
    {
        // The labels: the left column's texts of the block's four rows, in the regular font.
        const FlightPinText& text = texts[index];
        if (text.leftColumn && text.font == "REGULAR" && text.text.ends_with(": "))
        {
            widest = std::max(widest, items.at(index).width);
            ++labels;
        }
    }
    if (labels == 0)
    {
        return std::nullopt;
    }
    return frame.x1 + widest + RocketInfo::kFlightValueGap;
}

/// The value of @p data's number @p rowFromBottom rows above the block's bottom one (the apogee
/// two, the maximum velocity with the Mach number one, the maximum acceleration none) as
/// RocketInfo writes it: in the default unit, "N/A" for NaN; none for another row.
[[nodiscard]] std::optional<QString> valueTextOf(const RocketInfo::FlightSummary& data,
                                                 int                              rowFromBottom)
{
    const QString notAvailable = QStringLiteral("N/A");
    if (rowFromBottom == kApogeeRow)
    {
        return std::isnan(data.maxAltitude)
                   ? notAvailable
                   : QtStrings::defaultUnitText(UnitGroupId::DISTANCE, data.maxAltitude);
    }
    if (rowFromBottom == kVelocityRow)
    {
        return std::isnan(data.maxVelocity)
                   ? notAvailable
                   : QtStrings::defaultUnitText(UnitGroupId::VELOCITY, data.maxVelocity) +
                         QStringLiteral("  (Mach ") +
                         QtStrings::unitText(unitGroup(UnitGroupId::COEFFICIENT).getDefaultUnit(),
                                             data.maxMachNumber) +
                         QStringLiteral(")");
    }
    if (rowFromBottom == kAccelerationRow)
    {
        return std::isnan(data.maxAcceleration)
                   ? notAvailable
                   : QtStrings::defaultUnitText(UnitGroupId::ACCELERATION, data.maxAcceleration);
    }
    return std::nullopt;
}

/// Compares the text of the flight item @p item with that of @p pin, a value of a fresh flight,
/// which no test pins: it must be that of @p data, the numbers the block shows (valueTextOf(); the
/// callers compare them with the simulation's), so that the block shows the flight's data, not
/// "N/A" or the numbers of an earlier flight.
void expectFreshFlightTextAsShown(const RocketInfo::Item& item, const FlightPinText& pin,
                                  const RocketInfo::FlightSummary& data)
{
    EXPECT_FALSE(pin.leftColumn) << "a fresh text in the labels' column";
    const std::optional<QString> value = valueTextOf(data, pin.rowFromBottom);
    EXPECT_TRUE(value.has_value()) << "a fresh text in row " << pin.rowFromBottom;
    EXPECT_EQ(QtStrings::toUtf8(item.text), QtStrings::toUtf8(value.value_or(QString())));
}

/// Compares the text of the flight item @p item with @p pin's (a value of a fresh flight: @p
/// data's, expectFreshFlightTextAsShown()).
void expectFlightTextContentAsJava(const RocketInfo::Item& item, const FlightPinText& pin,
                                   const RocketInfo::FlightSummary& data)
{
    EXPECT_EQ(item.kind, RocketInfo::Item::Kind::TEXT);
    if (pin.fresh)
    {
        expectFreshFlightTextAsShown(item, pin, data);
        return;
    }
    EXPECT_EQ(QtStrings::toUtf8(item.text), pin.text);
}

/// Compares the text, the font and the colour of the flight item @p item with @p pin (a fresh
/// value: with @p data's).
void expectFlightTextAsJava(const RocketInfo::Item& item, const FlightPinText& pin,
                            const RocketInfo::FlightSummary& data)
{
    expectFlightTextContentAsJava(item, pin, data);
    const bool bold = pin.font == "BOLD";
    EXPECT_EQ(item.font,
              bold ? RocketInfo::Item::FontRole::BOLD : RocketInfo::Item::FontRole::REGULAR);
    const QColor expected(pin.rgba.at(0), pin.rgba.at(1), pin.rgba.at(2), pin.rgba.at(3));
    EXPECT_EQ(item.colour, expected);
    const std::optional<ThemeColors::Key> key = colourKeyOf(pin.colour);
    EXPECT_EQ(key.has_value() ? ThemeColors::themeColor(*key, ThemeColors::Theme::LIGHT) : QColor(),
              expected);
}

/// Compares the row and the column of the flight item @p item with @p pin.
void expectFlightPlaceAsJava(const RocketInfo::Item& item, const FlightPinText& pin,
                             const BottomFrame& frame, std::optional<double> valueColumn)
{
    EXPECT_NEAR((frame.y2 - item.position.y()) / frame.line, pin.rowFromBottom, kRowTolerance);
    EXPECT_TRUE(pin.leftColumn || valueColumn.has_value());
    const double column = pin.leftColumn ? frame.x1 : valueColumn.value_or(0.0);
    EXPECT_NEAR(item.position.x(), column, kColumnTolerance);
}

/// Compares the flight item @p item with @p pin (a fresh value: with @p data's).
void expectFlightItemAsJava(const RocketInfo::Item& item, const FlightPinText& pin,
                            const RocketInfo::FlightSummary& data, const BottomFrame& frame,
                            std::optional<double> valueColumn)
{
    SCOPED_TRACE(pin.fresh ? std::string("(a value of the fresh flight)") : pin.text);
    expectFlightTextAsJava(item, pin, data);
    expectFlightPlaceAsJava(item, pin, frame, valueColumn);
}

}  // namespace

std::array<double, 4> flightPinMaximaOf(const RocketInfo::FlightSummary& summary) noexcept
{
    return {summary.maxAltitude, summary.maxVelocity, summary.maxMachNumber,
            summary.maxAcceleration};
}

std::optional<std::array<double, 4>> shownFlightPinMaxima(const RocketInfoFlightPin& pin)
{
    const std::optional<std::size_t>& shown = pin.overlay.shownSimulation;
    if (!shown.has_value() || *shown >= pin.simulations.size())
    {
        return std::nullopt;
    }
    return pin.simulations.at(*shown).maxima;
}

void expectFlightPinMaxima(const RocketInfo::FlightSummary& summary,
                           const std::array<double, 4>&     maxima)
{
    expectSameValue(summary.maxAltitude, maxima.at(0), "apogee");
    expectSameValue(summary.maxVelocity, maxima.at(1), "maximum velocity");
    expectSameValue(summary.maxMachNumber, maxima.at(2), "maximum Mach number");
    expectSameValue(summary.maxAcceleration, maxima.at(3), "maximum acceleration");
}

void expectFlightOverlayAsJava(const RocketInfo& info, std::string_view configurationName,
                               const FlightPinOverlay&                     overlay,
                               const std::optional<std::array<double, 4>>& shownMaxima)
{
    EXPECT_EQ(info.isCalculatingData(), overlay.calculating);
    EXPECT_EQ(QtStrings::toUtf8(info.configurationName()), configurationName);
    expectFlightDataAsJava(info, overlay, shownMaxima);
    expectStatusAsJava(info, overlay);
}

std::vector<RocketInfo::Item> flightBlockItemsOf(RocketInfo& info)
{
    const std::optional<RocketInfo::FlightSummary> data        = info.flightData();
    const bool                                     calculating = info.isCalculatingData();
    info.setFlightData(std::nullopt);
    info.setCalculatingData(false);
    const std::size_t baseline =
        info.layout(kFlightPinVisible, QApplication::font(), nullptr).size();
    info.setFlightData(data);
    info.setCalculatingData(calculating);
    std::vector<RocketInfo::Item> items =
        info.layout(kFlightPinVisible, QApplication::font(), nullptr);
    items.erase(items.begin(), items.begin() + static_cast<std::ptrdiff_t>(baseline));
    return items;
}

void expectFlightBlockAsJava(RocketInfo& info, const Preferences& preferences,
                             std::span<const FlightPinText> texts)
{
    const std::vector<RocketInfo::Item> items = flightBlockItemsOf(info);
    ASSERT_EQ(items.size(), texts.size());
    const BottomFrame               frame       = bottomFrameOf(preferences);
    const std::optional<double>     valueColumn = valueColumnOf(items, texts, frame);
    const RocketInfo::FlightSummary data = info.flightData().value_or(RocketInfo::FlightSummary{});
    for (std::size_t index = 0; index < items.size(); ++index)
    {
        SCOPED_TRACE(index);
        expectFlightItemAsJava(items.at(index), texts[index], data, frame, valueColumn);
    }
}

std::vector<std::size_t> scheduledFlightPinSimulations(const BackgroundSimulation& background,
                                                       const OpenRocketDocument&   document)
{
    std::vector<std::shared_ptr<Simulation>> simulations;
    if (background.runningSimulation() != nullptr)
    {
        simulations.push_back(background.runningSimulation());
    }
    const std::vector<std::shared_ptr<Simulation>> chain = background.chain();
    simulations.insert(simulations.end(), chain.begin(), chain.end());
    std::vector<std::size_t> indices;
    indices.reserve(simulations.size());
    std::ranges::transform(
        simulations, std::back_inserter(indices),
        [&document](const std::shared_ptr<Simulation>& simulation) {
            return document.getSimulationIndex(*simulation).value_or(document.getSimulationCount());
        });
    return indices;
}

}  // namespace QtRocket::Gui::Test
