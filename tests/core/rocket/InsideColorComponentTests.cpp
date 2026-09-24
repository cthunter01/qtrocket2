#include "QtRocket/rocket/InsideColorComponent.h"

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/InsideColorComponentHandler.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Color.h"
#include "rocket/TestComponent.h"

namespace
{

using QtRocket::Appearance;
using QtRocket::AxialStage;
using QtRocket::Color;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::InsideColorComponentHandler;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Test::TestComponent;

/// A rocket with one stage holding one body tube, events enabled, recording the last event.
class InsideColorComponentTest : public ::testing::Test
{
protected:
    InsideColorComponentTest()
    {
        AxialStage& stage = m_rocket.addChild(std::make_unique<AxialStage>());
        m_tube            = &stage.addChild(TestComponent::make(0.2));
        m_rocket.enableEvents();
        m_connection = m_rocket.addComponentChangeListener(
            [this](const ComponentChangeEvent& event) { m_lastType = event.getType(); });
    }

    Rocket                                  m_rocket;
    TestComponent*                          m_tube{nullptr};
    std::optional<int>                      m_lastType;
    ComponentChangeSignal::ScopedConnection m_connection;
};

// ---- Ported from InsideColorComponentHandlerTest.java ----

TEST_F(InsideColorComponentTest, MaterialPartitionChangesFireGraphicEvents)
{
    InsideColorComponentHandler& handler = m_tube->getInsideColorComponentHandler();

    handler.setSeparateInsideOutside(true);
    EXPECT_EQ(std::optional{ComponentChangeEvent::kGraphicChange}, m_lastType);

    m_lastType.reset();
    handler.setEdgesSameAsInside(true);
    EXPECT_EQ(std::optional{ComponentChangeEvent::kGraphicChange}, m_lastType);

    m_lastType.reset();
    handler.setEdgesSameAsInside(true);
    EXPECT_FALSE(m_lastType.has_value())
        << "Assigning the current partition setting should not rebuild the scene";
}

// ---- QtRocket additions ----

TEST_F(InsideColorComponentTest, TheMixinOwnsTheHandler)
{
    static_assert(std::is_base_of_v<QtRocket::InsideColorComponent, TestComponent>);
    QtRocket::InsideColorComponent&       mixin      = *m_tube;
    const QtRocket::InsideColorComponent& constMixin = *m_tube;
    EXPECT_EQ(&mixin.getInsideColorComponentHandler(),
              &constMixin.getInsideColorComponentHandler());
}

TEST_F(InsideColorComponentTest, DefaultsAreOff)
{
    const InsideColorComponentHandler& handler = m_tube->getInsideColorComponentHandler();
    EXPECT_FALSE(handler.getInsideAppearance().has_value());
    EXPECT_FALSE(handler.isSeparateInsideOutside());
    EXPECT_FALSE(handler.isEdgesSameAsInside());
}

TEST_F(InsideColorComponentTest, InsideAppearanceFiresNonFunctionalAlways)
{
    InsideColorComponentHandler& handler = m_tube->getInsideColorComponentHandler();
    const Appearance             inside{Color{1, 2, 3}, 0.4};
    handler.setInsideAppearance(inside);
    EXPECT_EQ(handler.getInsideAppearance(), std::optional{inside});
    EXPECT_EQ(m_lastType, std::optional{ComponentChangeEvent::kNonFunctionalChange});

    m_lastType.reset();
    handler.setInsideAppearance(inside);  // the same value fires again, as in Java
    EXPECT_EQ(m_lastType, std::optional{ComponentChangeEvent::kNonFunctionalChange});

    handler.setInsideAppearance(std::nullopt);
    EXPECT_FALSE(handler.getInsideAppearance().has_value());
}

TEST_F(InsideColorComponentTest, SeparateInsideOutsideUnchangedFiresNothing)
{
    InsideColorComponentHandler& handler = m_tube->getInsideColorComponentHandler();
    handler.setSeparateInsideOutside(false);
    EXPECT_FALSE(m_lastType.has_value());
}

TEST_F(InsideColorComponentTest, CopyFromCopiesTheStateSilently)
{
    InsideColorComponentHandler& handler = m_tube->getInsideColorComponentHandler();
    handler.setInsideAppearance(Appearance{Color{9, 9, 9}, 0.9});
    handler.setSeparateInsideOutside(true);
    handler.setEdgesSameAsInside(true);

    TestComponent other;
    other.getInsideColorComponentHandler().copyFrom(handler);
    EXPECT_EQ(other.getInsideColorComponentHandler().getInsideAppearance(),
              handler.getInsideAppearance());
    EXPECT_TRUE(other.getInsideColorComponentHandler().isSeparateInsideOutside());
    EXPECT_TRUE(other.getInsideColorComponentHandler().isEdgesSameAsInside());

    TestComponent third;
    third.setInsideColorComponentHandler(handler);
    EXPECT_TRUE(third.getInsideColorComponentHandler().isEdgesSameAsInside());
}

TEST_F(InsideColorComponentTest, ACopiedComponentNotifiesAsItself)
{
    InsideColorComponentHandler& handler = m_tube->getInsideColorComponentHandler();
    handler.setSeparateInsideOutside(true);

    // The copy has its own handler with the same state, bound to the copy: once the copy is in
    // the rocket, its handler fires with the copy as the source.
    std::unique_ptr<RocketComponent> copy   = m_tube->copyWithNewIds();
    auto&                            copied = dynamic_cast<TestComponent&>(*copy);
    EXPECT_TRUE(copied.getInsideColorComponentHandler().isSeparateInsideOutside());
    EXPECT_NE(&copied.getInsideColorComponentHandler(), &handler);

    m_tube->getParent()->addChild(std::move(copy));
    const RocketComponent* source = nullptr;
    const auto             connection =
        ComponentChangeSignal::ScopedConnection{m_rocket.addComponentChangeListener(
            [&source](const ComponentChangeEvent& event) { source = event.getSource(); })};
    copied.getInsideColorComponentHandler().setEdgesSameAsInside(true);
    EXPECT_EQ(source, &copied);
    EXPECT_FALSE(handler.isEdgesSameAsInside());
}

TEST_F(InsideColorComponentTest, DetachedComponentFiresNothing)
{
    TestComponent detached;
    detached.getInsideColorComponentHandler().setSeparateInsideOutside(true);
    EXPECT_TRUE(detached.getInsideColorComponentHandler().isSeparateInsideOutside());
    EXPECT_FALSE(m_lastType.has_value());
}

}  // namespace
