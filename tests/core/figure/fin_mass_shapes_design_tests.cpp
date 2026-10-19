// The shapes of fin sets, mass components, parachutes, streamers, shock cords, rail buttons, pod
// sets and booster sets of example designs and test rockets, compared with what OpenRocket's
// providers returned for the same instances in the side, top and back views
// (FinMassShapePins.h), as body_shapes_design_tests.cpp compares the body components.
//
// The provider is handed Java's exact transformation, and the shapes are compared exactly, but
// for what goes through sin, cos or atan2: a rail button's view angle, a tracker's arcs and an
// off-axis mass object within kTranscendentalTolerance, and the float outlines of canted and
// elliptical fins (whose points went through sin and cos before the rounding) within
// kFloatTolerance.

#include <string>

#include <gtest/gtest.h>

#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/MathUtil.h"
#include "figure/DesignPinSupport.h"
#include "figure/FigureTestSupport.h"
#include "figure/FinMassShapePins.h"
#include "figure/ShapePins.h"

namespace
{

using QtRocket::ComponentKind;
using QtRocket::FinSet;
using QtRocket::MassComponent;
using QtRocket::MassObject;
using QtRocket::RocketComponent;
using QtRocket::Test::DesignPin;
using QtRocket::Test::kFloatTolerance;
using QtRocket::Test::kTranscendentalTolerance;
using QtRocket::Test::PinnedDesign;
using QtRocket::Test::FinMassShapePins::kDesigns;

/// The tolerance of @p component's shapes (see the file comment).
[[nodiscard]] double shapeTolerance(const RocketComponent& component)
{
    if (component.kind() == ComponentKind::RAIL_BUTTON)
    {
        return kTranscendentalTolerance;
    }
    if (const auto* fins = dynamic_cast<const FinSet*>(&component))
    {
        const bool canted = !QtRocket::MathUtil::equals(fins->getCantAngle(), 0);
        return canted || component.kind() == ComponentKind::ELLIPTICAL_FIN_SET ? kFloatTolerance
                                                                               : 0;
    }
    if (const auto* massObject = dynamic_cast<const MassObject*>(&component))
    {
        const auto* massComponent = dynamic_cast<const MassComponent*>(&component);
        const bool  tracker =
            massComponent != nullptr &&
            massComponent->getMassComponentType() == MassComponent::MassComponentType::TRACKER;
        return tracker || massObject->getRadialPosition() != 0 ? kTranscendentalTolerance : 0;
    }
    return 0;
}

class FinMassShapesOfDesigns : public ::testing::TestWithParam<DesignPin>
{ };

TEST_P(FinMassShapesOfDesigns, AsJava)
{
    const DesignPin&   pin    = GetParam();
    const PinnedDesign design = QtRocket::Test::loadPinnedDesign(pin.source, pin.name);
    ASSERT_TRUE(design.isLoaded());
    EXPECT_TRUE(QtRocket::Test::designInstancesAsJava(pin, design.rocket(), &shapeTolerance));
}

[[nodiscard]] std::string designTestName(const ::testing::TestParamInfo<DesignPin>& info)
{
    return QtRocket::Test::pinnedDesignTestName(info.param.source, info.param.name);
}

INSTANTIATE_TEST_SUITE_P(Pins, FinMassShapesOfDesigns, ::testing::ValuesIn(kDesigns),
                         designTestName);

}  // namespace
