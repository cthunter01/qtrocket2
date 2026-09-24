#include "QtRocket/models/InterpolatingAtmosphericModel.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/models/ExtendedIsaModel.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/ModId.h"

namespace
{

using QtRocket::AtmosphericConditions;
using QtRocket::BugError;
using QtRocket::ExtendedIsaModel;
using QtRocket::InterpolatingAtmosphericModel;
using QtRocket::ModId;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// ExtendedISAModelTest.LinearHumidityInterpolatingModel: the humidity rises linearly from 0 at
/// sea level to 1 at 1000 m, the table stops at 1000 m. Also counts the exact evaluations.
class LinearHumidityInterpolatingModel final : public InterpolatingAtmosphericModel
{
public:
    [[nodiscard]] ModId modId() const override { return ModId::zero(); }

    [[nodiscard]] int evaluations() const noexcept { return m_evaluations.load(); }
    [[nodiscard]] const std::vector<double>& altitudes() const noexcept { return m_altitudes; }

protected:
    [[nodiscard]] double getMaxAltitude() const override { return 1000; }

    [[nodiscard]] AtmosphericConditions getExactConditions(double altitude) const override
    {
        ++m_evaluations;
        m_altitudes.push_back(altitude);
        const double relativeHumidity = std::max(0.0, std::min(1.0, altitude / 1000.0));
        return AtmosphericConditions{288.15, 101325.0, relativeHumidity};
    }

private:
    mutable std::atomic<int>    m_evaluations{0};
    mutable std::vector<double> m_altitudes;  // only written while the table is built
};

/// A model whose table is empty (the maximum altitude is not positive).
class EmptyTableModel final : public InterpolatingAtmosphericModel
{
public:
    [[nodiscard]] ModId modId() const override { return ModId::zero(); }

    [[nodiscard]] std::vector<double> sampledAltitudes() const { return tableAltitudes(); }

protected:
    [[nodiscard]] double                getMaxAltitude() const override { return 0; }
    [[nodiscard]] AtmosphericConditions getExactConditions(double /*altitude*/) const override
    {
        return AtmosphericConditions{};
    }
};

/// A model with a linear temperature and pressure profile over 2750 m (six levels, the last at
/// 2500 m).
class LinearProfileModel final : public InterpolatingAtmosphericModel
{
public:
    [[nodiscard]] ModId modId() const override { return ModId::zero(); }

    [[nodiscard]] std::vector<double> sampledAltitudes() const { return tableAltitudes(); }

protected:
    [[nodiscard]] double                getMaxAltitude() const override { return 2750; }
    [[nodiscard]] AtmosphericConditions getExactConditions(double altitude) const override
    {
        return AtmosphericConditions{300.0 - (altitude / 100), 100000.0 - (altitude * 10), 0};
    }
};

// ---- Ported from InterpolatingAtmosphericModelTest.java ----

TEST(InterpolatingAtmosphericModel, InterpolationIsSmoothBetweenLayers)
{
    const ExtendedIsaModel model;  // the ISA model exercises the interpolation

    // Points between the 500 m levels
    const AtmosphericConditions cond1 = model.getConditions(250);
    const AtmosphericConditions cond2 = model.getConditions(500);
    const AtmosphericConditions cond3 = model.getConditions(750);

    // Monotonic values
    EXPECT_GE(cond1.getPressure(), cond2.getPressure());
    EXPECT_GE(cond2.getPressure(), cond3.getPressure());

    // No sudden jumps (within 10%)
    const double pressureDiff1 = std::abs(cond1.getPressure() - cond2.getPressure());
    const double pressureDiff2 = std::abs(cond2.getPressure() - cond3.getPressure());
    EXPECT_LT(std::abs(pressureDiff1 - pressureDiff2), pressureDiff1 * 0.1);
}

// ---- Ported from ExtendedISAModelTest.java (testInterpolatedRelativeHumidity) ----

TEST(InterpolatingAtmosphericModel, InterpolatesRelativeHumidity)
{
    const LinearHumidityInterpolatingModel model;

    EXPECT_NEAR(model.getConditions(0).getRelativeHumidity(), 0.0, 1e-12);
    EXPECT_NEAR(model.getConditions(250).getRelativeHumidity(), 0.25, 1e-12);
    EXPECT_NEAR(model.getConditions(500).getRelativeHumidity(), 0.5, 1e-12);
}

// ---- QtRocket additions ----

TEST(InterpolatingAtmosphericModel, TableIsBuiltOnceOnFirstUse)
{
    const LinearHumidityInterpolatingModel model;
    EXPECT_EQ(model.evaluations(), 0);

    (void)model.getConditions(100);
    // ceil(1000 / 500) = 2 levels, at 0 and 500 m: the maximum altitude itself is not a level.
    EXPECT_EQ(model.evaluations(), 2);
    EXPECT_EQ(model.altitudes(), (std::vector<double>{0.0, 500.0}));

    (void)model.getConditions(700);
    (void)model.getConditions(-5);
    EXPECT_EQ(model.evaluations(), 2);
}

TEST(InterpolatingAtmosphericModel, TableAltitudesAreWhereTheTableSamples)
{
    // ceil(2750 / 500) = 6 levels from 0 m; none for a maximum altitude that is not positive.
    EXPECT_EQ(LinearProfileModel{}.sampledAltitudes(),
              (std::vector<double>{0.0, 500.0, 1000.0, 1500.0, 2000.0, 2500.0}));
    EXPECT_TRUE(EmptyTableModel{}.sampledAltitudes().empty());
}

TEST(InterpolatingAtmosphericModel, QuotientBelowALevelNeverRoundsUpToIt)
{
    // Why the clamp of the lower index is only defensive: the largest altitude below level n
    // divided by kDelta = 500 still floors to n - 1 (far beyond any table's 172 levels).
    constexpr double kDelta = InterpolatingAtmosphericModel::kDelta;
    int              wrong  = 0;
    for (int n = 1; n <= 1000000; ++n)
    {
        const double below = std::nextafter(kDelta * static_cast<double>(n), 0.0);
        if (std::floor(below / kDelta) != static_cast<double>(n - 1))
        {
            ++wrong;
        }
    }
    EXPECT_EQ(wrong, 0);
}

TEST(InterpolatingAtmosphericModel, EndsOfTheTableAreClamped)
{
    const LinearHumidityInterpolatingModel model;
    // At or below 0 m: the first level; at or above the last level (500 m): the last level.
    EXPECT_EQ(model.getConditions(-1000).getRelativeHumidity(), 0.0);
    EXPECT_EQ(model.getConditions(500).getRelativeHumidity(), 0.5);
    EXPECT_EQ(model.getConditions(900).getRelativeHumidity(), 0.5);
    EXPECT_EQ(model.getConditions(1e12).getRelativeHumidity(), 0.5);
    EXPECT_NEAR(model.getConditions(499.999).getRelativeHumidity(), 0.499999, 1e-12);
}

TEST(InterpolatingAtmosphericModel, InterpolatesEveryQuantityLinearly)
{
    const LinearProfileModel model;
    // Levels every 500 m up to 2500 m (ceil(2750 / 500) = 6).
    const AtmosphericConditions at1250 = model.getConditions(1250);
    EXPECT_NEAR(at1250.getTemperature(), 287.5, 1e-12);
    EXPECT_NEAR(at1250.getPressure(), 87500.0, 1e-9);

    const AtmosphericConditions at1 = model.getConditions(1);
    EXPECT_NEAR(at1.getTemperature(), 299.99, 1e-12);
    EXPECT_NEAR(at1.getPressure(), 99990.0, 1e-9);

    // Beyond 2500 m the last level holds, although the profile goes on to 2750 m.
    EXPECT_EQ(model.getConditions(2600).getTemperature(), 275.0);
    EXPECT_EQ(model.getConditions(2600).getPressure(), 75000.0);
}

TEST(InterpolatingAtmosphericModel, NaNAltitudeGivesNaN)
{
    const LinearProfileModel    model;
    const AtmosphericConditions conditions = model.getConditions(kNaN);
    EXPECT_TRUE(std::isnan(conditions.getTemperature()));
    EXPECT_TRUE(std::isnan(conditions.getPressure()));
}

TEST(InterpolatingAtmosphericModel, EmptyTableIsABug)
{
    const EmptyTableModel model;
    EXPECT_THROW((void)model.getConditions(100), BugError);
    EXPECT_THROW((void)model.getConditions(0), BugError);
}

TEST(InterpolatingAtmosphericModel, ReturnedConditionsAreCopies)
{
    // Java hands out its cached level objects at the ends of the table; here a caller's change
    // stays with the caller.
    const LinearHumidityInterpolatingModel model;
    AtmosphericConditions                  first = model.getConditions(0);
    first.setTemperature(100.0);
    EXPECT_EQ(model.getConditions(0).getTemperature(), 288.15);
}

}  // namespace
