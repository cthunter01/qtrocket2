#include "QtRocket/aero/barrowman/ComponentAssemblyCalc.h"

#include <cmath>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/barrowman/RocketComponentCalc.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Transformation.h"

namespace
{

using QtRocket::AerodynamicForces;
using QtRocket::AxialStage;
using QtRocket::ComponentAssemblyCalc;
using QtRocket::Coordinate;
using QtRocket::FlightConditions;
using QtRocket::ModId;
using QtRocket::RocketComponentCalc;
using QtRocket::Transformation;
using QtRocket::WarningSet;

TEST(ComponentAssemblyCalc, AddsNoForcesAndNoDrag)
{
    const AxialStage      stage;
    ComponentAssemblyCalc calc{stage};
    RocketComponentCalc&  base = calc;

    FlightConditions conditions;
    conditions.setMach(1.2);
    conditions.setAOA(0.1);
    WarningSet        warnings;
    AerodynamicForces forces;
    forces.setCN(0.5);
    forces.setCP(Coordinate{0.3, 0, 0, 2});
    const ModId before = forces.modId();

    base.calculateNonaxialForces(conditions, Transformation::kIdentity, forces, warnings);
    EXPECT_EQ(forces.modId(), before);  // untouched
    EXPECT_EQ(forces.getCN(), 0.5);
    EXPECT_TRUE(std::isnan(forces.getCm()));

    EXPECT_EQ(base.calculateFrictionCD(conditions, 0.004, warnings), 0.0);
    EXPECT_EQ(base.calculatePressureCD(conditions, 0.85, 0.12, warnings), 0.0);
    EXPECT_EQ(base.calculateComponentBaseCD(conditions, 0.12, warnings), 0.0);
    EXPECT_TRUE(warnings.empty());
}

}  // namespace
