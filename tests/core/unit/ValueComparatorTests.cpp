#include "QtRocket/unit/ValueComparator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/Value.h"

namespace
{

using QtRocket::GeneralUnit;
using QtRocket::Value;
using QtRocket::ValueComparator;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// ---- QtRocket additions ----

TEST(ValueComparator, OrdersByCompareTo)
{
    const GeneralUnit     cm(0.01, "cm");
    const GeneralUnit     m(1, "m");
    const ValueComparator less;
    EXPECT_TRUE(less(Value(100.0, cm), Value(0.001, m)));  // "cm" before "m"
    EXPECT_FALSE(less(Value(0.001, m), Value(100.0, cm)));
    EXPECT_TRUE(less(Value(0.1, cm), Value(0.2, cm)));
    EXPECT_FALSE(less(Value(0.1, cm), Value(0.1, cm)));
    EXPECT_TRUE(less(Value(-0.0, cm), Value(0.0, cm)));
    EXPECT_TRUE(less(Value(1e300, cm), Value(kNaN, cm)));
    EXPECT_FALSE(less(Value(kNaN, cm), Value(kNaN, cm)));
}

TEST(ValueComparator, SortsATableColumn)
{
    const GeneralUnit cm(0.01, "cm");
    const GeneralUnit m(1, "m");

    std::vector<Value> sorted{Value(0.3, cm), Value(0.1, m), Value(0.1, cm), Value(kNaN, cm)};
    std::ranges::sort(sorted, ValueComparator{});
    EXPECT_EQ(sorted[0].getValue(), 0.1);
    EXPECT_EQ(&sorted[0].getUnit(), &cm);
    EXPECT_EQ(sorted[1].getValue(), 0.3);
    EXPECT_TRUE(std::isnan(sorted[2].getValue()));
    EXPECT_EQ(&sorted[3].getUnit(), &m);
}

}  // namespace
