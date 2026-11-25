#include "QtRocket/gui/plot/PlotEventGraphics.h"

#include <optional>

#include <QColor>
#include <QRgb>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/plot/PlotColors.h"
#include "QtRocket/gui/util/SimulationIcons.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "ScopedApplicationPalette.h"
#include "plot/PlotReadback.h"

// OpenRocket has no JUnit test of EventGraphics; its colours are those of its EVENT_COLORS map
// (swing gui/plot/EventGraphics.java), its images those of EVENT_IMAGES and MESSAGE_IMAGES.

namespace QtRocket::Gui
{

namespace
{

using PlotEventGraphicsTest = Gui::Test::GuiTest;
using Type                  = FlightEvent::Type;
using Theme                 = ThemeColors::Theme;
using Palette               = Gui::Test::ScopedApplicationPalette;

[[nodiscard]] QRgb rgbOf(Type type)
{
    return PlotEventGraphics::eventColor(type).rgba();
}

[[nodiscard]] QRgb darkRgbOf(Type type)
{
    return PlotEventGraphics::eventColor(type, Theme::DARK).rgba();
}

}  // namespace

TEST_F(PlotEventGraphicsTest, TheColoursAreEventGraphicsEventColors)
{
    EXPECT_EQ(rgbOf(Type::LAUNCH), qRgb(255, 0, 0));
    EXPECT_EQ(rgbOf(Type::LIFTOFF), qRgb(0, 80, 196));
    EXPECT_EQ(rgbOf(Type::LAUNCHROD), qRgb(0, 100, 80));
    EXPECT_EQ(rgbOf(Type::IGNITION), qRgb(230, 130, 15));
    EXPECT_EQ(rgbOf(Type::BURNOUT), qRgb(80, 55, 40));
    EXPECT_EQ(rgbOf(Type::EJECTION_CHARGE), qRgb(80, 55, 40));
    EXPECT_EQ(rgbOf(Type::STAGE_SEPARATION), qRgb(80, 55, 40));
    EXPECT_EQ(rgbOf(Type::APOGEE), qRgb(15, 120, 15));
    EXPECT_EQ(rgbOf(Type::RECOVERY_DEVICE_DEPLOYMENT), qRgb(0, 0, 128));
    EXPECT_EQ(rgbOf(Type::GROUND_HIT), qRgb(0, 0, 0));
    EXPECT_EQ(rgbOf(Type::SIMULATION_END), qRgb(128, 0, 0));
    EXPECT_EQ(rgbOf(Type::TUMBLE), qRgb(196, 0, 255));
    EXPECT_EQ(rgbOf(Type::EXCEPTION), qRgb(255, 0, 0));
    EXPECT_EQ(rgbOf(Type::SIM_WARN), qRgb(127, 127, 0));
    EXPECT_EQ(rgbOf(Type::SIM_ABORT), qRgb(255, 0, 0));
    // DEFAULT_EVENT_COLOR for the type the map has no colour of.
    EXPECT_EQ(rgbOf(Type::ALTITUDE), qRgb(0, 0, 0));
}

TEST_F(PlotEventGraphicsTest, ALightThemeDrawsJavasColours)
{
    for (const Type type : FlightEvent::kAllTypes)
    {
        EXPECT_EQ(PlotEventGraphics::eventColor(type, Theme::LIGHT),
                  PlotEventGraphics::eventColor(type))
            << static_cast<int>(type);
    }
}

TEST_F(PlotEventGraphicsTest, ADarkThemeBrightensThemAsTheCurvesAndDrawsBlackAsText)
{
    // Twice java.awt.Color.brighter(): each channel divided by 0.7, a channel of 1 or 2 raised to
    // 3 first.
    EXPECT_EQ(darkRgbOf(Type::APOGEE), qRgb(30, 244, 30));
    EXPECT_EQ(darkRgbOf(Type::RECOVERY_DEVICE_DEPLOYMENT), qRgb(0, 0, 255));
    EXPECT_EQ(darkRgbOf(Type::IGNITION), qRgb(255, 255, 30));
    EXPECT_EQ(PlotEventGraphics::eventColor(Type::LIFTOFF, Theme::DARK),
              PlotColors::javaBrighter(PlotColors::javaBrighter(QColor::fromRgb(0, 80, 196))));
    const QColor darkText = ThemeColors::themeColor(ThemeColors::Key::TEXT, Theme::DARK);
    EXPECT_EQ(PlotEventGraphics::eventColor(Type::GROUND_HIT, Theme::DARK), darkText);
    EXPECT_EQ(PlotEventGraphics::eventColor(Type::ALTITUDE, Theme::DARK), darkText);
}

TEST_F(PlotEventGraphicsTest, APaletteChoosesTheThemeAsThePlots)
{
    EXPECT_EQ(PlotEventGraphics::eventColor(Type::APOGEE, Palette::light()).rgba(),
              qRgb(15, 120, 15));
    EXPECT_EQ(PlotEventGraphics::eventColor(Type::APOGEE, Palette::dark()).rgba(),
              qRgb(30, 244, 30));
}

TEST_F(PlotEventGraphicsTest, AMarkerIsDrawnAtJavasOpacity)
{
    const QColor marker = PlotEventGraphics::markerColor(Type::APOGEE, Palette::light());
    EXPECT_EQ(marker.rgb(), qRgb(15, 120, 15));
    // ValueMarker.setAlpha(0.7f).
    EXPECT_EQ(marker.alpha(), 179);
    EXPECT_EQ(PlotEventGraphics::markerColor(Type::GROUND_HIT, Palette::dark()).rgb(),
              ThemeColors::themeColor(ThemeColors::Key::TEXT, Theme::DARK).rgb());
}

TEST_F(PlotEventGraphicsTest, TheImagesAreEventGraphicsImages)
{
    using Gui::Test::iconImage;
    EXPECT_EQ(iconImage(PlotEventGraphics::eventIcon(Type::APOGEE, std::nullopt)),
              iconImage(SimulationIcons::eventIcon(Type::APOGEE)));
    // SIM_ABORT shares EXCEPTION's image.
    EXPECT_EQ(iconImage(PlotEventGraphics::eventIcon(Type::SIM_ABORT, std::nullopt)),
              iconImage(SimulationIcons::eventIcon(Type::EXCEPTION)));
    // A warning's image is its priority's.
    EXPECT_EQ(iconImage(PlotEventGraphics::eventIcon(Type::SIM_WARN, MessagePriority::HIGH)),
              iconImage(SimulationIcons::warningIcon(MessagePriority::HIGH)));
    EXPECT_EQ(iconImage(PlotEventGraphics::eventIcon(Type::SIM_WARN, MessagePriority::LOW)),
              iconImage(SimulationIcons::warningIcon(MessagePriority::LOW)));
    EXPECT_NE(iconImage(PlotEventGraphics::eventIcon(Type::SIM_WARN, MessagePriority::LOW)),
              iconImage(SimulationIcons::warningIcon(MessagePriority::HIGH)));
    // No image: a warning without a priority, ALTITUDE.
    EXPECT_TRUE(PlotEventGraphics::eventIcon(Type::SIM_WARN, std::nullopt).isNull());
    EXPECT_TRUE(PlotEventGraphics::eventIcon(Type::ALTITUDE, std::nullopt).isNull());
}

}  // namespace QtRocket::Gui
