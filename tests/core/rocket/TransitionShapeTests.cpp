#include "QtRocket/rocket/TransitionShape.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace
{

using QtRocket::TransitionShape;

/// Transition.Shape.getRadius(x, radius, length, param) and its result, computed by OpenRocket
/// (JDK 17). For the shapes that need only +, -, *, / and sqrt (CONICAL, OGIVE, ELLIPSOID,
/// PARABOLIC, and POWER's small-parameter branch) the result must match bit for bit; the others go
/// through pow, acos and sin, which may differ in the last bit between math libraries.
struct RadiusCase
{
    TransitionShape shape;
    double          x;
    double          radius;
    double          length;
    double          param;
    double          expected;
    bool            exact;
};

/// One row of kRadiusCases, kept on one line.
[[nodiscard]] constexpr RadiusCase row(TransitionShape shape, double x, double radius,
                                       double length, double param, double expected,
                                       bool exact) noexcept
{
    return {.shape    = shape,
            .x        = x,
            .radius   = radius,
            .length   = length,
            .param    = param,
            .expected = expected,
            .exact    = exact};
}

// NOLINTBEGIN(modernize-use-std-numbers): OpenRocket's results, not approximations of constants
constexpr std::array<RadiusCase, 132> kRadiusCases{{
    row(TransitionShape::CONICAL, 0.0, 1.0, 2.0, 0.0, 0.0, true),
    row(TransitionShape::CONICAL, 1.0, 1.0, 2.0, 0.0, 0.5, true),
    row(TransitionShape::CONICAL, 2.0, 1.0, 2.0, 0.0, 1.0, true),
    row(TransitionShape::CONICAL, 0.3, 0.7, 1.3, 0.0, 0.16153846153846152, true),
    row(TransitionShape::CONICAL, 0.0, 1.0, 2.0, 1.0, 0.0, true),
    row(TransitionShape::CONICAL, 0.5, 1.0, 2.0, 1.0, 0.25, true),
    row(TransitionShape::CONICAL, 1.0, 1.0, 2.0, 1.0, 0.5, true),
    row(TransitionShape::CONICAL, 1.5, 1.0, 2.0, 1.0, 0.75, true),
    row(TransitionShape::CONICAL, 2.0, 1.0, 2.0, 1.0, 1.0, true),
    row(TransitionShape::CONICAL, 0.5, 1.0, 2.0, 0.5, 0.25, true),
    row(TransitionShape::CONICAL, 0.5, 1.0, 2.0, 0.0005, 0.25, true),
    row(TransitionShape::CONICAL, 0.25, 1.0, 0.5, 1.0, 0.5, true),
    row(TransitionShape::CONICAL, 0.5, 1.0, 0.5, 1.0, 1.0, true),
    row(TransitionShape::CONICAL, 0.1, 0.8, 0.3, 0.7, 0.2666666666666667, true),
    row(TransitionShape::CONICAL, 0.3, 0.7, 0.3, 1.0, 0.7, true),
    row(TransitionShape::CONICAL, 1.0, 1.0, 8.0, 1.0, 0.125, true),
    row(TransitionShape::CONICAL, 2.0, 1.0, 8.0, 1.0, 0.25, true),
    row(TransitionShape::CONICAL, 7.0, 1.0, 8.0, 1.0, 0.875, true),
    row(TransitionShape::CONICAL, 0.5, 1.0, 2.0, 0.3333333333333333, 0.25, true),
    row(TransitionShape::CONICAL, 1.2, 0.05, 1.3, 0.75, 0.04615384615384615, true),
    row(TransitionShape::CONICAL, 1e-05, 1.0, 2.0, 1e-06, 5e-06, true),
    row(TransitionShape::CONICAL, 0.0, 1.0, 2.0, 1e-06, 0.0, true),
    row(TransitionShape::OGIVE, 0.0, 1.0, 2.0, 0.0, 0.0, true),
    row(TransitionShape::OGIVE, 1.0, 1.0, 2.0, 0.0, 0.5, true),
    row(TransitionShape::OGIVE, 2.0, 1.0, 2.0, 0.0, 1.0, true),
    row(TransitionShape::OGIVE, 0.3, 0.7, 1.3, 0.0, 0.16153846153846152, true),
    row(TransitionShape::OGIVE, 0.0, 1.0, 2.0, 1.0, 0.0, true),
    row(TransitionShape::OGIVE, 0.5, 1.0, 2.0, 1.0, 0.5, true),
    row(TransitionShape::OGIVE, 1.0, 1.0, 2.0, 1.0, 0.7912878474779199, true),
    row(TransitionShape::OGIVE, 1.5, 1.0, 2.0, 1.0, 0.9494897427831779, true),
    row(TransitionShape::OGIVE, 2.0, 1.0, 2.0, 1.0, 1.0, true),
    row(TransitionShape::OGIVE, 0.5, 1.0, 2.0, 0.5, 0.33095189484529985, true),
    row(TransitionShape::OGIVE, 0.5, 1.0, 2.0, 0.0005, 0.25, true),
    row(TransitionShape::OGIVE, 0.25, 1.0, 0.5, 1.0, 0.8660254037844386, true),
    row(TransitionShape::OGIVE, 0.5, 1.0, 0.5, 1.0, 1.0, true),
    row(TransitionShape::OGIVE, 0.1, 0.8, 0.3, 0.7, 0.4670587285893064, true),
    row(TransitionShape::OGIVE, 0.3, 0.7, 0.3, 1.0, 0.7, true),
    row(TransitionShape::OGIVE, 1.0, 1.0, 8.0, 1.0, 0.23720214511669724, true),
    row(TransitionShape::OGIVE, 2.0, 1.0, 8.0, 1.0, 0.441352507368876, true),
    row(TransitionShape::OGIVE, 7.0, 1.0, 8.0, 1.0, 0.9846117415615723, true),
    row(TransitionShape::OGIVE, 0.5, 1.0, 2.0, 0.3333333333333333, 0.2979589711327115, true),
    row(TransitionShape::OGIVE, 1.2, 0.05, 1.3, 0.75, 0.04828549323913478, true),
    row(TransitionShape::OGIVE, 1e-05, 1.0, 2.0, 1e-06, 5e-06, true),
    row(TransitionShape::OGIVE, 0.0, 1.0, 2.0, 1e-06, 0.0, true),
    row(TransitionShape::ELLIPSOID, 0.0, 1.0, 2.0, 0.0, 0.0, true),
    row(TransitionShape::ELLIPSOID, 1.0, 1.0, 2.0, 0.0, 0.8660254037844386, true),
    row(TransitionShape::ELLIPSOID, 2.0, 1.0, 2.0, 0.0, 1.0, true),
    row(TransitionShape::ELLIPSOID, 0.3, 0.7, 1.3, 0.0, 0.4472797464648194, true),
    row(TransitionShape::ELLIPSOID, 0.0, 1.0, 2.0, 1.0, 0.0, true),
    row(TransitionShape::ELLIPSOID, 0.5, 1.0, 2.0, 1.0, 0.6614378277661477, true),
    row(TransitionShape::ELLIPSOID, 1.0, 1.0, 2.0, 1.0, 0.8660254037844386, true),
    row(TransitionShape::ELLIPSOID, 1.5, 1.0, 2.0, 1.0, 0.9682458365518543, true),
    row(TransitionShape::ELLIPSOID, 2.0, 1.0, 2.0, 1.0, 1.0, true),
    row(TransitionShape::ELLIPSOID, 0.5, 1.0, 2.0, 0.5, 0.6614378277661477, true),
    row(TransitionShape::ELLIPSOID, 0.5, 1.0, 2.0, 0.0005, 0.6614378277661477, true),
    row(TransitionShape::ELLIPSOID, 0.25, 1.0, 0.5, 1.0, 0.8660254037844386, true),
    row(TransitionShape::ELLIPSOID, 0.5, 1.0, 0.5, 1.0, 1.0, true),
    row(TransitionShape::ELLIPSOID, 0.1, 0.8, 0.3, 0.7, 0.5962847939999439, true),
    row(TransitionShape::ELLIPSOID, 0.3, 0.7, 0.3, 1.0, 0.7, true),
    row(TransitionShape::ELLIPSOID, 1.0, 1.0, 8.0, 1.0, 0.4841229182759271, true),
    row(TransitionShape::ELLIPSOID, 2.0, 1.0, 8.0, 1.0, 0.6614378277661477, true),
    row(TransitionShape::ELLIPSOID, 7.0, 1.0, 8.0, 1.0, 0.9921567416492215, true),
    row(TransitionShape::ELLIPSOID, 0.5, 1.0, 2.0, 0.3333333333333333, 0.6614378277661477, true),
    row(TransitionShape::ELLIPSOID, 1.2, 0.05, 1.3, 0.75, 0.04985185152621431, true),
    row(TransitionShape::ELLIPSOID, 1e-05, 1.0, 2.0, 1e-06, 0.003162273707318834, true),
    row(TransitionShape::ELLIPSOID, 0.0, 1.0, 2.0, 1e-06, 0.0, true),
    row(TransitionShape::POWER, 0.0, 1.0, 2.0, 0.0, 0.0, true),
    row(TransitionShape::POWER, 1.0, 1.0, 2.0, 0.0, 1.0, true),
    row(TransitionShape::POWER, 2.0, 1.0, 2.0, 0.0, 1.0, true),
    row(TransitionShape::POWER, 0.3, 0.7, 1.3, 0.0, 0.7, true),
    row(TransitionShape::POWER, 0.0, 1.0, 2.0, 1.0, 0.0, false),
    row(TransitionShape::POWER, 0.5, 1.0, 2.0, 1.0, 0.25, false),
    row(TransitionShape::POWER, 1.0, 1.0, 2.0, 1.0, 0.5, false),
    row(TransitionShape::POWER, 1.5, 1.0, 2.0, 1.0, 0.75, false),
    row(TransitionShape::POWER, 2.0, 1.0, 2.0, 1.0, 1.0, false),
    row(TransitionShape::POWER, 0.5, 1.0, 2.0, 0.5, 0.5, false),
    row(TransitionShape::POWER, 0.5, 1.0, 2.0, 0.0005, 0.9993070929904525, false),
    row(TransitionShape::POWER, 0.25, 1.0, 0.5, 1.0, 0.5, false),
    row(TransitionShape::POWER, 0.5, 1.0, 0.5, 1.0, 1.0, false),
    row(TransitionShape::POWER, 0.1, 0.8, 0.3, 0.7, 0.3707704454175759, false),
    row(TransitionShape::POWER, 0.3, 0.7, 0.3, 1.0, 0.7, false),
    row(TransitionShape::POWER, 1.0, 1.0, 8.0, 1.0, 0.125, false),
    row(TransitionShape::POWER, 2.0, 1.0, 8.0, 1.0, 0.25, false),
    row(TransitionShape::POWER, 7.0, 1.0, 8.0, 1.0, 0.875, false),
    row(TransitionShape::POWER, 0.5, 1.0, 2.0, 0.3333333333333333, 0.6299605249474366, false),
    row(TransitionShape::POWER, 1.2, 0.05, 1.3, 0.75, 0.0470867184319084, false),
    row(TransitionShape::POWER, 1e-05, 1.0, 2.0, 1e-06, 0.0, true),
    row(TransitionShape::POWER, 0.0, 1.0, 2.0, 1e-06, 0.0, true),
    row(TransitionShape::PARABOLIC, 0.0, 1.0, 2.0, 0.0, 0.0, true),
    row(TransitionShape::PARABOLIC, 1.0, 1.0, 2.0, 0.0, 0.5, true),
    row(TransitionShape::PARABOLIC, 2.0, 1.0, 2.0, 0.0, 1.0, true),
    row(TransitionShape::PARABOLIC, 0.3, 0.7, 1.3, 0.0, 0.16153846153846152, true),
    row(TransitionShape::PARABOLIC, 0.0, 1.0, 2.0, 1.0, 0.0, true),
    row(TransitionShape::PARABOLIC, 0.5, 1.0, 2.0, 1.0, 0.4375, true),
    row(TransitionShape::PARABOLIC, 1.0, 1.0, 2.0, 1.0, 0.75, true),
    row(TransitionShape::PARABOLIC, 1.5, 1.0, 2.0, 1.0, 0.9375, true),
    row(TransitionShape::PARABOLIC, 2.0, 1.0, 2.0, 1.0, 1.0, true),
    row(TransitionShape::PARABOLIC, 0.5, 1.0, 2.0, 0.5, 0.3125, true),
    row(TransitionShape::PARABOLIC, 0.5, 1.0, 2.0, 0.0005, 0.2500468867216804, true),
    row(TransitionShape::PARABOLIC, 0.25, 1.0, 0.5, 1.0, 0.75, true),
    row(TransitionShape::PARABOLIC, 0.5, 1.0, 0.5, 1.0, 1.0, true),
    row(TransitionShape::PARABOLIC, 0.1, 0.8, 0.3, 0.7, 0.36239316239316244, true),
    row(TransitionShape::PARABOLIC, 0.3, 0.7, 0.3, 1.0, 0.7, true),
    row(TransitionShape::PARABOLIC, 1.0, 1.0, 8.0, 1.0, 0.234375, true),
    row(TransitionShape::PARABOLIC, 2.0, 1.0, 8.0, 1.0, 0.4375, true),
    row(TransitionShape::PARABOLIC, 7.0, 1.0, 8.0, 1.0, 0.984375, true),
    row(TransitionShape::PARABOLIC, 0.5, 1.0, 2.0, 0.3333333333333333, 0.2875, true),
    row(TransitionShape::PARABOLIC, 1.2, 0.05, 1.3, 0.75, 0.048284023668639056, true),
    row(TransitionShape::PARABOLIC, 1e-05, 1.0, 2.0, 1e-06, 5.00000249998875e-06, true),
    row(TransitionShape::PARABOLIC, 0.0, 1.0, 2.0, 1e-06, 0.0, true),
    row(TransitionShape::HAACK, 0.0, 1.0, 2.0, 0.0, 0.0, false),
    row(TransitionShape::HAACK, 1.0, 1.0, 2.0, 0.0, 0.7071067811865476, false),
    row(TransitionShape::HAACK, 2.0, 1.0, 2.0, 0.0, 1.0, false),
    row(TransitionShape::HAACK, 0.3, 0.7, 1.3, 0.0, 0.2924772993531959, false),
    row(TransitionShape::HAACK, 0.0, 1.0, 2.0, 0.3333333333333333, 0.0, false),
    row(TransitionShape::HAACK, 0.5, 1.0, 2.0, 0.3333333333333333, 0.5142151508907623, false),
    row(TransitionShape::HAACK, 1.0, 1.0, 2.0, 0.3333333333333333, 0.7785263613999187, false),
    row(TransitionShape::HAACK, 1.5, 1.0, 2.0, 0.3333333333333333, 0.9345667458506315, false),
    row(TransitionShape::HAACK, 2.0, 1.0, 2.0, 0.3333333333333333, 1.0, false),
    row(TransitionShape::HAACK, 0.5, 1.0, 2.0, 0.16666666666666666, 0.47954057747154977, false),
    row(TransitionShape::HAACK, 0.5, 1.0, 2.0, 0.0005, 0.4422719566576396, false),
    row(TransitionShape::HAACK, 0.25, 1.0, 0.5, 0.3333333333333333, 0.7785263613999187, false),
    row(TransitionShape::HAACK, 0.5, 1.0, 0.5, 0.3333333333333333, 1.0, false),
    row(TransitionShape::HAACK, 0.1, 0.8, 0.3, 0.2333333333333333, 0.47600705660954673, false),
    row(TransitionShape::HAACK, 0.3, 0.7, 0.3, 0.3333333333333333, 0.7, false),
    row(TransitionShape::HAACK, 1.0, 1.0, 8.0, 0.3333333333333333, 0.32070372182587825, false),
    row(TransitionShape::HAACK, 2.0, 1.0, 8.0, 0.3333333333333333, 0.5142151508907623, false),
    row(TransitionShape::HAACK, 7.0, 1.0, 8.0, 0.3333333333333333, 0.9790593773508238, false),
    row(TransitionShape::HAACK, 0.5, 1.0, 2.0, 0.3333333333333333, 0.5142151508907623, false),
    row(TransitionShape::HAACK, 1.2, 0.05, 1.3, 0.25, 0.049413416586772396, false),
    row(TransitionShape::HAACK, 1e-05, 1.0, 2.0, 1e-06, 0.0001377691347533374, false),
    row(TransitionShape::HAACK, 0.0, 1.0, 2.0, 1e-06, 0.0, false),
}};
// NOLINTEND(modernize-use-std-numbers)

/// Transition.getRadius(x) of a transition with fore radius, aft radius,
/// length, the shape's default parameter and the clipped flag, at x = -1, 0,
/// 0.1, 0.25, 0.5, 0.9 and 1 times the length, and the clip length it cached
/// (-1 when it never solved one), computed by OpenRocket.
struct TransitionCase
{
    TransitionShape       shape;
    double                foreRadius;
    double                aftRadius;
    double                length;
    double                param;
    bool                  clipped;
    double                clipLength;
    std::array<double, 7> radii;
};

// NOLINTBEGIN(modernize-use-std-numbers): OpenRocket's results, not approximations of constants
constexpr std::array<TransitionCase, 12> kTransitionCases{{
    {.shape      = TransitionShape::ELLIPSOID,
     .foreRadius = 0.5,
     .aftRadius  = 1.0,
     .length     = 5.0,
     .param      = 0.0,
     .clipped    = true,
     .clipLength = 0.7735061645507812,
     .radii = {0.5, 0.5000009022204828, 0.6264987875481088, 0.7603456500168061, 0.9013879439815712,
               0.9962429467866826, 1.0}},
    {.shape      = TransitionShape::ELLIPSOID,
     .foreRadius = 0.5,
     .aftRadius  = 1.0,
     .length     = 5.0,
     .param      = 0.0,
     .clipped    = false,
     .clipLength = -1.0,
     .radii      = {0.5, 0.5, 0.7179449471770336, 0.8307189138830738, 0.9330127018922193,
                    0.9974937185533099, 1.0}},
    {.shape      = TransitionShape::ELLIPSOID,
     .foreRadius = 1.0,
     .aftRadius  = 0.5,
     .length     = 5.0,
     .param      = 0.0,
     .clipped    = true,
     .clipLength = 0.7735061645507812,
     .radii      = {1.0, 1.0, 0.9962429467866826, 0.9762812383677313, 0.9013879439815712,
                    0.6264987875481088, 0.5}},
    {.shape      = TransitionShape::POWER,
     .foreRadius = 0.2,
     .aftRadius  = 0.6,
     .length     = 1.5,
     .param      = 0.5,
     .clipped    = true,
     .clipLength = 0.1875457763671875,
     .radii      = {0.2, 0.2000216996230329, 0.2683427142444492, 0.34641955810846636,
                    0.44721844792007787, 0.5727136003553684, 0.6}},
    {.shape      = TransitionShape::POWER,
     .foreRadius = 0.2,
     .aftRadius  = 0.6,
     .length     = 1.5,
     .param      = 0.5,
     .clipped    = false,
     .clipLength = -1.0,
     .radii      = {0.2, 0.2, 0.3264911064067352, 0.4, 0.482842712474619, 0.5794733192202055, 0.6}},
    {.shape      = TransitionShape::HAACK,
     .foreRadius = 0.3,
     .aftRadius  = 0.9,
     .length     = 2.0,
     .param      = 0.0,
     .clipped    = true,
     .clipLength = 0.404754638671875,
     .radii = {0.3, 0.30000896308376995, 0.3996018632494796, 0.5278964185873513, 0.7009862599868389,
               0.8819602433481255, 0.9}},
    {.shape      = TransitionShape::HAACK,
     .foreRadius = 0.3,
     .aftRadius  = 0.9,
     .length     = 2.0,
     .param      = 0.0,
     .clipped    = false,
     .clipLength = -1.0,
     .radii      = {0.3, 0.3, 0.436878950022014, 0.5652930444094582, 0.7242640687119286,
                    0.884178186036479, 0.9}},
    {.shape      = TransitionShape::OGIVE,
     .foreRadius = 0.44135,
     .aftRadius  = 1.0,
     .length     = 6.0,
     .param      = 1.0,
     .clipped    = true,
     .clipLength = -1.0,
     .radii      = {0.44135, 0.44135, 0.5482428608977515, 0.6869524845432907, 0.8612416358652909,
                    0.9944610420277088, 1.0}},
    {.shape      = TransitionShape::CONICAL,
     .foreRadius = 0.5,
     .aftRadius  = 1.0,
     .length     = 5.0,
     .param      = 0.0,
     .clipped    = true,
     .clipLength = -1.0,
     .radii      = {0.5, 0.5, 0.55, 0.625, 0.75, 0.95, 1.0}},
    {.shape      = TransitionShape::PARABOLIC,
     .foreRadius = 0.25,
     .aftRadius  = 1.0,
     .length     = 3.0,
     .param      = 1.0,
     .clipped    = true,
     .clipLength = -1.0,
     .radii      = {0.25, 0.25, 0.3925, 0.578125, 0.8125, 0.9924999999999999, 1.0}},
    {.shape      = TransitionShape::ELLIPSOID,
     .foreRadius = 0.0,
     .aftRadius  = 1.0,
     .length     = 3.0,
     .param      = 0.0,
     .clipped    = true,
     .clipLength = 0.0,
     .radii      = {0.0, 0.0, 0.4358898943540674, 0.6614378277661477, 0.8660254037844386,
                    0.99498743710662, 1.0}},
    {.shape      = TransitionShape::ELLIPSOID,
     .foreRadius = 0.01,
     .aftRadius  = 1.0,
     .length     = 0.1,
     .param      = 0.0,
     .clipped    = true,
     .clipLength = 4.8828125e-05,
     .radii      = {0.01, 0.031238561260722813, 0.4367956463208034, 0.6618526379313393,
                    0.8661662438166301, 0.9949923409137863, 1.0}},
}};
// NOLINTEND(modernize-use-std-numbers)

/// The x values of TransitionCase::radii for a transition of length @p length,
/// computed as the harness computed them.
[[nodiscard]] std::array<double, 7> sampleXs(double length)
{
    return {-1, 0, length * 0.1, length * 0.25, length / 2, length * 0.9, length};
}

/// Within a relative 1e-12 (or 1e-15 absolutely, for values near zero): the
/// tolerance for results that went through pow, acos or sin.
void expectClose(double actual, double expected)
{
    EXPECT_NEAR(actual, expected, (1e-12 * std::abs(expected)) + 1e-15);
}

/// EXPECT_EQ when @p exact, expectClose() otherwise.
void expectMatch(double actual, double expected, bool exact)
{
    if (exact)
    {
        EXPECT_EQ(actual, expected);
    }
    else
    {
        expectClose(actual, expected);
    }
}

/// The EPSILON of OpenRocket's TransitionTest: MathUtil.EPSILON * 1000.
constexpr double kTransitionTestEpsilon = 1e-5;

/// A transition's radius at @p x and the radius expected there.
struct RadiusAt
{
    double x;
    double expected;
};

/// getTransitionRadius() at each point of @p points, within
/// kTransitionTestEpsilon.
void expectProfile(TransitionShape shape, double foreRadius, double aftRadius, double length,
                   bool clipped, std::initializer_list<RadiusAt> points)
{
    for (const RadiusAt& point : points)
    {
        EXPECT_NEAR(point.expected,
                    getTransitionRadius(shape, point.x, foreRadius, aftRadius, length,
                                        defaultParameter(shape), clipped),
                    kTransitionTestEpsilon)
            << "x=" << point.x;
    }
}

TEST(TransitionShape, DeclarationOrderIsJavas)
{
    // The ordinal is written into the preset digest.
    constexpr std::array<TransitionShape, 6> kJavaOrder{
        TransitionShape::CONICAL, TransitionShape::OGIVE,     TransitionShape::ELLIPSOID,
        TransitionShape::POWER,   TransitionShape::PARABOLIC, TransitionShape::HAACK};
    for (std::size_t i = 0; i < kJavaOrder.size(); i++)
    {
        EXPECT_EQ(static_cast<std::size_t>(kJavaOrder.at(i)), i);
    }
    EXPECT_EQ(QtRocket::kAllTransitionShapes, kJavaOrder);
}

/// What OpenRocket gives for usesParameter, maxParameter, defaultParameter and
/// isClippable.
struct ParameterExpectation
{
    TransitionShape shape;
    bool            usesParameter;
    double          max;
    double          defaultParameter;
    bool            clippable;
};

void expectParameters(const ParameterExpectation& e)
{
    SCOPED_TRACE(std::string(transitionShapeName(e.shape)));
    EXPECT_EQ(usesParameter(e.shape), e.usesParameter);
    EXPECT_EQ(minParameter(e.shape), 0.0);
    EXPECT_EQ(maxParameter(e.shape), e.max);
    EXPECT_EQ(defaultParameter(e.shape), e.defaultParameter);
    EXPECT_EQ(isClippable(e.shape), e.clippable);
}

TEST(TransitionShape, ParameterRangesAndClipping)
{
    constexpr std::array<ParameterExpectation, 6> kExpected{{
        {.shape            = TransitionShape::CONICAL,
         .usesParameter    = false,
         .max              = 1.0,
         .defaultParameter = 0.0,
         .clippable        = false},
        {.shape            = TransitionShape::OGIVE,
         .usesParameter    = true,
         .max              = 1.0,
         .defaultParameter = 1.0,
         .clippable        = false},
        {.shape            = TransitionShape::ELLIPSOID,
         .usesParameter    = false,
         .max              = 1.0,
         .defaultParameter = 0.0,
         .clippable        = true},
        {.shape            = TransitionShape::POWER,
         .usesParameter    = true,
         .max              = 1.0,
         .defaultParameter = 0.5,
         .clippable        = true},
        {.shape            = TransitionShape::PARABOLIC,
         .usesParameter    = true,
         .max              = 1.0,
         .defaultParameter = 1.0,
         .clippable        = false},
        {.shape            = TransitionShape::HAACK,
         .usesParameter    = true,
         .max              = 0.3333333333333333,
         .defaultParameter = 0.0,
         .clippable        = true},
    }};
    for (const ParameterExpectation& e : kExpected)
    {
        expectParameters(e);
    }
    static_assert(maxParameter(TransitionShape::HAACK) == 1.0 / 3.0);
    EXPECT_EQ(QtRocket::kTransitionMinFeature, 0.001);
    EXPECT_EQ(QtRocket::kTransitionClipPrecision, 0.0001);
}

TEST(TransitionShape, RadiusMatchesOpenRocket)
{
    for (const RadiusCase& c : kRadiusCases)
    {
        SCOPED_TRACE(std::string(transitionShapeName(c.shape)) + " x=" + std::to_string(c.x) +
                     " r=" + std::to_string(c.radius) + " l=" + std::to_string(c.length) +
                     " p=" + std::to_string(c.param));
        expectMatch(getRadius(c.shape, c.x, c.radius, c.length, c.param), c.expected, c.exact);
    }
}

/// The profile of @p shape rises monotonically from 0 at the tip to the radius
/// at the aft end.
void expectZeroToRadius(TransitionShape shape)
{
    SCOPED_TRACE(std::string(transitionShapeName(shape)));
    const double param = defaultParameter(shape);
    EXPECT_NEAR(getRadius(shape, 0.0, 0.05, 0.3, param), 0.0, 1e-15);
    EXPECT_NEAR(getRadius(shape, 0.3, 0.05, 0.3, param), 0.05, 1e-15);
    double previous = 0.0;
    for (int i = 1; i <= 30; i++)
    {
        const double r = getRadius(shape, 0.3 * i / 30, 0.05, 0.3, param);
        EXPECT_GE(r, previous - 1e-15) << i;
        previous = r;
    }
}

TEST(TransitionShape, EveryShapeRunsFromZeroToTheRadius)
{
    for (const TransitionShape shape : QtRocket::kAllTransitionShapes)
    {
        expectZeroToRadius(shape);
    }
}

TEST(TransitionShape, ConicalOgiveAndParabolicSpecifics)
{
    // CONICAL is linear; PARABOLIC with parameter 0 is the cone too.
    EXPECT_DOUBLE_EQ(getRadius(TransitionShape::CONICAL, 0.15, 0.05, 0.3, 0.0), 0.025);
    EXPECT_DOUBLE_EQ(getRadius(TransitionShape::PARABOLIC, 0.15, 0.05, 0.3, 0.0), 0.025);
    // An ogive parameter below 1 mm gives the cone.
    EXPECT_EQ(getRadius(TransitionShape::OGIVE, 0.15, 0.05, 0.3, 0.0009999),
              getRadius(TransitionShape::CONICAL, 0.15, 0.05, 0.3, 0.0));
    // An ogive shorter than its radius is scaled to the radius: x = 0.1 of length
    // 0.1 and radius 0.2 is x = 0.2 of length 0.2, the aft end.
    EXPECT_DOUBLE_EQ(getRadius(TransitionShape::OGIVE, 0.1, 0.2, 0.1, 1.0), 0.2);
    EXPECT_DOUBLE_EQ(getRadius(TransitionShape::OGIVE, 0.05, 0.2, 0.1, 1.0),
                     getRadius(TransitionShape::OGIVE, 0.1, 0.2, 0.2, 1.0));
    // ELLIPSOID is the quarter ellipse: at x = length/2, sqrt(3)/2 of the radius.
    EXPECT_EQ(getRadius(TransitionShape::ELLIPSOID, 1.0, 1.0, 2.0, 0.0), std::sqrt(0.75));
}

TEST(TransitionShape, PowerAndHaackSpecifics)
{
    // POWER with a parameter up to 1e-5 is a step at x = 1e-5.
    EXPECT_EQ(getRadius(TransitionShape::POWER, 0.00001, 0.05, 0.3, 0.00001), 0.0);
    EXPECT_EQ(getRadius(TransitionShape::POWER, 0.0000101, 0.05, 0.3, 0.00001), 0.05);
    // POWER with parameter 1 is the cone.
    expectClose(getRadius(TransitionShape::POWER, 0.1, 0.05, 0.3, 1.0), 0.05 / 3);
    // HAACK at mid-length: LD-Haack gives radius / sqrt(2), the parameter adds
    // (1/3)/pi inside.
    expectClose(getRadius(TransitionShape::HAACK, 0.15, 0.05, 0.3, 0.0),
                0.05 / std::numbers::sqrt2);
    expectClose(getRadius(TransitionShape::HAACK, 0.15, 0.05, 0.3, 1.0 / 3.0),
                0.05 * std::sqrt(0.5 + (1.0 / 3.0 / std::numbers::pi)));
    // A parameter within MathUtil.EPSILON of zero counts as zero for HAACK.
    EXPECT_EQ(getRadius(TransitionShape::HAACK, 0.1, 0.05, 0.3, 1e-9),
              getRadius(TransitionShape::HAACK, 0.1, 0.05, 0.3, 0.0));
}

TEST(TransitionShape, NegativeRootsAreClampedToZero)
{
    // safeSqrt turns rounding errors below zero into 0, never NaN.
    EXPECT_EQ(getRadius(TransitionShape::ELLIPSOID, 0.0, 1.0, 2.0, 0.0), 0.0);
    EXPECT_FALSE(std::isnan(getRadius(TransitionShape::OGIVE, 0.0, 0.3, 0.7, 1.0)));
    EXPECT_FALSE(std::isnan(getRadius(TransitionShape::HAACK, 0.0, 0.3, 0.7, 0.0)));
}

/// getTransitionRadius() and calculateClipLength() against one OpenRocket
/// transition.
void expectTransitionCase(const TransitionCase& c)
{
    SCOPED_TRACE(std::string(transitionShapeName(c.shape)) + " " + std::to_string(c.foreRadius) +
                 " -> " + std::to_string(c.aftRadius) + (c.clipped ? " clipped" : " unclipped"));
    EXPECT_EQ(defaultParameter(c.shape), c.param);
    const bool exact = c.shape != TransitionShape::POWER && c.shape != TransitionShape::HAACK;
    const std::array<double, 7> xs = sampleXs(c.length);
    for (std::size_t i = 0; i < xs.size(); i++)
    {
        SCOPED_TRACE("x=" + std::to_string(xs.at(i)));
        expectMatch(getTransitionRadius(c.shape, xs.at(i), c.foreRadius, c.aftRadius, c.length,
                                        c.param, c.clipped),
                    c.radii.at(i), exact);
    }
    if (c.clipLength >= 0)
    {
        expectMatch(calculateClipLength(c.shape, c.foreRadius, c.aftRadius, c.length, c.param),
                    c.clipLength, exact);
    }
}

TEST(TransitionShape, TransitionRadiusMatchesOpenRocket)
{
    for (const TransitionCase& c : kTransitionCases)
    {
        expectTransitionCase(c);
    }
}

TEST(TransitionShape, ClipLengthIsZeroWithoutClipping)
{
    // No clipping from a point, nor for a transition without length.
    EXPECT_EQ(calculateClipLength(TransitionShape::ELLIPSOID, 0.0, 1.0, 3.0, 0.0), 0.0);
    EXPECT_EQ(calculateClipLength(TransitionShape::ELLIPSOID, 1.0, 0.0, 3.0, 0.0), 0.0);
    EXPECT_EQ(calculateClipLength(TransitionShape::ELLIPSOID, 0.5, 1.0, 0.0, 0.0), 0.0);
    EXPECT_EQ(calculateClipLength(TransitionShape::ELLIPSOID, 0.5, 1.0, -1.0, 0.0), 0.0);
}

TEST(TransitionShape, ClipLengthSolvesTheForeRadius)
{
    // The radii may come in either order.
    EXPECT_EQ(calculateClipLength(TransitionShape::HAACK, 0.3, 0.9, 2.0, 0.0),
              calculateClipLength(TransitionShape::HAACK, 0.9, 0.3, 2.0, 0.0));
    // The clipped profile passes through the fore radius within the precision.
    const double clip = calculateClipLength(TransitionShape::POWER, 0.2, 0.6, 1.5, 0.5);
    EXPECT_NEAR(getRadius(TransitionShape::POWER, clip, 0.6, clip + 1.5, 0.5), 0.2, 1e-4);
    // Deviation: a NaN or infinite length ends the bisection (OpenRocket loops
    // forever).
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    EXPECT_TRUE(std::isnan(calculateClipLength(TransitionShape::ELLIPSOID, 0.5, 1.0, nan, 0.0)));
    EXPECT_FALSE(
        std::isfinite(calculateClipLength(TransitionShape::ELLIPSOID, 0.5, 1.0, inf, 0.0)));
}

TEST(TransitionShape, TransitionRadiusOutsideTheTransition)
{
    // Ahead of the fore end the fore radius, from the aft end on the aft radius.
    EXPECT_EQ(getTransitionRadius(TransitionShape::OGIVE, -0.001, 0.2, 0.7, 1.0, 1.0, false), 0.2);
    EXPECT_EQ(getTransitionRadius(TransitionShape::OGIVE, 1.0, 0.2, 0.7, 1.0, 1.0, false), 0.7);
    EXPECT_EQ(getTransitionRadius(TransitionShape::OGIVE, 5.0, 0.2, 0.7, 1.0, 1.0, false), 0.7);
    // A clipped nose cone (fore radius 0) is the plain shape.
    EXPECT_EQ(getTransitionRadius(TransitionShape::ELLIPSOID, 0.4, 0.0, 0.6, 1.0, 0.0, true),
              getRadius(TransitionShape::ELLIPSOID, 0.4, 0.6, 1.0, 0.0));
}

TEST(TransitionShape, EqualRadiiGiveACylinder)
{
    for (const TransitionShape shape : QtRocket::kAllTransitionShapes)
    {
        EXPECT_EQ(getTransitionRadius(shape, 0.4, 0.3, 0.3, 1.0, defaultParameter(shape), true),
                  0.3)
            << transitionShapeName(shape);
    }
}

TEST(TransitionShape, ClippedFlagNeedsAClippableShape)
{
    for (const TransitionShape shape :
         {TransitionShape::CONICAL, TransitionShape::OGIVE, TransitionShape::PARABOLIC})
    {
        EXPECT_EQ(getTransitionRadius(shape, 0.4, 0.3, 0.6, 1.0, defaultParameter(shape), true),
                  getTransitionRadius(shape, 0.4, 0.3, 0.6, 1.0, defaultParameter(shape), false))
            << transitionShapeName(shape);
    }
}

TEST(TransitionShape, BoattailMirrorsTheForwardTransition)
{
    for (const double x : {0.1, 0.25, 0.5, 0.9})
    {
        EXPECT_EQ(getTransitionRadius(TransitionShape::HAACK, x, 0.9, 0.3, 2.0, 0.0, true),
                  getTransitionRadius(TransitionShape::HAACK, 2.0 - x, 0.3, 0.9, 2.0, 0.0, true))
            << x;
    }
}

// ---- TransitionTest.java: the shape radii of its component tests (the
// component state they also check is ported with Transition and NoseCone). A
// new Transition is clipped and setShapeType() applies the shape's default
// parameter and clipping.

TEST(TransitionShape, VerifyConicNose)
{
    // NoseCone(CONICAL, 0.06, 0.01): fore radius 0.
    expectProfile(TransitionShape::CONICAL, 0.0, 0.01, 0.06, true,
                  {{.x = 0.00, .expected = 0.0},
                   {.x = 0.015, .expected = 0.0025},
                   {.x = 0.03, .expected = 0.005},
                   {.x = 0.045, .expected = 0.0075},
                   {.x = 0.06, .expected = 0.01}});
}

TEST(TransitionShape, VerifyForwardConicTransition)
{
    expectProfile(TransitionShape::CONICAL, 0.5, 1.0, 5.0, true,
                  {{.x = 0.0, .expected = 0.5},
                   {.x = 1.0, .expected = 0.6},
                   {.x = 2.0, .expected = 0.7},
                   {.x = 3.0, .expected = 0.8},
                   {.x = 4.0, .expected = 0.9},
                   {.x = 5.0, .expected = 1.0}});
}

TEST(TransitionShape, VerifyBackwardConicTransition)
{
    expectProfile(TransitionShape::CONICAL, 1.0, 0.5, 5.0, true,
                  {{.x = 0.0, .expected = 1.0},
                   {.x = 1.0, .expected = 0.9},
                   {.x = 2.0, .expected = 0.8},
                   {.x = 3.0, .expected = 0.7},
                   {.x = 4.0, .expected = 0.6},
                   {.x = 5.0, .expected = 0.5}});
}

TEST(TransitionShape, VerifyOgiveNoseCone)
{
    EXPECT_EQ(defaultParameter(TransitionShape::OGIVE), 1.0);
    expectProfile(TransitionShape::OGIVE, 0.0, 1.0, 8.0, isClippable(TransitionShape::OGIVE),
                  {{.x = 0.0, .expected = 0.0},
                   {.x = 1.0, .expected = 0.23720214511},
                   {.x = 2.0, .expected = 0.44135250736},
                   {.x = 3.0, .expected = 0.61308144666},
                   {.x = 4.0, .expected = 0.75290684574},
                   {.x = 5.0, .expected = 0.86124225056},
                   {.x = 6.0, .expected = 0.93840316661},
                   {.x = 7.0, .expected = 0.98461174156},
                   {.x = 8.0, .expected = 1.0}});
}

TEST(TransitionShape, VerifyForwardOgiveTransition)
{
    expectProfile(TransitionShape::OGIVE, 0.44135, 1.0, 6.0, false,
                  {{.x = 0.0, .expected = 0.44135250736},
                   {.x = 1.0, .expected = 0.61308144666},
                   {.x = 2.0, .expected = 0.75290684574},
                   {.x = 3.0, .expected = 0.86124225056},
                   {.x = 4.0, .expected = 0.93840316661},
                   {.x = 5.0, .expected = 0.98461174156},
                   {.x = 6.0, .expected = 1.0}});
}

TEST(TransitionShape, VerifyBackwardOgiveTransition)
{
    expectProfile(TransitionShape::OGIVE, 1.0, 0.44135, 6.0, false,
                  {{.x = 0.0, .expected = 1.0},
                   {.x = 1.0, .expected = 0.98461174156},
                   {.x = 2.0, .expected = 0.93840316661},
                   {.x = 3.0, .expected = 0.86124225056},
                   {.x = 4.0, .expected = 0.75290684574},
                   {.x = 5.0, .expected = 0.61308144666},
                   {.x = 6.0, .expected = 0.44135250736}});
}

// ---- names

/// The names of a shape: Java's name(), the .ork spelling, the translation key
/// and the English name.
struct NameExpectation
{
    TransitionShape  shape;
    std::string_view name;
    std::string_view ork;
    std::string_view key;
    std::string_view display;
};

constexpr std::array<NameExpectation, 6> kNames{{
    {.shape   = TransitionShape::CONICAL,
     .name    = "CONICAL",
     .ork     = "conical",
     .key     = "Shape.Conical",
     .display = "Conical"},
    {.shape   = TransitionShape::OGIVE,
     .name    = "OGIVE",
     .ork     = "ogive",
     .key     = "Shape.Ogive",
     .display = "Ogive"},
    {.shape   = TransitionShape::ELLIPSOID,
     .name    = "ELLIPSOID",
     .ork     = "ellipsoid",
     .key     = "Shape.Ellipsoid",
     .display = "Ellipsoid"},
    {.shape   = TransitionShape::POWER,
     .name    = "POWER",
     .ork     = "power",
     .key     = "Shape.Powerseries",
     .display = "Power series"},
    {.shape   = TransitionShape::PARABOLIC,
     .name    = "PARABOLIC",
     .ork     = "parabolic",
     .key     = "Shape.Parabolicseries",
     .display = "Parabolic series"},
    {.shape   = TransitionShape::HAACK,
     .name    = "HAACK",
     .ork     = "haack",
     .key     = "Shape.Haackseries",
     .display = "Haack series"},
}};

void expectNames(const NameExpectation& e)
{
    SCOPED_TRACE(std::string(e.name));
    EXPECT_EQ(transitionShapeName(e.shape), e.name);
    EXPECT_EQ(orkName(e.shape), e.ork);
    EXPECT_EQ(displayKey(e.shape), e.key);
    EXPECT_EQ(displayName(e.shape), e.display);
    EXPECT_EQ(noseConeDescriptionKey(e.shape), std::string(e.key) + ".desc1");
    EXPECT_EQ(transitionDescriptionKey(e.shape), std::string(e.key) + ".desc2");
}

void expectLookups(const NameExpectation& e)
{
    SCOPED_TRACE(std::string(e.name));
    EXPECT_EQ(QtRocket::transitionShapeFromName(e.name), e.shape);
    EXPECT_EQ(QtRocket::transitionShapeFromOrkName(e.ork), e.shape);
    EXPECT_EQ(QtRocket::transitionShapeFromDisplayName(e.display), e.shape);
}

TEST(TransitionShape, Names)
{
    for (const NameExpectation& e : kNames)
    {
        expectNames(e);
        expectLookups(e);
    }
}

TEST(TransitionShape, ExactLookupsFailCleanly)
{
    // valueOf and toShape are exact.
    EXPECT_EQ(QtRocket::transitionShapeFromName("conical"), std::nullopt);
    EXPECT_EQ(QtRocket::transitionShapeFromName(""), std::nullopt);
    EXPECT_EQ(QtRocket::transitionShapeFromDisplayName("conical"), std::nullopt);
    EXPECT_EQ(QtRocket::transitionShapeFromDisplayName("Power"), std::nullopt);
}

TEST(TransitionShape, OrkLookupTrimsButKeepsCase)
{
    // findEnum trims, but does not fold case.
    EXPECT_EQ(QtRocket::transitionShapeFromOrkName("  haack\n"), TransitionShape::HAACK);
    EXPECT_EQ(QtRocket::transitionShapeFromOrkName("HAACK"), std::nullopt);
    EXPECT_EQ(QtRocket::transitionShapeFromOrkName("vonkarman"), std::nullopt);
    EXPECT_EQ(QtRocket::transitionShapeFromOrkName(""), std::nullopt);
}

TEST(TransitionShape, Descriptions)
{
    EXPECT_EQ(noseConeDescription(TransitionShape::CONICAL),
              "A conical nose cone has a profile of a triangle.");
    EXPECT_EQ(transitionDescription(TransitionShape::CONICAL),
              "A conical transition has straight sides.");
    // messages.properties carries a tab in the ogive transition description.
    EXPECT_TRUE(transitionDescription(TransitionShape::OGIVE).contains("circle.  \tThe shape"));
    // ½ and ¾, as UTF-8.
    EXPECT_TRUE(noseConeDescription(TransitionShape::POWER).contains("<b>\xC2\xBD-power</b>"));
    EXPECT_TRUE(transitionDescription(TransitionShape::POWER).contains("<b>\xC2\xBE-power</b>"));
    EXPECT_TRUE(transitionDescription(TransitionShape::HAACK)
                    .ends_with("while a value of 0.333 produces an <b>LV-Haack</b> shape."));
}

TEST(TransitionShape, EveryShapeHasTwoDescriptions)
{
    for (const TransitionShape shape : QtRocket::kAllTransitionShapes)
    {
        EXPECT_FALSE(noseConeDescription(shape).empty());
        EXPECT_FALSE(transitionDescription(shape).empty());
        EXPECT_NE(noseConeDescription(shape), transitionDescription(shape));
    }
}

}  // namespace
