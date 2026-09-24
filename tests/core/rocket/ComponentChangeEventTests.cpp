#include "QtRocket/rocket/ComponentChangeEvent.h"

#include <optional>

#include <gtest/gtest.h>

#include "QtRocket/util/BugError.h"
#include "rocket/TestComponent.h"

namespace
{

using QtRocket::BugError;
using QtRocket::ComponentChangeEvent;
using QtRocket::Test::TestComponent;
using Type = ComponentChangeEvent::Type;

// ---- Ported from ComponentChangeEventTest.java ----

class ComponentChangeEventTest : public ::testing::Test
{
protected:
    ComponentChangeEvent createEvent(int type) { return ComponentChangeEvent{&m_source, type}; }

    TestComponent m_source;
};

TEST_F(ComponentChangeEventTest, NonFunctionalChangeIsNonFunctional)
{
    const ComponentChangeEvent event = createEvent(ComponentChangeEvent::kNonFunctionalChange);
    EXPECT_TRUE(event.isNonFunctionalChange());
    EXPECT_FALSE(event.isFunctionalChange());
}

TEST_F(ComponentChangeEventTest, TextureChangeIsNonFunctional)
{
    const ComponentChangeEvent event = createEvent(ComponentChangeEvent::kTextureChange);
    EXPECT_TRUE(event.isNonFunctionalChange());
    EXPECT_FALSE(event.isFunctionalChange());
}

TEST_F(ComponentChangeEventTest, GraphicChangeIsNonFunctional)
{
    const ComponentChangeEvent event = createEvent(ComponentChangeEvent::kGraphicChange);
    EXPECT_TRUE(event.isNonFunctionalChange());
    EXPECT_FALSE(event.isFunctionalChange());
}

TEST_F(ComponentChangeEventTest, TextureWithNonFunctionalIsNonFunctional)
{
    const ComponentChangeEvent event = createEvent(ComponentChangeEvent::kTextureChange |
                                                   ComponentChangeEvent::kNonFunctionalChange);
    EXPECT_TRUE(event.isNonFunctionalChange());
    EXPECT_FALSE(event.isFunctionalChange());
}

TEST_F(ComponentChangeEventTest, MassChangeIsFunctional)
{
    const ComponentChangeEvent event = createEvent(ComponentChangeEvent::kMassChange);
    EXPECT_FALSE(event.isNonFunctionalChange());
    EXPECT_TRUE(event.isFunctionalChange());
}

TEST_F(ComponentChangeEventTest, AerodynamicChangeIsFunctional)
{
    const ComponentChangeEvent event = createEvent(ComponentChangeEvent::kAerodynamicChange);
    EXPECT_FALSE(event.isNonFunctionalChange());
    EXPECT_TRUE(event.isFunctionalChange());
}

TEST_F(ComponentChangeEventTest, TextureWithMassChangeIsFunctional)
{
    const ComponentChangeEvent event =
        createEvent(ComponentChangeEvent::kTextureChange | ComponentChangeEvent::kMassChange);
    EXPECT_FALSE(event.isNonFunctionalChange());
    EXPECT_TRUE(event.isFunctionalChange());
}

TEST_F(ComponentChangeEventTest, TreeChangeIsFunctional)
{
    const ComponentChangeEvent event = createEvent(ComponentChangeEvent::kTreeChange);
    EXPECT_FALSE(event.isNonFunctionalChange());
    EXPECT_TRUE(event.isFunctionalChange());
}

TEST_F(ComponentChangeEventTest, MotorChangeIsFunctional)
{
    const ComponentChangeEvent event = createEvent(ComponentChangeEvent::kMotorChange);
    EXPECT_FALSE(event.isNonFunctionalChange());
    EXPECT_TRUE(event.isFunctionalChange());
}

TEST_F(ComponentChangeEventTest, EventChangeIsFunctional)
{
    const ComponentChangeEvent event = createEvent(ComponentChangeEvent::kEventChange);
    EXPECT_FALSE(event.isNonFunctionalChange());
    EXPECT_TRUE(event.isFunctionalChange());
}

// ---- QtRocket additions ----

TEST_F(ComponentChangeEventTest, BitValuesMatchOpenRocket)
{
    EXPECT_EQ(ComponentChangeEvent::kNonFunctionalChange, 1);
    EXPECT_EQ(ComponentChangeEvent::kMassChange, 2);
    EXPECT_EQ(ComponentChangeEvent::kAerodynamicChange, 4);
    EXPECT_EQ(ComponentChangeEvent::kTreeChange, 8);
    EXPECT_EQ(ComponentChangeEvent::kUndoChange, 16);
    EXPECT_EQ(ComponentChangeEvent::kMotorChange, 32);
    EXPECT_EQ(ComponentChangeEvent::kEventChange, 64);
    EXPECT_EQ(ComponentChangeEvent::kTextureChange, 128);
    EXPECT_EQ(ComponentChangeEvent::kGraphicChange, 256);
    EXPECT_EQ(ComponentChangeEvent::kTreeChangeChildren, 512);
    EXPECT_EQ(ComponentChangeEvent::kAeromassChange, 6);
    EXPECT_EQ(ComponentChangeEvent::kBothChange, 6);
    EXPECT_EQ(ComponentChangeEvent::value(Type::ERROR), -1);
    EXPECT_EQ(ComponentChangeEvent::value(Type::TREE_CHILDREN), 512);
}

TEST_F(ComponentChangeEventTest, PredicatesTestTheirBits)
{
    const ComponentChangeEvent all = createEvent(1023);
    EXPECT_TRUE(all.isMassChange());
    EXPECT_TRUE(all.isAerodynamicChange());
    EXPECT_TRUE(all.isTreeChange());
    EXPECT_TRUE(all.isTreeChildrenChange());
    EXPECT_TRUE(all.isUndoChange());
    EXPECT_TRUE(all.isMotorChange());
    EXPECT_TRUE(all.isEventChange());
    EXPECT_TRUE(all.isTextureChange());
    EXPECT_TRUE(all.isFunctionalChange());

    const ComponentChangeEvent none = createEvent(0);
    EXPECT_FALSE(none.isMassChange());
    EXPECT_FALSE(none.isAerodynamicChange());
    EXPECT_FALSE(none.isTreeChange());
    EXPECT_FALSE(none.isUndoChange());
    // No functional bit at all: non-functional, as in Java.
    EXPECT_TRUE(none.isNonFunctionalChange());

    // TREE_CHILDREN alone is functional.
    EXPECT_TRUE(createEvent(ComponentChangeEvent::kTreeChangeChildren).isFunctionalChange());
    // An undo change is functional.
    EXPECT_TRUE(createEvent(ComponentChangeEvent::kUndoChange).isFunctionalChange());
}

TEST_F(ComponentChangeEventTest, KeepsSourceAndType)
{
    const ComponentChangeEvent event = createEvent(ComponentChangeEvent::kAeromassChange);
    EXPECT_EQ(event.getSource(), &m_source);
    EXPECT_EQ(event.getType(), 6);

    const ComponentChangeEvent typed{&m_source, Type::MOTOR};
    EXPECT_EQ(typed.getType(), ComponentChangeEvent::kMotorChange);
    EXPECT_TRUE(typed.isMotorChange());
}

TEST_F(ComponentChangeEventTest, ErrorTypeOrNullSourceIsABug)
{
    EXPECT_THROW((ComponentChangeEvent{&m_source, Type::ERROR}), BugError);
    EXPECT_THROW((ComponentChangeEvent{nullptr, ComponentChangeEvent::kMassChange}), BugError);
    EXPECT_THROW((ComponentChangeEvent{nullptr, Type::MASS}), BugError);
}

TEST_F(ComponentChangeEventTest, TypeFromValueFindsExactValues)
{
    EXPECT_EQ(ComponentChangeEvent::typeFromValue(1), std::optional{Type::NON_FUNCTIONAL});
    EXPECT_EQ(ComponentChangeEvent::typeFromValue(-1), std::optional{Type::ERROR});
    EXPECT_EQ(ComponentChangeEvent::typeFromValue(512), std::optional{Type::TREE_CHILDREN});
    EXPECT_EQ(ComponentChangeEvent::typeFromValue(6), std::nullopt);
    EXPECT_EQ(ComponentChangeEvent::typeFromValue(0), std::nullopt);
}

TEST_F(ComponentChangeEventTest, TypeNamesAreJavas)
{
    EXPECT_EQ(ComponentChangeEvent::typeName(Type::ERROR), "Error");
    EXPECT_EQ(ComponentChangeEvent::typeName(Type::NON_FUNCTIONAL), "nonFunctional");
    EXPECT_EQ(ComponentChangeEvent::typeName(Type::GRAPHIC), "Configuration");
    EXPECT_EQ(ComponentChangeEvent::typeName(Type::TREE_CHILDREN), "TREE_CHILDREN");
    EXPECT_TRUE(ComponentChangeEvent::matches(Type::MASS, 6));
    EXPECT_FALSE(ComponentChangeEvent::matches(Type::TREE, 6));
}

TEST_F(ComponentChangeEventTest, ToStringListsTheFlags)
{
    EXPECT_EQ(createEvent(ComponentChangeEvent::kNonFunctionalChange).toString(),
              "ComponentChangeEvent[nonfunc]");
    EXPECT_EQ(createEvent(ComponentChangeEvent::kAeromassChange).toString(),
              "ComponentChangeEvent[mass,aero]");
    EXPECT_EQ(createEvent(1023).toString(),
              "ComponentChangeEvent[mass,aero,tree,treechild,undo,motor,event]");
    // Texture and graphic changes are non-functional but not listed themselves.
    EXPECT_EQ(createEvent(ComponentChangeEvent::kTextureChange).toString(),
              "ComponentChangeEvent[nonfunc]");
    EXPECT_EQ(createEvent(ComponentChangeEvent::kUndoChange | ComponentChangeEvent::kTreeChange)
                  .toString(),
              "ComponentChangeEvent[tree,undo]");
}

}  // namespace
