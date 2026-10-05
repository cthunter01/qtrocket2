#include "QtRocket/simulation/PlotAppearance.h"

#include <optional>
#include <type_traits>

#include <gtest/gtest.h>

#include "QtRocket/util/Color.h"
#include "QtRocket/util/LineStyle.h"

namespace
{

using QtRocket::Color;
using QtRocket::LineStyle;
using QtRocket::PlotAppearance;

// A value type: a copy is Java's clone().
static_assert(std::is_trivially_copyable_v<PlotAppearance>);
static_assert(!std::is_default_constructible_v<PlotAppearance>);

// OpenRocket has no test of PlotAppearance; the values are those of
// probes/tier8b-status/SimulationProbe.java ("plot appearances").

TEST(PlotAppearance, HoldsAColorAndALineStyle)
{
    PlotAppearance appearance(Color{10, 20, 30}, LineStyle::DASHDOT);
    EXPECT_EQ(appearance.getColor(), (Color{10, 20, 30}));
    EXPECT_EQ(appearance.getLineStyle(), LineStyle::DASHDOT);

    appearance.setColor(Color{1, 2, 3, 4});
    appearance.setLineStyle(LineStyle::DOTTED);
    EXPECT_EQ(appearance.getColor(), (Color{1, 2, 3, 4}));
    EXPECT_EQ(appearance.getLineStyle(), LineStyle::DOTTED);

    appearance.setColor(std::nullopt);
    appearance.setLineStyle(std::nullopt);
    EXPECT_FALSE(appearance.getColor().has_value());
    EXPECT_FALSE(appearance.getLineStyle().has_value());
}

TEST(PlotAppearance, IsEmptyOnlyWithoutColorAndWithoutLineStyle)
{
    // "empty truefalsefalse"
    EXPECT_TRUE(PlotAppearance(std::nullopt, std::nullopt).isEmpty());
    EXPECT_FALSE(PlotAppearance(Color{1, 2, 3}, std::nullopt).isEmpty());
    EXPECT_FALSE(PlotAppearance(std::nullopt, LineStyle::SOLID).isEmpty());
    EXPECT_FALSE(PlotAppearance(Color{1, 2, 3}, LineStyle::SOLID).isEmpty());
}

TEST(PlotAppearance, TheFactoryDefaultIsASolidLineWithoutAColor)
{
    // "factory default: color null style SOLID empty false"
    const PlotAppearance factory = PlotAppearance::factoryDefault();
    EXPECT_FALSE(factory.getColor().has_value());
    EXPECT_EQ(factory.getLineStyle(), LineStyle::SOLID);
    EXPECT_FALSE(factory.isEmpty());
    EXPECT_EQ(factory, PlotAppearance(std::nullopt, LineStyle::SOLID));
}

TEST(PlotAppearance, EqualsComparesTheColorAndTheLineStyle)
{
    // "equals: true false false true"
    const PlotAppearance p1(Color{1, 2, 3}, LineStyle::DASHED);
    EXPECT_EQ(p1, PlotAppearance(Color{1, 2, 3, 255}, LineStyle::DASHED)) << "an opaque color";
    EXPECT_NE(p1, PlotAppearance(std::nullopt, LineStyle::DASHED));
    EXPECT_NE(p1, PlotAppearance(Color{1, 2, 3}, std::nullopt));
    EXPECT_EQ(PlotAppearance(std::nullopt, std::nullopt),
              PlotAppearance(std::nullopt, std::nullopt));
    EXPECT_NE(p1, PlotAppearance(Color{1, 2, 4}, LineStyle::DASHED));
    EXPECT_NE(p1, PlotAppearance(Color{1, 2, 3, 254}, LineStyle::DASHED));
    EXPECT_NE(p1, PlotAppearance(Color{1, 2, 3}, LineStyle::DOTTED));
}

TEST(PlotAppearance, ACopyIsItsOwnValue)
{
    // Java: clone() copies the color.
    const PlotAppearance original(Color{1, 2, 3}, LineStyle::DASHED);
    PlotAppearance       copy = original;
    EXPECT_EQ(copy, original);
    copy.setColor(Color{9, 9, 9});
    copy.setLineStyle(LineStyle::SOLID);
    EXPECT_EQ(original.getColor(), (Color{1, 2, 3}));
    EXPECT_EQ(original.getLineStyle(), LineStyle::DASHED);
    EXPECT_NE(copy, original);
}

}  // namespace
