// The selected components of a configuration dialog (OpenRocket's config listeners, replaced by
// the user's decision U3: each selected component gets its own setter call, the others first and
// the main component last, inside one freeze).

#include "QtRocket/gui/binding/ComponentSelection.h"

#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "BindingTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using ComponentSelectionTest = Test::GuiTest;
using Test::RocketEventRecorder;
using Test::TwoTubeRocket;
using Phase = ComponentSelection::Phase;

/// Records the phases a selection sends.
class PhaseRecorder
{
public:
    explicit PhaseRecorder(ComponentSelection& selection)
      : m_connection(selection.addListener([this](Phase phase) { m_phases.push_back(phase); }))
    {
    }

    [[nodiscard]] const std::vector<Phase>& phases() const noexcept { return m_phases; }
    void                                    clear() { m_phases.clear(); }

private:
    std::vector<Phase>                                m_phases;
    ComponentSelection::PhaseSignal::ScopedConnection m_connection;
};

/// A listener that logs the phases it hears as "<its name> read" and "<its name> notify".
struct NamedPhaseLog
{
    std::vector<std::string>* log;
    std::string               name;

    void operator()(Phase phase) const
    {
        log->emplace_back(name + (phase == Phase::READ ? " read" : " notify"));
    }
};

/// A setter for apply(): the length of a body tube.
struct SetLength
{
    double length;

    void operator()(BodyTube& tube) const { tube.setLength(length); }
};

/// SetLength that throws once it has set the length of the component named "A".
struct SetLengthThenFailOnA
{
    double length;

    void operator()(BodyTube& tube) const
    {
        tube.setLength(length);
        if (tube.getName() == "A")
        {
            throw std::runtime_error("the main component's setter failed");
        }
    }
};

/// A setter for applyToEach(): the name of any component.
struct SetName
{
    std::string name;

    void operator()(RocketComponent& component) const { component.setName(name); }
};

/// Counts the components it is applied to.
struct CountComponents
{
    int* count;

    void operator()(SymmetricComponent& /*component*/) const { ++*count; }
};

/// The names of the components applyToEach() reaches, in order.
[[nodiscard]] std::vector<std::string> namesReached(ComponentSelection& selection)
{
    std::vector<std::string> names;
    selection.applyToEach(
        [&names](RocketComponent& component) { names.push_back(component.getName()); });
    return names;
}

/// A rocket listener that drops the last owner of a selection (a dialog closing on the event).
struct DropOwner
{
    std::shared_ptr<ComponentSelection>* owner;

    void operator()(const ComponentChangeEvent& /*event*/) const { owner->reset(); }
};

/// A selection listener that drops the last owner of its selection when it becomes invalid.
struct DropOwnerWhenInvalidated
{
    std::shared_ptr<ComponentSelection>* owner;

    void operator()(Phase phase) const
    {
        if (phase == Phase::INVALIDATED)
        {
            owner->reset();
        }
    }
};

/// The names of the body tubes apply<BodyTube>() reaches, in order.
[[nodiscard]] std::vector<std::string> tubesReached(ComponentSelection& selection)
{
    std::vector<std::string> names;
    selection.apply<BodyTube>([&names](BodyTube& tube) { names.push_back(tube.getName()); });
    return names;
}

/// A setter that freezes the rocket itself, as RocketComponent::loadPreset() does: the length of
/// a body tube inside its own freeze, recording whether the rocket was frozen before.
struct SetLengthInOwnFreeze
{
    double             length;
    std::vector<bool>* frozenBefore;

    void operator()(RocketComponent& component) const
    {
        Rocket& rocket = component.getRocket();
        frozenBefore->push_back(rocket.isFrozen());
        rocket.freeze();
        dynamic_cast<BodyTube&>(component).setLength(length);
        rocket.thaw();
    }
};

/// SetLength for applyToEach() and applyToEachUnfrozen(): every component is a body tube.
struct SetEachLength
{
    double length;

    void operator()(RocketComponent& component) const
    {
        dynamic_cast<BodyTube&>(component).setLength(length);
    }
};

/// Records each component it is called for, with the number of events the rocket fired so far.
struct NameAndEventCount
{
    const RocketEventRecorder* events;
    std::vector<std::string>*  log;

    void operator()(RocketComponent& component) const
    {
        log->push_back(component.getName() + " after " + std::to_string(events->count()));
    }
};

/// Adds a body tube to the stage of the first component it is called for (a TREE change),
/// recording every component it reaches.
struct AddTubeToStage
{
    std::vector<std::string>* reached;

    void operator()(RocketComponent& component) const
    {
        reached->push_back(component.getName());
        std::ignore = component.getParent()->addChild(std::make_unique<BodyTube>());
    }
};

/// Sets the length of a body tube and throws for the one named "B".
struct SetLengthThenFailOnB
{
    double length;

    void operator()(RocketComponent& component) const
    {
        dynamic_cast<BodyTube&>(component).setLength(length);
        if (component.getName() == "B")
        {
            throw std::runtime_error("an other component's setter failed");
        }
    }
};

/// A selection listener that releases its selection on Phase::NOTIFY (a dialog closing while the
/// widgets are told).
struct ReleaseOnNotify
{
    ComponentSelection* selection;

    void operator()(Phase phase) const
    {
        if (phase == Phase::NOTIFY)
        {
            selection->release();
        }
    }
};

}  // namespace

TEST_F(ComponentSelectionTest, TheMainComponentIsAppliedLastAfterTheOthersInTheirOrder)
{
    const TwoTubeRocket design;
    design.nose->setName("Nose");
    const std::array<RocketComponent*, 2> others{design.tubeB, design.nose};
    ComponentSelection                    selection(*design.tubeA, others);
    EXPECT_EQ(namesReached(selection), (std::vector<std::string>{"B", "Nose", "A"}));
    EXPECT_EQ(&selection.getMain(), design.tubeA);
    ASSERT_EQ(selection.getOthers().size(), 2U);
    EXPECT_EQ(selection.getOthers()[0], design.tubeB);
    EXPECT_EQ(selection.getOthers()[1], design.nose);
    EXPECT_EQ(&selection.getRocket(), design.rocket);
    EXPECT_TRUE(selection.isMultiEdit());
    EXPECT_FALSE(selection.isAllOfOneKind());
}

TEST_F(ComponentSelectionTest, ApplyReachesOnlyTheComponentsOfItsClass)
{
    const TwoTubeRocket                   design;
    const std::array<RocketComponent*, 2> others{design.nose, design.tubeB};
    ComponentSelection                    selection(*design.tubeA, others);
    EXPECT_EQ(tubesReached(selection), (std::vector<std::string>{"B", "A"}));

    // A nose cone and a body tube are both symmetric components.
    int symmetric = 0;
    selection.apply<SymmetricComponent>(CountComponents{&symmetric});
    EXPECT_EQ(symmetric, 3);
}

TEST_F(ComponentSelectionTest, TheMainComponentAndRepeatedOthersAreLeftOutOfTheOthers)
{
    // Java's addConfigListener() refuses the component itself and a listener it already has.
    const TwoTubeRocket                   design;
    const std::array<RocketComponent*, 3> others{design.tubeB, design.tubeA, design.tubeB};
    const ComponentSelection              selection(*design.tubeA, others);
    ASSERT_EQ(selection.getOthers().size(), 1U);
    EXPECT_EQ(selection.getOthers()[0], design.tubeB);
    EXPECT_TRUE(selection.isAllOfOneKind());

    const ComponentSelection single(*design.tubeA);
    EXPECT_FALSE(single.isMultiEdit());
    EXPECT_TRUE(single.isAllOfOneKind());
}

TEST_F(ComponentSelectionTest, EveryComponentMustBeInTheMainComponentsRocket)
{
    const TwoTubeRocket design;
    const TwoTubeRocket other;
    BodyTube            detached;
    EXPECT_THROW(ComponentSelection{detached}, BugError);

    const std::array<RocketComponent*, 1> foreign{other.tubeA};
    EXPECT_THROW((ComponentSelection{*design.tubeA, foreign}), BugError);
    const std::array<RocketComponent*, 1> null{nullptr};
    EXPECT_THROW((ComponentSelection{*design.tubeA, null}), BugError);
    const std::array<RocketComponent*, 1> loose{&detached};
    EXPECT_THROW((ComponentSelection{*design.tubeA, loose}), BugError);

    // The rocket itself can be edited (its own dialog).
    const ComponentSelection rocket(*design.rocket);
    EXPECT_EQ(&rocket.getMain(), design.rocket);
}

TEST_F(ComponentSelectionTest, AnEditOfSeveralComponentsFiresOneMergedEvent)
{
    const TwoTubeRocket                   design;
    const std::array<RocketComponent*, 1> others{design.tubeB};
    ComponentSelection                    selection(*design.tubeA, others);
    RocketEventRecorder                   events(*design.rocket);
    selection.apply<BodyTube>(SetLength{0.4});
    EXPECT_EQ(design.tubeA->getLength(), 0.4);
    EXPECT_EQ(design.tubeB->getLength(), 0.4);
    ASSERT_EQ(events.count(), 1U);
    // The source of the merged event is the last queued one's: the main component.
    EXPECT_EQ(events.sources().front(), "A");
    EXPECT_FALSE(design.rocket->isFrozen());

    // Java fires nothing when the main component's value does not change, although the others
    // change (the bindings scout's MultiEditProbe: events 0); here the others' events are merged.
    design.tubeB->setLength(0.5);
    events.clear();
    selection.apply<BodyTube>(SetLength{0.4});
    EXPECT_EQ(design.tubeB->getLength(), 0.4);
    ASSERT_EQ(events.count(), 1U);
    EXPECT_EQ(events.sources().front(), "B");
}

TEST_F(ComponentSelectionTest, ARocketFrozenAlreadyIsNotFrozenAgain)
{
    // Rocket::freeze() on a frozen rocket is a BugError: apply() takes no second freeze, and the
    // events come at the caller's thaw.
    const TwoTubeRocket                   design;
    const std::array<RocketComponent*, 1> others{design.tubeB};
    ComponentSelection                    selection(*design.tubeA, others);
    const RocketEventRecorder             events(*design.rocket);
    const PhaseRecorder                   phases(selection);
    design.rocket->freeze();
    EXPECT_NO_THROW(selection.apply<BodyTube>(SetLength{0.25}));
    EXPECT_TRUE(design.rocket->isFrozen());
    EXPECT_EQ(events.count(), 0U);
    // No event reached the selection, so it refreshed its bindings itself.
    EXPECT_EQ(phases.phases(), (std::vector<Phase>{Phase::READ, Phase::NOTIFY}));
    design.rocket->thaw();
    EXPECT_EQ(events.count(), 1U);
    EXPECT_EQ(design.tubeA->getLength(), 0.25);
    EXPECT_EQ(design.tubeB->getLength(), 0.25);
}

TEST_F(ComponentSelectionTest, AnExceptionThawsTheRocketAndKeepsWhatWasSet)
{
    const TwoTubeRocket                   design;
    const std::array<RocketComponent*, 1> others{design.tubeB};
    ComponentSelection                    selection(*design.tubeA, others);
    const RocketEventRecorder             events(*design.rocket);
    EXPECT_THROW(selection.apply<BodyTube>(SetLengthThenFailOnA{0.45}), std::runtime_error);
    EXPECT_FALSE(design.rocket->isFrozen());
    EXPECT_EQ(events.count(), 1U);
    EXPECT_EQ(design.tubeA->getLength(), 0.45);
    EXPECT_EQ(design.tubeB->getLength(), 0.45);
    EXPECT_TRUE(selection.isValid());
}

TEST_F(ComponentSelectionTest, AnEventRefreshesTheListenersInTwoPhases)
{
    const TwoTubeRocket                                     design;
    ComponentSelection                                      selection(*design.tubeA);
    std::vector<std::string>                                calls;
    const ComponentSelection::PhaseSignal::ScopedConnection first{
        selection.addListener(NamedPhaseLog{.log = &calls, .name = "1"})};
    const ComponentSelection::PhaseSignal::ScopedConnection second{
        selection.addListener(NamedPhaseLog{.log = &calls, .name = "2"})};
    design.tubeB->setLength(0.6);
    EXPECT_EQ(calls, (std::vector<std::string>{"1 read", "2 read", "1 notify", "2 notify"}));

    // So does refresh(), for a change that fires no event.
    calls.clear();
    selection.refresh();
    EXPECT_EQ(calls, (std::vector<std::string>{"1 read", "2 read", "1 notify", "2 notify"}));
}

TEST_F(ComponentSelectionTest, ARocketWithoutEventsIsRefreshedByApply)
{
    const TwoTubeRocket design;
    design.rocket->enableEvents(false);
    ComponentSelection  selection(*design.tubeA);
    const PhaseRecorder phases(selection);
    selection.apply<BodyTube>(SetLength{0.35});
    EXPECT_EQ(phases.phases(), (std::vector<Phase>{Phase::READ, Phase::NOTIFY}));
}

TEST_F(ComponentSelectionTest, ATreeChangeInvalidatesTheSelection)
{
    const TwoTubeRocket                   design;
    const std::array<RocketComponent*, 1> others{design.tubeB};
    ComponentSelection                    selection(*design.tubeA, others);
    PhaseRecorder                         phases(selection);
    std::ignore = design.stage->addChild(std::make_unique<BodyTube>());
    EXPECT_FALSE(selection.isValid());
    EXPECT_EQ(phases.phases(), (std::vector<Phase>{Phase::INVALIDATED}));
    EXPECT_THROW(std::ignore = selection.getMain(), BugError);
    EXPECT_THROW(std::ignore = selection.getOthers(), BugError);
    EXPECT_THROW(std::ignore = selection.getRocket(), BugError);
    EXPECT_THROW(std::ignore = selection.isMultiEdit(), BugError);
    EXPECT_THROW(std::ignore = selection.isAllOfOneKind(), BugError);
    EXPECT_THROW(selection.apply<BodyTube>(SetLength{0.1}), BugError);

    // Later events do not reach it, and refresh() does nothing.
    phases.clear();
    design.tubeA->setLength(1.0);
    selection.refresh();
    EXPECT_TRUE(phases.phases().empty());
}

TEST_F(ComponentSelectionTest, AStageRenamedThroughTheSelectionEndsIt)
{
    // A stage's name is shown by the tree: its setName() fires a TREE change, which closes the
    // dialog in OpenRocket too (ComponentConfigDialog.componentChanged()). The selection lets go
    // of the components during the thaw and touches none afterwards.
    const TwoTubeRocket design;
    ComponentSelection  selection(*design.stage);
    selection.applyToEach(SetName{"Booster"});
    EXPECT_FALSE(selection.isValid());
    EXPECT_EQ(design.stage->getName(), "Booster");
}

TEST_F(ComponentSelectionTest, AnUndoInvalidatesTheSelectionBeforeItsComponentsAreDestroyed)
{
    // Rocket::loadFrom() replaces every component and destroys the old ones once its UNDO|TREE
    // event is delivered: the selection must not touch them afterwards (proved under ASan).
    TwoTubeRocket                         design;
    OpenRocketDocument                    document(std::move(design.owner));
    const std::array<RocketComponent*, 1> others{design.tubeB};
    const auto          selection = std::make_shared<ComponentSelection>(*design.tubeA, others);
    const PhaseRecorder phases(*selection);
    document.addUndoPosition("Modify components");
    selection->apply<BodyTube>(SetLength{0.8});
    ASSERT_TRUE(selection->isValid());
    document.undo();
    EXPECT_FALSE(selection->isValid());
    EXPECT_EQ(phases.phases().back(), Phase::INVALIDATED);
    EXPECT_THROW(std::ignore = selection->getMain(), BugError);
    selection->refresh();
}

TEST_F(ComponentSelectionTest, ReleaseEndsTheSelectionWithoutTellingItsListeners)
{
    // A closing dialog (BindingScope::release()): the selection lets go of its components at
    // once and refreshes nothing more.
    const TwoTubeRocket                   design;
    const std::array<RocketComponent*, 1> others{design.tubeB};
    ComponentSelection                    selection(*design.tubeA, others);
    const PhaseRecorder                   phases(selection);
    selection.release();
    EXPECT_FALSE(selection.isValid());
    EXPECT_TRUE(phases.phases().empty());
    EXPECT_THROW(std::ignore = selection.getMain(), BugError);
    EXPECT_THROW(selection.apply<BodyTube>(SetLength{0.1}), BugError);
    EXPECT_DOUBLE_EQ(design.tubeA->getLength(), 0.3);
    EXPECT_DOUBLE_EQ(design.tubeB->getLength(), 0.5);

    design.tubeA->setLength(1.0);
    selection.refresh();
    selection.release();
    EXPECT_TRUE(phases.phases().empty());
    EXPECT_FALSE(selection.isValid());
}

TEST_F(ComponentSelectionTest, ReleaseAfterAnInvalidatingEventChangesNothing)
{
    const TwoTubeRocket design;
    ComponentSelection  selection(*design.tubeA);
    const PhaseRecorder phases(selection);
    std::ignore = design.stage->addChild(std::make_unique<BodyTube>());
    selection.release();
    EXPECT_FALSE(selection.isValid());
    EXPECT_EQ(phases.phases(), (std::vector<Phase>{Phase::INVALIDATED}));
}

TEST_F(ComponentSelectionTest, ASelectionOutlivingItsRocketIsInvalid)
{
    std::unique_ptr<ComponentSelection> selection;
    {
        const TwoTubeRocket design;
        selection = std::make_unique<ComponentSelection>(*design.tubeA);
        EXPECT_TRUE(selection->isValid());
    }
    EXPECT_FALSE(selection->isValid());
    EXPECT_THROW(std::ignore = selection->getMain(), BugError);

    const TwoTubeRocket      design;
    const ComponentSelection reset(*design.tubeA);
    design.rocket->resetListeners();
    EXPECT_FALSE(reset.isValid());
}

TEST_F(ComponentSelectionTest, ApplyToEachUnfrozenCallsEachComponentOutsideAFreeze)
{
    // For a setter that freezes the rocket itself (RocketComponent::loadPreset()): the others
    // first and the main component last, as apply(), but each call's events are delivered at once.
    const TwoTubeRocket                   design;
    const std::array<RocketComponent*, 1> others{design.tubeB};
    ComponentSelection                    selection(*design.tubeA, others);
    const RocketEventRecorder             events(*design.rocket);
    std::vector<bool>                     frozenBefore;
    selection.applyToEachUnfrozen(
        SetLengthInOwnFreeze{.length = 0.45, .frozenBefore = &frozenBefore});
    EXPECT_EQ(frozenBefore, (std::vector<bool>{false, false}));
    EXPECT_EQ(events.sources(), (std::vector<std::string>{"B", "A"}));
    EXPECT_DOUBLE_EQ(design.tubeA->getLength(), 0.45);
    EXPECT_DOUBLE_EQ(design.tubeB->getLength(), 0.45);
    EXPECT_FALSE(design.rocket->isFrozen());

    std::vector<std::string> log;
    selection.applyToEachUnfrozen(SetEachLength{0.5});
    selection.applyToEachUnfrozen(NameAndEventCount{.events = &events, .log = &log});
    EXPECT_EQ(log, (std::vector<std::string>{"B after 4", "A after 4"}));

    // The same setter inside apply() is a second freeze.
    frozenBefore.clear();
    EXPECT_THROW(
        selection.applyToEach(SetLengthInOwnFreeze{.length = 0.6, .frozenBefore = &frozenBefore}),
        BugError);
    EXPECT_FALSE(design.rocket->isFrozen());
}

TEST_F(ComponentSelectionTest, ApplyToEachUnfrozenRefreshesAfterEveryEventOrOnceWithout)
{
    const TwoTubeRocket                   design;
    const std::array<RocketComponent*, 1> others{design.tubeB};
    ComponentSelection                    selection(*design.tubeA, others);
    PhaseRecorder                         phases(selection);
    selection.applyToEachUnfrozen(SetEachLength{0.42});
    EXPECT_EQ(phases.phases(),
              (std::vector<Phase>{Phase::READ, Phase::NOTIFY, Phase::READ, Phase::NOTIFY}));

    // No event (the lengths are set already): one refresh of its own.
    phases.clear();
    selection.applyToEachUnfrozen(SetEachLength{0.42});
    EXPECT_EQ(phases.phases(), (std::vector<Phase>{Phase::READ, Phase::NOTIFY}));
}

TEST_F(ComponentSelectionTest, ApplyToEachUnfrozenStopsWhenAnEventEndsTheSelection)
{
    // A TREE change of the first call ends the selection: the main component is not reached (it
    // could be gone; proved under ASan).
    const TwoTubeRocket                   design;
    const std::array<RocketComponent*, 1> others{design.tubeB};
    ComponentSelection                    selection(*design.tubeA, others);
    std::vector<std::string>              reached;
    selection.applyToEachUnfrozen(AddTubeToStage{&reached});
    EXPECT_EQ(reached, (std::vector<std::string>{"B"}));
    EXPECT_FALSE(selection.isValid());
    EXPECT_THROW(selection.applyToEachUnfrozen(SetEachLength{0.1}), BugError);

    // So does a listener that releases the selection on the first component's event.
    ComponentSelection                                      released(*design.tubeA, others);
    const ComponentSelection::PhaseSignal::ScopedConnection releaser{
        released.addListener(ReleaseOnNotify{&released})};
    released.applyToEachUnfrozen(SetEachLength{0.7});
    EXPECT_DOUBLE_EQ(design.tubeB->getLength(), 0.7);
    EXPECT_DOUBLE_EQ(design.tubeA->getLength(), 0.3);
    EXPECT_FALSE(released.isValid());
}

TEST_F(ComponentSelectionTest, ApplyToEachUnfrozenLetsAnExceptionThrough)
{
    const TwoTubeRocket                   design;
    const std::array<RocketComponent*, 1> others{design.tubeB};
    ComponentSelection                    selection(*design.tubeA, others);
    EXPECT_THROW(selection.applyToEachUnfrozen(SetLengthThenFailOnB{0.55}), std::runtime_error);
    EXPECT_DOUBLE_EQ(design.tubeB->getLength(), 0.55);
    EXPECT_DOUBLE_EQ(design.tubeA->getLength(), 0.3);
    EXPECT_FALSE(design.rocket->isFrozen());
    EXPECT_TRUE(selection.isValid());
}

TEST_F(ComponentSelectionTest, AListenerOfTheThawsEventMayDropTheLastOwner)
{
    // A dialog that closes on the event of the thaw drops the last owner of its selection while
    // apply() still runs: the selection keeps itself alive until apply() returns (proved under
    // ASan), for a plain change (a rocket listener after the selection's) and for a TREE change
    // (a listener of Phase::INVALIDATED).
    const TwoTubeRocket design;
    auto                owner     = std::make_shared<ComponentSelection>(*design.tubeA);
    ComponentSelection& selection = *owner;
    const ComponentChangeSignal::ScopedConnection dropper(
        design.rocket->addComponentChangeListener(DropOwner{&owner}));
    selection.apply<BodyTube>(SetLength{0.8});
    EXPECT_EQ(owner, nullptr);
    EXPECT_DOUBLE_EQ(design.tubeA->getLength(), 0.8);

    auto                stageOwner     = std::make_shared<ComponentSelection>(*design.stage);
    ComponentSelection& stageSelection = *stageOwner;
    const ComponentSelection::PhaseSignal::ScopedConnection stageDropper(
        stageSelection.addListener(DropOwnerWhenInvalidated{&stageOwner}));
    stageSelection.applyToEach(SetName{"Booster"});
    EXPECT_EQ(stageOwner, nullptr);
    EXPECT_EQ(design.stage->getName(), "Booster");
}

}  // namespace QtRocket::Gui
