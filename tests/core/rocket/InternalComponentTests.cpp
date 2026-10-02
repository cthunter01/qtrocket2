// InternalComponent, through EngineBlock: not aerodynamic but massive, positioned BOTTOM, and its
// axial method setter. OpenRocket has no JUnit test of the class of its own.

#include "QtRocket/rocket/InternalComponent.h"

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "rocket/InternalTestSupport.h"

namespace
{

using QtRocket::AxialMethod;
using QtRocket::ComponentChangeEvent;
using QtRocket::EngineBlock;
using QtRocket::InternalComponent;

constexpr double kEpsilon = 1e-12;

class InternalComponentEvents : public QtRocket::Test::RingEventsFixture
{ };

TEST(InternalComponent, IsMassiveButNotAerodynamicAndPositionedBottom)
{
    const EngineBlock        block;
    const InternalComponent& internal = block;
    EXPECT_FALSE(internal.isAerodynamic());
    EXPECT_TRUE(internal.isMassive());
    EXPECT_EQ(internal.getAxialMethod(), AxialMethod::BOTTOM);
    EXPECT_EQ(internal.getAxialOffset(), 0.0);
    EXPECT_FALSE(internal.isAfter());
}

TEST_F(InternalComponentEvents, SetAxialMethodAlwaysFiresNonFunctional)
{
    auto& block = m_body->addChild(std::make_unique<EngineBlock>());
    m_types.clear();
    block.setAxialMethod(AxialMethod::TOP);
    block.setAxialMethod(AxialMethod::TOP);  // unchanged: fires all the same, as Java
    EXPECT_EQ(m_types, std::vector<int>(2, ComponentChangeEvent::kNonFunctionalChange));
    EXPECT_EQ(block.getAxialMethod(), AxialMethod::TOP);
    // The position is kept: a BOTTOM block of length 0.005 at the end of a 0.3 m body.
    EXPECT_NEAR(block.getAxialOffset(), 0.295, kEpsilon);

    m_types.clear();
    block.setAxialOffset(0.1);
    EXPECT_EQ(m_types, std::vector<int>{ComponentChangeEvent::kAeromassChange});
    EXPECT_NEAR(block.getPosition().x, 0.1, kEpsilon);

    EXPECT_NEAR(block.getAxialOffset(AxialMethod::BOTTOM), 0.1 + 0.005 - 0.3, kEpsilon);
}

}  // namespace
