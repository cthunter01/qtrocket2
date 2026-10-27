// The model of a combo box of an enum's values (Java: EnumModel).
//
// The first five tests port EnumModelTest (swing/src/test/java/info/openrocket/swing/gui/adaptors/
// EnumModelTest.java). Its source is a plain bean; here it is BeanMode, a property that is not a
// component's, bound through a component of a rocket as every binding is. Not ported (plan O24:
// 5 of 7): stringSelectionClearsValueAndShowsNullText (QtRocket's enums have no null value and no
// dialog uses EnumModel's null text) and setSelectedItemRejectsNonEnumValues (select() takes the
// enum: C++ rejects anything else at compile time).
//
// Expectations marked "probe" are OpenRocket's own results, printed by the Java probe of this part
// (tools/openrocket-gui-pins/material-binding/probe/B2Probe.java, run on OpenRocket
// 5f164fd0e with JDK 17: EnumModel(mass, "MassComponentType", <MassComponentConfig's subset>)).

#include "QtRocket/gui/binding/EnumBinding.h"

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "BindingTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

namespace
{

using EnumBindingTest = Test::GuiTest;
using Test::ChangeCounter;
using Test::RocketEventRecorder;
using Test::TwoTubeRocket;

/// EnumModelTest.SampleMode.
enum class SampleMode
{
    ALPHA,
    BETA,
    GAMMA,
};

/// SampleMode.values().
constexpr std::array<SampleMode, 3> kAllSampleModes{SampleMode::ALPHA, SampleMode::BETA,
                                                    SampleMode::GAMMA};

/// EnumModelTest.SampleSource: a mode and the number of setMode() calls. Its setter fires no event
/// (a bean tells nobody): the selection refreshes its bindings itself after an edit that fired
/// none.
struct SampleSource
{
    SampleMode mode{SampleMode::ALPHA};
    int        setCount{0};

    /// primeMode(): changes the mode without the setter.
    void primeMode(SampleMode newMode) { mode = newMode; }

    /// The property "Mode" of the bean, on any component.
    [[nodiscard]] EnumSource<SampleMode> source()
    {
        return EnumSource<SampleMode>::of<RocketComponent>({
            .get = [this](const RocketComponent&) { return mode; },
            .set =
                [this](RocketComponent&, SampleMode newMode) {
                    mode = newMode;
                    ++setCount;
                },
        });
    }
};

/// EnumModel(source, "Mode"): every mode, on the component @p component.
[[nodiscard]] std::shared_ptr<EnumBinding<SampleMode>> sampleModel(SampleSource&    source,
                                                                   RocketComponent& component)
{
    return std::make_shared<EnumBinding<SampleMode>>(
        std::make_shared<ComponentSelection>(component), source.source(), kAllSampleModes);
}

/// The source of a mass component's type.
[[nodiscard]] EnumSource<MassComponent::MassComponentType> massComponentType()
{
    return EnumSource<MassComponent::MassComponentType>::of<MassComponent>({
        .get = [](const MassComponent& mass) { return mass.getMassComponentType(); },
        .set = [](MassComponent&                   mass,
                  MassComponent::MassComponentType type) { mass.setMassComponentType(type); },
    });
}

/// The source of a mass component's type without a setter.
[[nodiscard]] EnumSource<MassComponent::MassComponentType> massComponentTypeReadOnly()
{
    return EnumSource<MassComponent::MassComponentType>::of<MassComponent>({
        .get = [](const MassComponent& mass) { return mass.getMassComponentType(); },
    });
}

using MassType = MassComponent::MassComponentType;

/// MassComponentConfig's list of the mass component types (MassComponentConfig.java:49-57): all
/// eight, in their order.
constexpr std::span<const MassType> kMassTypeChoices = MassComponent::kAllMassComponentTypes;

/// The source of a component's axial method (Java: "AxialMethod", PlacementPanel).
[[nodiscard]] EnumSource<AxialMethod> axialMethod()
{
    return EnumSource<AxialMethod>::of<RocketComponent>({
        .get = [](const RocketComponent& component) { return component.getAxialMethod(); },
        .set = [](RocketComponent& component,
                  AxialMethod      method) { component.setAxialMethod(method); },
    });
}

/// A rocket of two tubes, with a mass component in each.
struct MassRocket
{
    TwoTubeRocket  design;
    MassComponent* massA{&design.tubeA->addChild(std::make_unique<MassComponent>())};
    MassComponent* massB{&design.tubeB->addChild(std::make_unique<MassComponent>())};
};

// ---------------------------------------------------------------------- EnumModelTest's tests

TEST_F(EnumBindingTest, SelectedItemReflectsUnderlyingValue)
{
    // Port of EnumModelTest.selectedItemReflectsUnderlyingValue.
    TwoTubeRocket design;
    SampleSource  source;
    source.primeMode(SampleMode::BETA);
    const auto model = sampleModel(source, *design.tubeA);

    EXPECT_EQ(model->value(), SampleMode::BETA);
    EXPECT_EQ(model->elementAt(1), SampleMode::BETA);
}

TEST_F(EnumBindingTest, SettingItemUpdatesSourceAndAvoidsDuplicateWrites)
{
    // Port of EnumModelTest.settingItemUpdatesSourceAndAvoidsDuplicateWrites.
    TwoTubeRocket design;
    SampleSource  source;
    const auto    model           = sampleModel(source, *design.tubeA);
    const int     initialSetCount = source.setCount;

    EXPECT_TRUE(model->select(SampleMode::GAMMA));

    EXPECT_EQ(source.mode, SampleMode::GAMMA);
    EXPECT_EQ(model->value(), SampleMode::GAMMA);

    EXPECT_FALSE(model->select(SampleMode::GAMMA));  // no-op
    EXPECT_EQ(source.setCount, initialSetCount + 1);
}

TEST_F(EnumBindingTest, RemovingElementsShrinksDisplayedValues)
{
    // Port of EnumModelTest.removingElementsShrinksDisplayedValues.
    TwoTubeRocket design;
    SampleSource  source;
    const auto    model        = sampleModel(source, *design.tubeA);
    const int     originalSize = model->size();

    model->removeElementAt(0);

    EXPECT_EQ(model->size(), originalSize - 1);
    EXPECT_EQ(model->elementAt(originalSize), std::nullopt);
}

TEST_F(EnumBindingTest, StateChangedReflectsExternalSourceUpdates)
{
    // Port of EnumModelTest.stateChangedReflectsExternalSourceUpdates. Java calls the model's
    // stateChanged() as the source's listener would; here the component the binding reads through
    // fires an event, which every binding hears (Java's EnumModel is no listener of its source:
    // the scout's decision D16).
    TwoTubeRocket design;
    SampleSource  source;
    const auto    model = sampleModel(source, *design.tubeA);

    source.primeMode(SampleMode::BETA);
    design.tubeA->fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);

    EXPECT_EQ(model->value(), SampleMode::BETA);
}

TEST_F(EnumBindingTest, RemoveElementBoundsAreGraceful)
{
    // Port of EnumModelTest.removeElementBoundsAreGraceful. Java's removeElement(null) has no
    // counterpart (an enum is never null): its stand-in is a value that is not shown, one of no
    // constant of the enum.
    TwoTubeRocket design;
    SampleSource  source;
    const auto    model        = sampleModel(source, *design.tubeA);
    const int     originalSize = model->size();

    model->removeElement(static_cast<SampleMode>(-1));
    model->removeElementAt(-1);
    model->removeElementAt(originalSize);

    EXPECT_EQ(model->size(), originalSize);
}

// ------------------------------------------------------------------------------------- the rest

TEST_F(EnumBindingTest, TheMassComponentTypesAsOpenRocket)
{
    // probe: MassComponentType size=8 selected=Mass Component values=MASSCOMPONENT ALTIMETER
    // FLIGHTCOMPUTER DEPLOYMENTCHARGE TRACKER PAYLOAD RECOVERYHARDWARE BATTERY;
    // set TRACKER: source=TRACKER.
    const MassRocket rocket;
    const auto       types = std::make_shared<EnumBinding<MassType>>(
        std::make_shared<ComponentSelection>(*rocket.massA), massComponentType(), kMassTypeChoices);
    EXPECT_EQ(types->size(), 8);
    EXPECT_EQ(types->value(), MassType::MASSCOMPONENT);
    EXPECT_EQ(
        std::vector<MassType>(types->displayedValues().begin(), types->displayedValues().end()),
        std::vector<MassType>(kMassTypeChoices.begin(), kMassTypeChoices.end()));
    EXPECT_TRUE(types->select(MassType::TRACKER));
    EXPECT_EQ(rocket.massA->getMassComponentType(), MassType::TRACKER);
    EXPECT_EQ(types->indexOf(MassType::TRACKER), 4);
}

TEST_F(EnumBindingTest, ASubsetOfTheValues)
{
    // PlacementPanel offers AxialMethod.axialOffsetMethods, every method but AFTER, which a
    // component may hold all the same (a body tube placed after its sibling).
    const MassRocket rocket;
    const auto       methods = std::make_shared<EnumBinding<AxialMethod>>(
        std::make_shared<ComponentSelection>(*rocket.design.tubeA), axialMethod(),
        kAxialOffsetMethods);
    EXPECT_EQ(methods->size(), 4);
    EXPECT_EQ(methods->getValues().size(), 4U);
    EXPECT_EQ(methods->value(), AxialMethod::AFTER);
    EXPECT_EQ(methods->indexOf(AxialMethod::AFTER), -1);
    EXPECT_EQ(methods->elementAt(1), AxialMethod::TOP);
    EXPECT_TRUE(methods->select(AxialMethod::BOTTOM));
    EXPECT_EQ(rocket.design.tubeA->getAxialMethod(), AxialMethod::BOTTOM);
    EXPECT_EQ(methods->indexOf(methods->value()), 3);
}

TEST_F(EnumBindingTest, PickingTheMainsValueLeavesTheOthersAlone)
{
    // Java's EnumModel skips the value it shows, so the main's setter, which forwards to the
    // others, is not called.
    MassRocket const rocket;
    rocket.massB->setMassComponentType(MassType::ALTIMETER);
    const std::array<RocketComponent*, 1> others{rocket.massB};
    const auto                            types = std::make_shared<EnumBinding<MassType>>(
        std::make_shared<ComponentSelection>(*rocket.massA, others), massComponentType(),
        kMassTypeChoices);
    const RocketEventRecorder events(*rocket.design.rocket);
    EXPECT_FALSE(types->select(MassType::MASSCOMPONENT));
    EXPECT_EQ(rocket.massB->getMassComponentType(), MassType::ALTIMETER);
    EXPECT_TRUE(types->select(MassType::BATTERY));
    EXPECT_EQ(rocket.massA->getMassComponentType(), MassType::BATTERY);
    EXPECT_EQ(rocket.massB->getMassComponentType(), MassType::BATTERY);
    EXPECT_EQ(events.count(), 1U);
}

TEST_F(EnumBindingTest, AHiddenValueIsShownByNoIndex)
{
    // The deployment combo of the bottom stage hides "at the separation of the lower stage";
    // a component may hold it all the same.
    const MassRocket rocket;
    const auto       types = std::make_shared<EnumBinding<MassType>>(
        std::make_shared<ComponentSelection>(*rocket.massA), massComponentType(), kMassTypeChoices);
    const ChangeCounter changes(*types);
    types->removeElement(MassType::MASSCOMPONENT);
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(types->size(), 7);
    EXPECT_EQ(types->elementAt(0), MassType::ALTIMETER);
    EXPECT_EQ(types->value(), MassType::MASSCOMPONENT);
    EXPECT_EQ(types->indexOf(types->value()), -1);
    EXPECT_EQ(types->getValues().size(), 8U);
    types->removeElement(MassType::MASSCOMPONENT);  // not shown: nothing to tell
    EXPECT_EQ(changes.count(), 1);
}

TEST_F(EnumBindingTest, TellsOfAChangeOfTheValueOnly)
{
    MassRocket const rocket;
    const auto       types = std::make_shared<EnumBinding<MassType>>(
        std::make_shared<ComponentSelection>(*rocket.massA), massComponentType(), kMassTypeChoices);
    const ChangeCounter changes(*types);
    rocket.massA->setName("Payload");
    EXPECT_EQ(changes.count(), 0);
    rocket.massA->setMassComponentType(MassType::PAYLOAD);
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(types->value(), MassType::PAYLOAD);
    EXPECT_TRUE(types->isEnabled());
}

/// What the combo box's write gave while the binding told its listeners.
struct FiringWrite
{
    std::optional<bool> selected;
};

/// Tries the combo box's write of @p binding from its change listener, into @p write.
[[nodiscard]] Signal<>::ScopedConnection tryWriteWhileFiring(EnumBinding<SampleMode>& binding,
                                                             FiringWrite&             write)
{
    return Signal<>::ScopedConnection(binding.addChangeListener(
        [&binding, &write] { write.selected = binding.select(SampleMode::ALPHA); }));
}

TEST_F(EnumBindingTest, TheComboBoxWriteIsDroppedWhileFiring)
{
    TwoTubeRocket                    design;
    SampleSource                     source;
    const auto                       model = sampleModel(source, *design.tubeA);
    FiringWrite                      write;
    const Signal<>::ScopedConnection connection = tryWriteWhileFiring(*model, write);
    EXPECT_TRUE(model->select(SampleMode::GAMMA));
    EXPECT_EQ(write.selected, false);
    EXPECT_EQ(source.mode, SampleMode::GAMMA);
    EXPECT_EQ(source.setCount, 1);
}

TEST_F(EnumBindingTest, AnUndoEndsTheBinding)
{
    // The undo destroys the components (Rocket::loadFrom()): the binding never touches them again
    // (proved under ASan).
    MassRocket         rocket;
    OpenRocketDocument document(std::move(rocket.design.owner));
    const auto         types = std::make_shared<EnumBinding<MassType>>(
        std::make_shared<ComponentSelection>(*rocket.massA), massComponentType(), kMassTypeChoices);
    const ChangeCounter changes(*types);
    document.addUndoPosition("Modify mass component");
    EXPECT_TRUE(types->select(MassType::TRACKER));
    document.undo();
    EXPECT_FALSE(types->isValid());
    EXPECT_EQ(changes.count(), 2);
    EXPECT_FALSE(types->isEnabled());
    EXPECT_EQ(types->value(), MassType::TRACKER);  // the last value read
    EXPECT_FALSE(types->select(MassType::BATTERY));
    EXPECT_EQ(changes.count(), 2);
}

// ------------------------------------------------------------------- a value of its own

TEST_F(EnumBindingTest, AValueOfItsOwn)
{
    // The flight configuration dialogs' combo boxes (Java: an EnumModel of the dialog's copy of a
    // configuration): no selection, always valid and enabled.
    const auto mode = std::make_shared<EnumBinding<SampleMode>>(SampleMode::BETA, kAllSampleModes);
    const ChangeCounter changes(*mode);
    EXPECT_EQ(mode->getSelection(), nullptr);
    EXPECT_TRUE(mode->isValid());
    EXPECT_TRUE(mode->isEnabled());
    EXPECT_EQ(mode->value(), SampleMode::BETA);
    EXPECT_EQ(mode->size(), 3);
    EXPECT_EQ(mode->indexOf(SampleMode::BETA), 1);

    // setSelectedItem(): the value changes and the listeners hear of it; the value shown already
    // changes nothing ("no duplicate writes").
    EXPECT_TRUE(mode->select(SampleMode::GAMMA));
    EXPECT_EQ(mode->value(), SampleMode::GAMMA);
    EXPECT_EQ(changes.count(), 1);
    EXPECT_FALSE(mode->select(SampleMode::GAMMA));
    EXPECT_EQ(changes.count(), 1);
}

TEST_F(EnumBindingTest, AValueOfItsOwnHidesValuesAndKeepsAHiddenOne)
{
    // DeploymentSelectionDialog on the bottom stage: removeItem(LOWER_STAGE_SEPARATION), while a
    // configuration that has it still shows it (by no index).
    const auto mode = std::make_shared<EnumBinding<SampleMode>>(SampleMode::GAMMA, kAllSampleModes);
    const ChangeCounter changes(*mode);
    mode->removeElement(SampleMode::GAMMA);
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(mode->size(), 2);
    EXPECT_EQ(mode->value(), SampleMode::GAMMA);
    EXPECT_EQ(mode->indexOf(mode->value()), -1);
    EXPECT_TRUE(mode->select(SampleMode::ALPHA));
    EXPECT_EQ(mode->indexOf(mode->value()), 0);
    EXPECT_EQ(changes.count(), 2);
}

/// Tries the combo box's write of @p binding from its change listener, into @p write, choosing
/// @p mode.
[[nodiscard]] Signal<>::ScopedConnection tryWriteWhileFiring(EnumBinding<SampleMode>& binding,
                                                             FiringWrite& write, SampleMode mode)
{
    return Signal<>::ScopedConnection(binding.addChangeListener(
        [&binding, &write, mode] { write.selected = binding.select(mode); }));
}

TEST_F(EnumBindingTest, AValueOfItsOwnDropsTheComboBoxWriteWhileFiring)
{
    const auto mode = std::make_shared<EnumBinding<SampleMode>>(SampleMode::ALPHA, kAllSampleModes);
    FiringWrite                      write;
    const Signal<>::ScopedConnection connection =
        tryWriteWhileFiring(*mode, write, SampleMode::BETA);
    EXPECT_TRUE(mode->select(SampleMode::GAMMA));
    EXPECT_EQ(write.selected, false);
    EXPECT_EQ(mode->value(), SampleMode::GAMMA);
}

TEST_F(EnumBindingTest, RefusesAMissingSelectionOrSetter)
{
    const MassRocket rocket;
    EXPECT_THROW(
        static_cast<void>(EnumBinding<MassType>(nullptr, massComponentType(), kMassTypeChoices)),
        BugError);
    EXPECT_THROW(
        static_cast<void>(EnumBinding<MassType>(std::make_shared<ComponentSelection>(*rocket.massA),
                                                massComponentTypeReadOnly(), kMassTypeChoices)),
        BugError);
    // The main component must be of the source's class.
    EXPECT_THROW(static_cast<void>(EnumBinding<MassType>(
                     std::make_shared<ComponentSelection>(*rocket.design.tubeA),
                     massComponentType(), kMassTypeChoices)),
                 BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
