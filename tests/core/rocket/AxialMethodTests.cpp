#include "QtRocket/rocket/position/AxialMethod.h"

#include <algorithm>
#include <optional>

#include <gtest/gtest.h>

namespace
{

using QtRocket::AxialMethod;

// Parent of length 0.2 and component of length 0.05, the Estes Alpha III body and fins of
// RocketTest.testChangeAxialMethod.
constexpr double kInner = 0.05;
constexpr double kOuter = 0.2;

TEST(AxialMethod, PositionFromOffset)
{
    EXPECT_DOUBLE_EQ(getAsPosition(AxialMethod::ABSOLUTE, 0.03, kInner, kOuter), 0.03);
    EXPECT_DOUBLE_EQ(getAsPosition(AxialMethod::TOP, 0.03, kInner, kOuter), 0.03);
    EXPECT_DOUBLE_EQ(getAsPosition(AxialMethod::AFTER, 0.03, kInner, kOuter), 0.23);
    EXPECT_DOUBLE_EQ(getAsPosition(AxialMethod::MIDDLE, 0.03, kInner, kOuter), 0.105);
    EXPECT_DOUBLE_EQ(getAsPosition(AxialMethod::BOTTOM, 0.03, kInner, kOuter), 0.18);
    // Exactly Java's expressions.
    EXPECT_EQ(getAsPosition(AxialMethod::MIDDLE, 0.03, kInner, kOuter),
              0.03 + ((kOuter - kInner) / 2));
    EXPECT_EQ(getAsPosition(AxialMethod::BOTTOM, -0.03, kInner, kOuter), -0.03 + (kOuter - kInner));
}

TEST(AxialMethod, OffsetFromPosition)
{
    EXPECT_DOUBLE_EQ(getAsOffset(AxialMethod::ABSOLUTE, 0.15, kInner, kOuter), 0.15);
    EXPECT_DOUBLE_EQ(getAsOffset(AxialMethod::TOP, 0.15, kInner, kOuter), 0.15);
    EXPECT_DOUBLE_EQ(getAsOffset(AxialMethod::AFTER, 0.15, kInner, kOuter), -0.05);
    EXPECT_DOUBLE_EQ(getAsOffset(AxialMethod::MIDDLE, 0.15, kInner, kOuter), 0.075);
    EXPECT_NEAR(getAsOffset(AxialMethod::BOTTOM, 0.15, kInner, kOuter), 0.0, 1e-15);
    EXPECT_EQ(getAsOffset(AxialMethod::MIDDLE, 0.15, kInner, kOuter),
              0.15 + ((kInner - kOuter) / 2));
}

TEST(AxialMethod, OffsetAndPositionAreInverses)
{
    for (const AxialMethod method : QtRocket::kAllAxialMethods)
    {
        for (const double offset : {-0.1, 0.0, 0.025, 0.3})
        {
            const double position = getAsPosition(method, offset, kInner, kOuter);
            EXPECT_NEAR(getAsOffset(method, position, kInner, kOuter), offset, 1e-15)
                << axialMethodName(method);
        }
    }
}

TEST(AxialMethod, NothingClampsToZero)
{
    for (const AxialMethod method : QtRocket::kAllAxialMethods)
    {
        EXPECT_FALSE(clampToZero(method));
    }
}

TEST(AxialMethod, ChoicesLeaveOutAfter)
{
    EXPECT_EQ(QtRocket::kAxialOffsetMethods.size(), 4U);
    EXPECT_EQ(QtRocket::kAxialOffsetMethods[0], AxialMethod::ABSOLUTE);
    EXPECT_EQ(QtRocket::kAxialOffsetMethods[1], AxialMethod::TOP);
    EXPECT_EQ(QtRocket::kAxialOffsetMethods[2], AxialMethod::MIDDLE);
    EXPECT_EQ(QtRocket::kAxialOffsetMethods[3], AxialMethod::BOTTOM);
}

TEST(AxialMethod, NamesAndOrkSpelling)
{
    EXPECT_EQ(axialMethodName(AxialMethod::ABSOLUTE), "ABSOLUTE");
    EXPECT_EQ(axialMethodName(AxialMethod::BOTTOM), "BOTTOM");
    EXPECT_EQ(orkName(AxialMethod::ABSOLUTE), "absolute");
    EXPECT_EQ(orkName(AxialMethod::AFTER), "after");
    EXPECT_EQ(orkName(AxialMethod::TOP), "top");
    EXPECT_EQ(orkName(AxialMethod::MIDDLE), "middle");
    EXPECT_EQ(orkName(AxialMethod::BOTTOM), "bottom");
}

TEST(AxialMethod, OrkNamesReadBack)
{
    EXPECT_TRUE(std::ranges::all_of(QtRocket::kAllAxialMethods, [](AxialMethod method) {
        return QtRocket::axialMethodFromOrkName(orkName(method)) == method;
    }));
    EXPECT_EQ(QtRocket::axialMethodFromOrkName(" middle "), AxialMethod::MIDDLE);
    EXPECT_EQ(QtRocket::axialMethodFromOrkName("Middle"), std::nullopt);
    EXPECT_EQ(QtRocket::axialMethodFromOrkName("center"), std::nullopt);
}

TEST(AxialMethod, DisplayKeysAndEnglishNames)
{
    EXPECT_EQ(displayKey(AxialMethod::TOP), "RocketComponent.Position.Method.Axial.TOP");
    EXPECT_EQ(displayName(AxialMethod::ABSOLUTE), "Tip of the rocket");
    EXPECT_EQ(displayName(AxialMethod::AFTER), "After the sibling component");
    EXPECT_EQ(displayName(AxialMethod::MIDDLE), "Middle of the parent component");
}

}  // namespace
