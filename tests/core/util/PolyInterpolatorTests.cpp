#include "QtRocket/util/PolyInterpolator.h"

#include <array>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

namespace
{

using QtRocket::PolyInterpolator;

/// The coefficients of the derivative of a polynomial given highest order term first.
std::vector<double> derivative(std::span<const double> coefficients)
{
    std::vector<double> result;
    const std::size_t   n = coefficients.size();
    for (std::size_t i = 0; i + 1 < n; ++i)
    {
        result.push_back(coefficients[i] * static_cast<double>(n - 1 - i));
    }
    return result;
}

// PolyInterpolatorTest.oldMainTest, first block: two values and two derivatives
TEST(PolyInterpolator, OldMainTestValuesAndDerivatives)
{
    const PolyInterpolator    p0{{0.6, 1.1}, {0.6, 1.1}};
    const std::vector<double> r0 = p0.interpolator({1.5, 1.6, 2, -3});

    constexpr std::array<double, 51> kAnswer0 = {
        /* x=0.60 */ 1.4999999999999987, /* x=0.61 */ 1.5199143999999984,
        /* x=0.62 */ 1.5396351999999984, /* x=0.63 */ 1.5591287999999988,
        /* x=0.64 */ 1.578361599999998,  /* x=0.65 */ 1.597299999999998,
        /* x=0.66 */ 1.6159103999999977, /* x=0.67 */ 1.6341591999999976,
        /* x=0.68 */ 1.6520127999999976, /* x=0.69 */ 1.669437599999997,
        /* x=0.70 */ 1.6863999999999977, /* x=0.71 */ 1.702866399999997,
        /* x=0.72 */ 1.7188031999999964, /* x=0.73 */ 1.7341767999999966,
        /* x=0.74 */ 1.7489535999999957, /* x=0.75 */ 1.7630999999999966,
        /* x=0.76 */ 1.7765823999999952, /* x=0.77 */ 1.7893671999999965,
        /* x=0.78 */ 1.8014207999999958, /* x=0.79 */ 1.8127095999999945,
        /* x=0.80 */ 1.8231999999999942, /* x=0.81 */ 1.8328583999999952,
        /* x=0.82 */ 1.841651199999994,  /* x=0.83 */ 1.849544799999994,
        /* x=0.84 */ 1.856505599999993,  /* x=0.85 */ 1.8624999999999927,
        /* x=0.86 */ 1.8674943999999924, /* x=0.87 */ 1.8714551999999918,
        /* x=0.88 */ 1.8743487999999924, /* x=0.89 */ 1.876141599999992,
        /* x=0.90 */ 1.8767999999999914, /* x=0.91 */ 1.8762903999999914,
        /* x=0.92 */ 1.8745791999999906, /* x=0.93 */ 1.8716327999999898,
        /* x=0.94 */ 1.8674175999999907, /* x=0.95 */ 1.8618999999999888,
        /* x=0.96 */ 1.8550463999999902, /* x=0.97 */ 1.8468231999999887,
        /* x=0.98 */ 1.8371967999999885, /* x=0.99 */ 1.826133599999988,
        /* x=1.00 */ 1.8135999999999868, /* x=1.01 */ 1.7995623999999868,
        /* x=1.02 */ 1.783987199999987,  /* x=1.03 */ 1.7668407999999873,
        /* x=1.04 */ 1.748089599999986,  /* x=1.05 */ 1.7276999999999854,
        /* x=1.06 */ 1.7056383999999847, /* x=1.07 */ 1.6818711999999838,
        /* x=1.08 */ 1.6563647999999844, /* x=1.09 */ 1.629085599999983,
        /* x=1.10 */ 1.5999999999999837};

    double x = 0.6;
    for (const double v : kAnswer0)
    {
        EXPECT_NEAR(PolyInterpolator::eval(x, r0), v, 0.00001) << "r0 different at x=" << x;
        x += 0.01;
    }
}

// PolyInterpolatorTest.oldMainTest, second block: one second derivative constraint added
TEST(PolyInterpolator, OldMainTestWithOneSecondDerivative)
{
    const PolyInterpolator    p1{{0.6, 1.1}, {0.6, 1.1}, {0.6}};
    const std::vector<double> r1 = p1.interpolator({1.5, 1.6, 2, -3, 0});

    // OpenRocket's golden values; the one at x=0.66 happens to be within 1e-4 of the golden ratio.
    // NOLINTBEGIN(modernize-use-std-numbers)
    constexpr std::array<double, 51> kAnswer1 = {
        /* x=0.60 */ 1.4999999999999907, /* x=0.61 */ 1.5199912319999909,
        /* x=0.62 */ 1.5399301119999906, /* x=0.63 */ 1.5597649919999914,
        /* x=0.64 */ 1.5794449919999898, /* x=0.65 */ 1.5989199999999888,
        /* x=0.66 */ 1.6181406719999905, /* x=0.67 */ 1.6370584319999903,
        /* x=0.68 */ 1.655625471999989,  /* x=0.69 */ 1.6737947519999874,
        /* x=0.70 */ 1.691519999999989,  /* x=0.71 */ 1.7087557119999883,
        /* x=0.72 */ 1.7254571519999873, /* x=0.73 */ 1.741580351999988,
        /* x=0.74 */ 1.7570821119999884, /* x=0.75 */ 1.7719199999999875,
        /* x=0.76 */ 1.7860523519999871, /* x=0.77 */ 1.7994382719999862,
        /* x=0.78 */ 1.8120376319999876, /* x=0.79 */ 1.8238110719999838,
        /* x=0.80 */ 1.8347199999999855, /* x=0.81 */ 1.8447265919999851,
        /* x=0.82 */ 1.8537937919999865, /* x=0.83 */ 1.861885311999986,
        /* x=0.84 */ 1.8689656319999832, /* x=0.85 */ 1.8749999999999836,
        /* x=0.86 */ 1.8799544319999804, /* x=0.87 */ 1.8837957119999817,
        /* x=0.88 */ 1.8864913919999804, /* x=0.89 */ 1.8880097919999814,
        /* x=0.90 */ 1.8883199999999807, /* x=0.91 */ 1.8873918719999794,
        /* x=0.92 */ 1.88519603199998,   /* x=0.93 */ 1.8817038719999788,
        /* x=0.94 */ 1.8768875519999786, /* x=0.95 */ 1.8707199999999764,
        /* x=0.96 */ 1.8631749119999794, /* x=0.97 */ 1.8542267519999784,
        /* x=0.98 */ 1.8438507519999745, /* x=0.99 */ 1.832022911999974,
        /* x=1.00 */ 1.8187199999999768, /* x=1.01 */ 1.8039195519999738,
        /* x=1.02 */ 1.7875998719999755, /* x=1.03 */ 1.7697400319999765,
        /* x=1.04 */ 1.7503198719999729, /* x=1.05 */ 1.7293199999999675,
        /* x=1.06 */ 1.7067217919999687, /* x=1.07 */ 1.6825073919999713,
        /* x=1.08 */ 1.656659711999966,  /* x=1.09 */ 1.629162431999962,
        /* x=1.10 */ 1.5999999999999686};
    // NOLINTEND(modernize-use-std-numbers)

    double x = 0.6;
    for (const double v : kAnswer1)
    {
        EXPECT_NEAR(PolyInterpolator::eval(x, r1), v, 0.00001) << "r1 different at x=" << x;
        x += 0.01;
    }
}

// PolyInterpolatorTest.oldMainTest, third block: values, derivatives and second derivatives
TEST(PolyInterpolator, OldMainTestWithSecondDerivatives)
{
    const PolyInterpolator    p2{{0.6, 1.1}, {0.6, 1.1}, {0.6, 1.1}};
    const std::vector<double> r2 = p2.interpolator({1.5, 1.6, 2, -3, 0, 0});

    constexpr std::array<double, 51> kAnswer2 = {
        /* x=0.60 */ 1.5000000000000844, /* x=0.61 */ 1.520007366720093,
        /* x=0.62 */ 1.5400539750400783, /* x=0.63 */ 1.5601657929600794,
        /* x=0.64 */ 1.58035504128011,   /* x=0.65 */ 1.6006210000000962,
        /* x=0.66 */ 1.620950814720107,  /* x=0.67 */ 1.641320303040109,
        /* x=0.68 */ 1.6616947609601311, /* x=0.69 */ 1.682029769280133,
        /* x=0.70 */ 1.7022720000001055, /* x=0.71 */ 1.7223600227201015,
        /* x=0.72 */ 1.7422251110400993, /* x=0.73 */ 1.7617920489601087,
        /* x=0.74 */ 1.7809799372800814, /* x=0.75 */ 1.7997030000000898,
        /* x=0.76 */ 1.8178713907201,    /* x=0.77 */ 1.8353919990401195,
        /* x=0.78 */ 1.85216925696011,   /* x=0.79 */ 1.8681059452800994,
        /* x=0.80 */ 1.8831040000000847, /* x=0.81 */ 1.897065318720074,
        /* x=0.82 */ 1.9098925670401137, /* x=0.83 */ 1.921489984960104,
        /* x=0.84 */ 1.931764193280106,  /* x=0.85 */ 1.940625000000086,
        /* x=0.86 */ 1.9479862067200955, /* x=0.87 */ 1.9537664150401213,
        /* x=0.88 */ 1.9578898329600989, /* x=0.89 */ 1.9602870812801143,
        /* x=0.90 */ 1.960896000000119,  /* x=0.91 */ 1.9596624547200818,
        /* x=0.92 */ 1.9565411430400914, /* x=0.93 */ 1.9514964009601528,
        /* x=0.94 */ 1.9445030092801048, /* x=0.95 */ 1.9355470000001276,
        /* x=0.96 */ 1.92462646272012,   /* x=0.97 */ 1.9117523510400858,
        /* x=0.98 */ 1.8969492889601298, /* x=0.99 */ 1.8802563772801548,
        /* x=1.00 */ 1.86172800000017,   /* x=1.01 */ 1.8414346307201441,
        /* x=1.02 */ 1.8194636390400944, /* x=1.03 */ 1.7959200969601596,
        /* x=1.04 */ 1.7709275852801198, /* x=1.05 */ 1.7446290000001738,
        /* x=1.06 */ 1.7171873587201247, /* x=1.07 */ 1.6887866070401714,
        /* x=1.08 */ 1.659632424960222,  /* x=1.09 */ 1.6299530332800884,
        /* x=1.10 */ 1.6000000000001648};

    double x = 0.6;
    for (const double v : kAnswer2)
    {
        EXPECT_NEAR(PolyInterpolator::eval(x, r2), v, 0.00001) << "r2 different at x=" << x;
        x += 0.01;
    }
}

TEST(PolyInterpolator, EvalTakesTheHighestOrderCoefficientFirst)
{
    constexpr std::array<double, 3> kQuadratic = {1, 2, 3};  // x^2 + 2x + 3
    EXPECT_DOUBLE_EQ(PolyInterpolator::eval(2, kQuadratic), 11);
    EXPECT_DOUBLE_EQ(PolyInterpolator::eval(0, kQuadratic), 3);
    EXPECT_DOUBLE_EQ(PolyInterpolator::eval(-1, kQuadratic), 2);
    EXPECT_DOUBLE_EQ(PolyInterpolator::eval(5, std::span<const double>{}), 0);
}

TEST(PolyInterpolator, CountsTheConstraints)
{
    EXPECT_EQ((PolyInterpolator{{0.6, 1.1}, {0.6, 1.1}}.count()), 4U);
    EXPECT_EQ((PolyInterpolator{{0.6, 1.1}, {0.6, 1.1}, {0.6}}.count()), 5U);
    EXPECT_EQ((PolyInterpolator{{2.0}}.count()), 1U);
    // Value at 0, slope at 1 and curvature at 2: one constraint of each order.
    EXPECT_EQ((PolyInterpolator{{0.0}, {1.0}, {2.0}}.count()), 3U);
}

TEST(PolyInterpolator, MeetsOneConstraintOfEachOrder)
{
    // Value 1 at 0, slope 2 at 1, curvature 3 at 2: 1.5 x^2 - x + 1.
    const PolyInterpolator    p{{0.0}, {1.0}, {2.0}};
    const std::vector<double> poly = p.interpolator({1.0, 2.0, 3.0});
    ASSERT_EQ(poly.size(), 3U);
    EXPECT_NEAR(poly[0], 1.5, 1e-12);
    EXPECT_NEAR(poly[1], -1.0, 1e-12);
    EXPECT_NEAR(poly[2], 1.0, 1e-12);
}

TEST(PolyInterpolator, OneValueGivesAConstant)
{
    const PolyInterpolator    p{{2.0}};
    const std::vector<double> coefficients = p.interpolator({7.0});
    ASSERT_EQ(coefficients.size(), 1U);
    EXPECT_DOUBLE_EQ(coefficients[0], 7.0);
    EXPECT_DOUBLE_EQ(p.interpolate(-3.0, {7.0}), 7.0);
}

TEST(PolyInterpolator, TwoValuesGiveALine)
{
    const PolyInterpolator    p{{0.0, 1.0}};
    const std::vector<double> coefficients = p.interpolator({1.0, 3.0});  // y = 2x + 1
    ASSERT_EQ(coefficients.size(), 2U);
    EXPECT_NEAR(coefficients[0], 2.0, 1e-12);
    EXPECT_NEAR(coefficients[1], 1.0, 1e-12);
}

TEST(PolyInterpolator, MeetsItsValueAndDerivativeConstraints)
{
    // The interpolator OpenRocket's conical nose cone drag uses: value and slope at both ends.
    const PolyInterpolator    p{{1.0, 1.3}, {1.0, 1.3}};
    const std::vector<double> poly = p.interpolator({0.5, 0.25, -0.1, 0.0});
    ASSERT_EQ(poly.size(), 4U);
    EXPECT_NEAR(PolyInterpolator::eval(1.0, poly), 0.5, 1e-9);
    EXPECT_NEAR(PolyInterpolator::eval(1.3, poly), 0.25, 1e-9);
    const std::vector<double> slope = derivative(poly);
    EXPECT_NEAR(PolyInterpolator::eval(1.0, slope), -0.1, 1e-9);
    EXPECT_NEAR(PolyInterpolator::eval(1.3, slope), 0.0, 1e-9);
}

TEST(PolyInterpolator, MeetsItsSecondDerivativeConstraints)
{
    const PolyInterpolator    p{{0.6, 1.1}, {0.6, 1.1}, {0.6, 1.1}};
    const std::vector<double> poly      = p.interpolator({1.5, 1.6, 2, -3, 0.5, -0.25});
    const std::vector<double> slope     = derivative(poly);
    const std::vector<double> curvature = derivative(slope);
    EXPECT_NEAR(PolyInterpolator::eval(0.6, poly), 1.5, 1e-9);
    EXPECT_NEAR(PolyInterpolator::eval(1.1, poly), 1.6, 1e-9);
    EXPECT_NEAR(PolyInterpolator::eval(0.6, slope), 2, 1e-9);
    EXPECT_NEAR(PolyInterpolator::eval(1.1, slope), -3, 1e-9);
    EXPECT_NEAR(PolyInterpolator::eval(0.6, curvature), 0.5, 1e-8);
    EXPECT_NEAR(PolyInterpolator::eval(1.1, curvature), -0.25, 1e-8);
}

TEST(PolyInterpolator, InterpolateEvaluatesTheInterpolator)
{
    const PolyInterpolator          p{{0.6, 1.1}, {0.6, 1.1}};
    constexpr std::array<double, 4> kValues = {1.5, 1.6, 2, -3};
    const std::vector<double>       poly    = p.interpolator(kValues);
    for (const double x : {0.6, 0.85, 1.1, 2.0})
    {
        EXPECT_DOUBLE_EQ(p.interpolate(x, kValues), PolyInterpolator::eval(x, poly));
    }
}

TEST(PolyInterpolator, RejectsNoConstraints)
{
    EXPECT_THROW((PolyInterpolator{{}}), std::invalid_argument);
    EXPECT_THROW((PolyInterpolator{{}, {}}), std::invalid_argument);
    const std::vector<std::vector<double>> none;
    EXPECT_THROW(PolyInterpolator{none}, std::invalid_argument);
}

TEST(PolyInterpolator, RejectsTheWrongNumberOfValues)
{
    const PolyInterpolator p{{0.6, 1.1}, {0.6, 1.1}};
    EXPECT_THROW(static_cast<void>(p.interpolator({1.5, 1.6, 2})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(p.interpolator({1.5, 1.6, 2, -3, 0})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(p.interpolate(0.7, {1.5})), std::invalid_argument);
}

}  // namespace
