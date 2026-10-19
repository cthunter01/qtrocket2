// The shapes of body tubes, transitions, nose cones, ring components, launch lugs and tube fin
// sets of example designs and test rockets, compared with what OpenRocket's providers returned
// for the same instances in the side, top and back views (BodyShapePins.h).
//
// Each pin holds the transformation OpenRocket's RocketFigure handed the provider for an
// instance in a view. The test checks that QtRocket's own instance and figure rotation give the
// same transformation (within the sin/cos tolerance), then hands the provider Java's exact
// transformation and compares the shapes exactly: they come from +, -, * and / of the
// component's dimensions and the transformation, the outlines of ogive, ellipsoid, parabolic and
// conical shapes from sqrt as well. A power or Haack outline goes through pow, or acos and sin,
// and is compared within kTranscendentalTolerance.

#include <string>

#include <gtest/gtest.h>

#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "figure/BodyShapePins.h"
#include "figure/DesignPinSupport.h"
#include "figure/FigureTestSupport.h"
#include "figure/ShapePins.h"

namespace
{

using QtRocket::RocketComponent;
using QtRocket::Transition;
using QtRocket::TransitionShape;
using QtRocket::Test::DesignPin;
using QtRocket::Test::kTranscendentalTolerance;
using QtRocket::Test::PinnedDesign;
using QtRocket::Test::BodyShapePins::kDesigns;

/// The tolerance of @p component's shapes: an outline sampled from a power or Haack profile
/// went through pow, acos and sin.
[[nodiscard]] double shapeTolerance(const RocketComponent& component)
{
    const auto* transition = dynamic_cast<const Transition*>(&component);
    if (transition != nullptr && (transition->getShapeType() == TransitionShape::POWER ||
                                  transition->getShapeType() == TransitionShape::HAACK))
    {
        return kTranscendentalTolerance;
    }
    return 0;
}

class BodyShapesOfDesigns : public ::testing::TestWithParam<DesignPin>
{ };

TEST_P(BodyShapesOfDesigns, AsJava)
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

INSTANTIATE_TEST_SUITE_P(Pins, BodyShapesOfDesigns, ::testing::ValuesIn(kDesigns), designTestName);

}  // namespace
