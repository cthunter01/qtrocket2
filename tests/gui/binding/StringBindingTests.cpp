// The model of a text field: a component's name and comment (Java: the private TextFieldListener
// of RocketComponentConfig, which calls setName() and setComment() itself).

#include "QtRocket/gui/binding/StringBinding.h"

#include <array>
#include <memory>
#include <string>
#include <tuple>

#include <gtest/gtest.h>

#include "BindingTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

namespace
{

using StringBindingTest = Test::GuiTest;
using Test::ChangeCounter;
using Test::RocketEventRecorder;
using Test::StringBeanProperty;
using Test::TwoTubeRocket;

/// A binding of @p source on @p component alone.
[[nodiscard]] std::shared_ptr<StringBinding> bindOn(RocketComponent&    component,
                                                    const StringSource& source)
{
    return std::make_shared<StringBinding>(std::make_shared<ComponentSelection>(component), source);
}

TEST_F(StringBindingTest, NeedsASelectionAndASetter)
{
    const TwoTubeRocket design;
    EXPECT_THROW(std::ignore = std::make_shared<StringBinding>(nullptr, componentNameSource()),
                 BugError);
    const StringSource readOnly = StringSource::of<RocketComponent>({
        .get = [](const RocketComponent& component) { return component.getName(); },
    });
    EXPECT_THROW(std::ignore = bindOn(*design.tubeA, readOnly), BugError);
}

TEST_F(StringBindingTest, ShowsTheMainComponentsText)
{
    const TwoTubeRocket design;
    design.tubeA->setComment("line 1\nline 2");
    const auto name    = bindOn(*design.tubeA, componentNameSource());
    const auto comment = bindOn(*design.tubeA, componentCommentSource());
    EXPECT_EQ(name->value(), "A");
    EXPECT_EQ(comment->value(), "line 1\nline 2");
    EXPECT_TRUE(name->isEnabled());
}

TEST_F(StringBindingTest, SetsATextThatDiffersFromTheMainComponents)
{
    // Java: if (!component.getName().equals(field.getText())) component.setName(field.getText()).
    const TwoTubeRocket design;
    StringBeanProperty  bean{.value = "same"};
    const auto          binding = bindOn(*design.tubeA, bean.source());
    const ChangeCounter changes(*binding);
    EXPECT_FALSE(binding->setText("same"));
    EXPECT_EQ(bean.sets, 0);
    EXPECT_TRUE(binding->setText("other"));
    EXPECT_EQ(bean.sets, 1);
    EXPECT_EQ(binding->value(), "other");
    EXPECT_EQ(changes.count(), 1);
}

TEST_F(StringBindingTest, ABlankNameGivesTheDefaultNameBack)
{
    // RocketComponent::setName(): a blank name is the component's default name (Java: the same).
    const TwoTubeRocket design;
    const auto          binding = bindOn(*design.tubeA, componentNameSource());
    EXPECT_TRUE(binding->setText("   "));
    EXPECT_EQ(design.tubeA->getName(), "Body Tube");
    EXPECT_EQ(binding->value(), "Body Tube");
}

TEST_F(StringBindingTest, AMultiEditRenamesEverySelectedComponentInOneEvent)
{
    // Java's setName() and setComment() forward the text to every config listener.
    const TwoTubeRocket                   design;
    const std::array<RocketComponent*, 1> others{design.tubeB};
    const auto selection = std::make_shared<ComponentSelection>(*design.tubeA, others);
    const auto name      = std::make_shared<StringBinding>(selection, componentNameSource());
    const auto comment   = std::make_shared<StringBinding>(selection, componentCommentSource());
    const RocketEventRecorder events(*design.rocket);
    EXPECT_TRUE(name->setText("Tube"));
    EXPECT_EQ(design.tubeA->getName(), "Tube");
    EXPECT_EQ(design.tubeB->getName(), "Tube");
    EXPECT_EQ(events.count(), 1U);
    EXPECT_TRUE(comment->setText("glued"));
    EXPECT_EQ(design.tubeA->getComment(), "glued");
    EXPECT_EQ(design.tubeB->getComment(), "glued");
    EXPECT_EQ(events.count(), 2U);
}

TEST_F(StringBindingTest, FollowsChangesElsewhere)
{
    const TwoTubeRocket design;
    const auto          binding = bindOn(*design.tubeA, componentNameSource());
    const ChangeCounter changes(*binding);
    design.tubeA->setName("Renamed");
    EXPECT_EQ(binding->value(), "Renamed");
    EXPECT_EQ(changes.count(), 1);
    design.tubeB->setLength(0.9);  // another component: the text is the same
    EXPECT_EQ(changes.count(), 1);
}

TEST_F(StringBindingTest, AnInvalidSelectionTakesNothing)
{
    const TwoTubeRocket design;
    StringBeanProperty  bean{.value = "kept"};
    const auto          binding = bindOn(*design.tubeA, bean.source());
    const ChangeCounter changes(*binding);
    std::ignore = design.stage->addChild(std::make_unique<BodyTube>());
    EXPECT_FALSE(binding->isValid());
    EXPECT_FALSE(binding->isEnabled());
    EXPECT_EQ(changes.count(), 1);
    EXPECT_FALSE(binding->setText("lost"));
    EXPECT_EQ(bean.sets, 0);
    EXPECT_EQ(binding->value(), "kept");
}

TEST_F(StringBindingTest, AReleasedSelectionTakesNothing)
{
    const TwoTubeRocket design;
    StringBeanProperty  bean{.value = "kept"};
    const auto          binding = bindOn(*design.tubeA, bean.source());
    binding->getSelection()->release();
    EXPECT_FALSE(binding->isValid());
    EXPECT_FALSE(binding->setText("lost"));
    EXPECT_EQ(bean.sets, 0);
}

TEST_F(StringBindingTest, AWriteWhileTheBindingNotifiesIsDropped)
{
    const TwoTubeRocket design;
    StringBeanProperty  bean{.value = "first"};
    const auto          binding = bindOn(*design.tubeA, bean.source());
    struct Echo
    {
        bool taken{true};
    } echo;
    const Signal<>::ScopedConnection connection(
        binding->addChangeListener([&binding, &echo] { echo.taken = binding->setText("echo"); }));
    bean.value = "second";
    design.tubeA->fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    EXPECT_FALSE(echo.taken);
    EXPECT_EQ(bean.value, "second");
    EXPECT_EQ(bean.sets, 0);
}

TEST_F(StringBindingTest, TheSourcesIsEnabledDisablesTheField)
{
    const TwoTubeRocket design;
    StringBeanProperty  bean{.value = "text", .enabled = false};
    const auto          binding = bindOn(*design.tubeA, bean.source());
    EXPECT_FALSE(binding->isEnabled());
    bean.enabled = true;
    design.tubeA->fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    EXPECT_TRUE(binding->isEnabled());
}

}  // namespace

}  // namespace QtRocket::Gui
