// The slider mapping of a number field (Java: DoubleModel's ValueSliderModel).
//
// The table is OpenRocket's: the probe SliderProbe.java of the bindings scout
// (handoff/m2-scout/evidence/m2-scout-bindings/java/slider-java.out, OpenRocket 5f164fd0e, JDK
// 17) set a slider of a length model without bounds to every 125th position, in mm and in
// inches, and printed the value the model got and the position the slider showed for it. The
// arithmetic is + - * /, sqrt and the unit's rounding, so the values are pinned exactly.

#include "QtRocket/gui/binding/SliderMapping.h"

#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <numbers>
#include <string>
#include <string_view>
#include <tuple>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using SliderMappingTest = Test::GuiTest;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

/// One point of the table: the slider position set, the value the model got, the position the
/// slider then showed.
struct SliderPoint
{
    int    position;
    double value;
    int    shownPosition;
};

/// One line of the table: the slider configuration of getSliderModel(min, mid, max) (or
/// getSliderModel(min, max) when mid is NaN), the unit and the nine points.
struct SliderLine
{
    double                     min;
    double                     mid;
    double                     max;
    std::string_view           unit;
    std::array<SliderPoint, 9> points;
};

constexpr double kNoMid = std::numeric_limits<double>::quiet_NaN();

// NOLINTBEGIN(modernize-use-designated-initializers,modernize-use-std-numbers): a table printed by
// the Java probe (slider-java.out), whose values near pi are OpenRocket's results
const std::array<SliderLine, 12> kSliderTable{{
    {0.0,
     0.04,
     0.2,
     "mm",
     {{{0, 0.0, 0},
       {125, 0.01, 125},
       {250, 0.02, 250},
       {375, 0.03, 375},
       {500, 0.04, 500},
       {625, 0.058, 627},
       {750, 0.09, 750},
       {875, 0.14, 880},
       {1000, 0.2, 1000}}}},
    {0.0,
     0.04,
     0.2,
     "in",
     {{{0, 0.0, 0},
       {125, 0.01016, 127},
       {250, 0.02032, 254},
       {375, 0.030479999999999997, 380},
       {500, 0.04064, 507},
       {625, 0.05841999999999999, 629},
       {750, 0.08889999999999999, 746},
       {875, 0.13716, 874},
       {1000, 0.20066, 1000}}}},
    {0.0,
     0.5,
     2.0,
     "mm",
     {{{0, 0.0, 0},
       {125, 0.12, 120},
       {250, 0.25, 250},
       {375, 0.38, 380},
       {500, 0.5, 500},
       {625, 0.6900000000000001, 626},
       {750, 1.0, 750},
       {875, 1.4000000000000001, 865},
       {1000, 2.0, 1000}}}},
    {0.0,
     0.5,
     2.0,
     "in",
     {{{0, 0.0, 0},
       {125, 0.12446, 124},
       {250, 0.24892, 248},
       {375, 0.381, 381},
       {500, 0.508, 507},
       {625, 0.6858, 624},
       {750, 0.9905999999999999, 746},
       {875, 1.4478, 877},
       {1000, 2.0065999999999997, 1000}}}},
    {0.0,
     0.1,
     0.2,
     "mm",
     {{{0, 0.0, 0},
       {125, 0.025, 125},
       {250, 0.05, 250},
       {375, 0.075, 374},
       {500, 0.1, 500},
       {625, 0.12, 600},
       {750, 0.15, 749},
       {875, 0.18, 899},
       {1000, 0.2, 1000}}}},
    {0.0,
     0.1,
     0.2,
     "in",
     {{{0, 0.0, 0},
       {125, 0.0254, 126},
       {250, 0.0508, 253},
       {375, 0.07619999999999999, 380},
       {500, 0.09906, 495},
       {625, 0.12446, 622},
       {750, 0.14986, 749},
       {875, 0.17526, 876},
       {1000, 0.20066, 1000}}}},
    {0.0,
     kNoMid,
     0.02,
     "mm",
     {{{0, 0.0, 0},
       {125, 0.0025, 125},
       {250, 0.005, 250},
       {375, 0.0075, 375},
       {500, 0.01, 500},
       {625, 0.012, 600},
       {750, 0.015, 750},
       {875, 0.018000000000000002, 900},
       {1000, 0.02, 1000}}}},
    {0.0,
     kNoMid,
     0.02,
     "in",
     {{{0, 0.0, 0},
       {125, 0.00254, 127},
       {250, 0.00508, 254},
       {375, 0.007619999999999999, 380},
       {500, 0.01016, 508},
       {625, 0.0127, 635},
       {750, 0.015239999999999998, 761},
       {875, 0.017779999999999997, 888},
       {1000, 0.02032, 1000}}}},
    {500.0,
     2000.0,
     10000.0,
     "mm",
     {{{0, 500.0, 0},
       {125, 880.0, 126},
       {250, 1200.0, 233},
       {375, 1600.0, 366},
       {500, 2000.0, 500},
       {625, 2800.0, 626},
       {750, 4400.0, 751},
       {875, 6800.0, 875},
       {1000, 10000.0, 1000}}}},
    {500.0,
     2000.0,
     10000.0,
     "in",
     {{{0, 508.0, 2},
       {125, 863.5999999999999, 121},
       {250, 1244.6, 248},
       {375, 1625.6, 375},
       {500, 2006.6, 502},
       {625, 2794.0, 626},
       {750, 4318.0, 746},
       {875, 6858.0, 878},
       {1000, 9906.0, 996}}}},
    {-std::numbers::pi,
     kNoMid,
     std::numbers::pi,
     "mm",
     {{{0, -3.1416, 0},
       {125, -2.3562, 124},
       {250, -1.5708, 249},
       {375, -0.7854, 374},
       {500, 0.0, 500},
       {625, 0.79, 625},
       {750, 1.6, 754},
       {875, 2.4, 881},
       {1000, 3.1, 993}}}},
    {-std::numbers::pi,
     kNoMid,
     std::numbers::pi,
     "in",
     {{{0, -3.1419799999999998, 0},
       {125, -2.3571199999999997, 124},
       {250, -1.5697199999999998, 250},
       {375, -0.7848599999999999, 375},
       {500, 0.0, 500},
       {625, 0.7874, 625},
       {750, 1.5748, 750},
       {875, 2.3622, 875},
       {1000, 3.048, 985}}}},
}};
// NOLINTEND(modernize-use-designated-initializers,modernize-use-std-numbers)

/// Java's getSliderModel(min, max) or getSliderModel(min, mid, max) for @p line.
[[nodiscard]] SliderMapping mappingOf(const SliderLine& line)
{
    return std::isnan(line.mid) ? SliderMapping::linear(line.min, line.max)
                                : SliderMapping::fromRange(line.min, line.mid, line.max);
}

/// The differences between the table and the slider of a length binding without bounds (the
/// probe's model), one line each.
[[nodiscard]] std::string sliderTableDifferences()
{
    std::string differences;
    for (const SliderLine& line : kSliderTable)
    {
        const UnitGroup& length = unitGroup(UnitGroupId::LENGTH);
        DoubleBinding    binding(0.0, length, -kInfinity, kInfinity);
        binding.setCurrentUnit(*length.getUnit(line.unit));
        const SliderMapping mapping = mappingOf(line);
        for (const SliderPoint& point : line.points)
        {
            binding.setSliderPosition(mapping, point.position);
            const int shown = binding.sliderPosition(mapping);
            if (binding.value() != point.value || shown != point.shownPosition)
            {
                differences +=
                    std::format("[{}, {}, {}] {}: {} -> {} -> {}, OpenRocket {} -> {}\n", line.min,
                                line.mid, line.max, line.unit, point.position, binding.value(),
                                shown, point.value, point.shownPosition);
            }
        }
    }
    return differences;
}

}  // namespace

TEST_F(SliderMappingTest, TheSliderOfALengthIsOpenRocketsAtEveryPointOfTheTable)
{
    EXPECT_EQ(sliderTableDifferences(), "");
}

TEST_F(SliderMappingTest, MinMidMaxIsLinearWhenMidIsTheCentre)
{
    // Java's getSliderModel(min, mid, max) compares with MathUtil.equals().
    const SliderMapping centred = SliderMapping::fromRange(0, 0.1, 0.2);
    EXPECT_TRUE(centred.isLinear());
    EXPECT_EQ(centred.getMin(), 0.0);
    EXPECT_EQ(centred.getMid(), 0.2);
    EXPECT_EQ(centred.getMax(), 0.2);
    EXPECT_EQ(centred.getLinearPosition(), 1.0);
    const SliderMapping nearlyCentred = SliderMapping::fromRange(0, 0.1 * (1 + 1e-9), 0.2);
    EXPECT_TRUE(nearlyCentred.isLinear());

    const SliderMapping quadratic = SliderMapping::fromRange(0, 0.04, 0.2);
    EXPECT_FALSE(quadratic.isLinear());
    EXPECT_EQ(quadratic.getMid(), 0.04);
    EXPECT_EQ(quadratic.getLinearPosition(), 0.5);
}

TEST_F(SliderMappingTest, AQuadraticMappingNeedsMinBelowMidUpToMaxAndAPositionInside)
{
    // Java: IllegalArgumentException "Bad arguments for ValueSliderModel".
    EXPECT_THROW(std::ignore = SliderMapping::quadratic(0, 0.5, 0, 1), BugError);
    EXPECT_THROW(std::ignore = SliderMapping::quadratic(0, 0.5, 2, 1), BugError);
    EXPECT_THROW(std::ignore = SliderMapping::quadratic(0, 0, 0.5, 1), BugError);
    EXPECT_THROW(std::ignore = SliderMapping::quadratic(0, 1, 0.5, 1), BugError);
    EXPECT_THROW(std::ignore = SliderMapping::fromRange(0, 1.5, 1), BugError);
    EXPECT_NO_THROW(std::ignore = SliderMapping::quadratic(0, 0.5, 1, 1));
}

TEST_F(SliderMappingTest, TheQuadraticPartJoinsTheLinearPartAtMid)
{
    const Unit&         mm      = *unitGroup(UnitGroupId::LENGTH).getUnit("mm");
    const SliderMapping mapping = SliderMapping::quadratic(0, 0.25, 0.05, 1.0);
    EXPECT_EQ(mapping.valueAt(250, mm), 0.05);
    EXPECT_EQ(mapping.valueAt(SliderMapping::kMaximum, mm), 1.0);
    EXPECT_EQ(mapping.positionOf(0.05, mm), 250);
    EXPECT_EQ(mapping.positionOf(0.0, mm), 0);
    EXPECT_EQ(mapping.positionOf(-1.0, mm), 0);
    EXPECT_EQ(mapping.positionOf(1.0, mm), SliderMapping::kMaximum);
    EXPECT_EQ(mapping.positionOf(2.0, mm), SliderMapping::kMaximum);
}

TEST_F(SliderMappingTest, ANaNValueIsPositionZero)
{
    // Java's (int) of NaN is 0 (MathUtil::javaIntCast); a static_cast would be undefined.
    const Unit&  mm  = *unitGroup(UnitGroupId::LENGTH).getUnit("mm");
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(SliderMapping::linear(0, 1).positionOf(nan, mm), 0);
    EXPECT_EQ(SliderMapping::quadratic(0, 0.5, 0.1, 1).positionOf(nan, mm), 0);
}

TEST_F(SliderMappingTest, AUnitWithANegativeMultiplierInvertsTheSlider)
{
    // No unit of OpenRocket's groups has a negative multiplier; Java's slider model handles one.
    const GeneralUnit   negative(-1, "neg");
    const SliderMapping mapping = SliderMapping::linear(0, 10);
    EXPECT_EQ(mapping.valueAt(0, negative), 10.0);
    EXPECT_EQ(mapping.valueAt(SliderMapping::kMaximum, negative), 0.0);
    EXPECT_EQ(mapping.valueAt(250, negative), 7.5);
    EXPECT_EQ(mapping.positionOf(10.0, negative), 0);
    EXPECT_EQ(mapping.positionOf(0.0, negative), SliderMapping::kMaximum);
    EXPECT_EQ(mapping.positionOf(7.5, negative), 250);
}

TEST_F(SliderMappingTest, NewBoundsMoveALinearMappingAndPutAQuadraticMidAtAThird)
{
    const Unit& mm = *unitGroup(UnitGroupId::LENGTH).getUnit("mm");

    SliderMapping linear = SliderMapping::linear(0, 0.02);
    linear.setBounds(0, 0.04);
    EXPECT_EQ(linear.getMax(), 0.04);
    EXPECT_EQ(linear.getMid(), 0.04);
    EXPECT_EQ(linear.valueAt(500, mm), 0.02);

    // Java's ValueSliderModel.stateChanged(): mid = (max - min) / 3, not min + (max - min) / 3.
    SliderMapping quadratic = SliderMapping::quadratic(0, 0.5, 0.5, 2.0);
    quadratic.setBounds(0, 3.0);
    EXPECT_EQ(quadratic.getMid(), 1.0);
    EXPECT_EQ(quadratic.getMax(), 3.0);
    EXPECT_EQ(quadratic.valueAt(500, mm), 1.0);
    EXPECT_EQ(quadratic.valueAt(SliderMapping::kMaximum, mm), 3.0);
    SliderMapping offset = SliderMapping::quadratic(1.0, 0.5, 2.0, 4.0);
    offset.setBounds(1.0, 7.0);
    EXPECT_EQ(offset.getMid(), 2.0);
}

}  // namespace QtRocket::Gui
