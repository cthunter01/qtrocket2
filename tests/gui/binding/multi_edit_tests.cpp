// Editing several selected components at once through one binding: the user's decision U3 of
// 2026-10-17 (each component its own setter call, the others first and the main component last,
// one freeze, one undo step), where OpenRocket's config listeners give the others values derived
// from the main component.
//
// The expectations are QtRocket's results of the design (the bindings scout's
// multiedit_main.cpp on the QtRocket core, handoff/m2-scout/evidence/m2-scout-bindings/
// multiedit-cpp.out); the comments give OpenRocket's results for the same edits (its
// MultiEditProbe.java, java/multiedit-probe.out), which differ on purpose.

#include <array>
#include <cstddef>
#include <memory>
#include <numbers>
#include <utility>

#include <gtest/gtest.h>

#include "BindingTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

using MultiEdit = Test::GuiTest;
using Test::bodyTubeInnerRadius;
using Test::bodyTubeLength;
using Test::RocketEventRecorder;
using Test::TwoTubeRocket;

[[nodiscard]] const UnitGroup& lengthUnits()
{
    return unitGroup(UnitGroupId::LENGTH);
}

/// The selection of the main component @p main with the others @p others.
template <std::size_t N>
[[nodiscard]] std::shared_ptr<ComponentSelection> selectionOf(
    RocketComponent& main, const std::array<RocketComponent*, N>& others)
{
    return std::make_shared<ComponentSelection>(main, others);
}

/// The override mass of any component.
[[nodiscard]] DoubleSource overrideMass()
{
    return DoubleSource::of<RocketComponent>({
        .get = [](const RocketComponent& component) { return component.getOverrideMass(); },
        .set = [](RocketComponent& component, double mass) { component.setOverrideMass(mass); },
    });
}

/// The wall thickness of a symmetric component.
[[nodiscard]] DoubleSource thickness()
{
    return DoubleSource::of<SymmetricComponent>({
        .get = [](const SymmetricComponent& component) { return component.getThickness(); },
        .set = [](SymmetricComponent& component, double wall) { component.setThickness(wall); },
    });
}

}  // namespace

TEST_F(MultiEdit, EachBodyTubeDerivesItsWallFromItsOwnOuterRadius)
{
    // BodyTube.setInnerRadius(r) is setThickness(outer radius - r). OpenRocket forwards the
    // main's thickness: B (outer 0.03 m) gets an inner radius of 0.024999999999999998.
    const TwoTubeRocket design;
    RocketEventRecorder events(*design.rocket);
    DoubleBinding inner(selectionOf(*design.tubeA, std::array<RocketComponent*, 1>{design.tubeB}),
                        bodyTubeInnerRadius(), lengthUnits(), {.multiplier = 2});
    inner.setValue(0.03);
    EXPECT_EQ(design.tubeA->getInnerRadius(), 0.015);
    EXPECT_EQ(design.tubeA->getThickness(), 0.02 - 0.015);
    EXPECT_EQ(design.tubeB->getInnerRadius(), 0.015);
    EXPECT_EQ(design.tubeB->getThickness(), 0.03 - 0.015);
    // One merged event, whose source is the main component (applied last).
    ASSERT_EQ(events.count(), 1U);
    EXPECT_EQ(events.sources().front(), "A");
    // The field shows the main component's value.
    EXPECT_EQ(inner.value(), 0.03);
}

TEST_F(MultiEdit, OneEventFiresAlsoWhenOnlyTheOthersChange)
{
    // OpenRocket: setLength(0.3) with A at 0.3 already fires no event (modID unchanged), although
    // B changes from 0.5.
    const TwoTubeRocket design;
    RocketEventRecorder events(*design.rocket);
    DoubleBinding length(selectionOf(*design.tubeA, std::array<RocketComponent*, 1>{design.tubeB}),
                         bodyTubeLength(), lengthUnits(), {});
    length.setValue(0.3);
    EXPECT_EQ(design.tubeA->getLength(), 0.3);
    EXPECT_EQ(design.tubeB->getLength(), 0.3);
    ASSERT_EQ(events.count(), 1U);
    EXPECT_EQ(events.sources().front(), "B");
}

TEST_F(MultiEdit, EachMassComponentGetsTheMassOfItsOwnVolume)
{
    // MassComponent.setDensity(d) is setComponentMass(d * volume). OpenRocket forwards the main's
    // mass: m2 gets 0.015707963267948967 kg and shows a density of 125.
    const TwoTubeRocket design;
    auto&               m1 = design.tubeA->addChild(std::make_unique<MassComponent>());
    auto&               m2 = design.tubeB->addChild(std::make_unique<MassComponent>());
    m1.setLength(0.05);
    m1.setRadiusAutomatic(false);
    m1.setRadius(0.01);
    m2.setLength(0.1);
    m2.setRadiusAutomatic(false);
    m2.setRadius(0.02);
    RocketEventRecorder events(*design.rocket);
    const auto          density = DoubleSource::of<MassComponent>({
        .get = [](const MassComponent& mass) { return mass.getDensity(); },
        .set = [](MassComponent& mass, double value) { mass.setDensity(value); },
    });
    DoubleBinding       binding(selectionOf(m1, std::array<RocketComponent*, 1>{&m2}), density,
                                unitGroup(UnitGroupId::DENSITY_BULK), {});
    binding.setValue(1000);
    EXPECT_DOUBLE_EQ(m1.getComponentMass(), 0.015707963267948967);
    EXPECT_DOUBLE_EQ(m2.getComponentMass(), 0.12566370614359174);
    EXPECT_DOUBLE_EQ(m1.getDensity(), 1000);
    EXPECT_DOUBLE_EQ(m2.getDensity(), 1000);
    EXPECT_EQ(events.count(), 1U);
}

TEST_F(MultiEdit, EachFinSetGetsTheSweepAngleTyped)
{
    // TrapezoidFinSet.setSweepAngle(a) is setSweep(tan(a) * height). OpenRocket forwards the
    // main's sweep: the fin set twice as high gets 16.102113751986014 degrees for 30 typed.
    const TwoTubeRocket design;
    auto&               f1 = design.tubeA->addChild(std::make_unique<TrapezoidFinSet>());
    auto&               f2 = design.tubeB->addChild(std::make_unique<TrapezoidFinSet>());
    f1.setHeight(0.05);
    f2.setHeight(0.10);
    const auto    sweepAngle = DoubleSource::of<TrapezoidFinSet>({
        .get = [](const TrapezoidFinSet& fins) { return fins.getSweepAngle(); },
        .set = [](TrapezoidFinSet& fins, double angle) { fins.setSweepAngle(angle); },
    });
    DoubleBinding binding(selectionOf(f1, std::array<RocketComponent*, 1>{&f2}), sweepAngle,
                          unitGroup(UnitGroupId::ANGLE), {});
    EXPECT_TRUE(binding.commitText("30"));  // degrees
    constexpr double kTolerance = 1e-12;
    EXPECT_NEAR(f1.getSweep(), 0.028867513459481287, kTolerance);
    EXPECT_NEAR(f2.getSweep(), 0.057735026918962574, kTolerance);
    EXPECT_NEAR(f1.getSweepAngle(), std::numbers::pi / 6, kTolerance);
    EXPECT_NEAR(f2.getSweepAngle(), std::numbers::pi / 6, kTolerance);
}

TEST_F(MultiEdit, ASetterOfOneClassReachesOnlyTheSelectedComponentsOfThatClass)
{
    // A nose cone and two body tubes: the generic dialog of mixed kinds edits what they share.
    const TwoTubeRocket design;
    design.nose->setLength(0.1);
    const auto selection =
        selectionOf(*design.tubeA, std::array<RocketComponent*, 2>{design.nose, design.tubeB});
    EXPECT_FALSE(selection->isAllOfOneKind());

    DoubleBinding length(selection, bodyTubeLength(), lengthUnits(), {});
    length.setValue(0.45);
    EXPECT_EQ(design.tubeA->getLength(), 0.45);
    EXPECT_EQ(design.tubeB->getLength(), 0.45);
    EXPECT_EQ(design.nose->getLength(), 0.1);

    DoubleBinding wall(selection, thickness(), lengthUnits(), {});
    wall.setValue(0.0005);
    EXPECT_EQ(design.nose->getThickness(), 0.0005);
    EXPECT_EQ(design.tubeA->getThickness(), 0.0005);
    EXPECT_EQ(design.tubeB->getThickness(), 0.0005);

    // getOverrideMass() is the component's own mass until the mass is overridden.
    design.nose->setMassOverridden(true);
    design.tubeA->setMassOverridden(true);
    design.tubeB->setMassOverridden(true);
    DoubleBinding mass(selection, overrideMass(), unitGroup(UnitGroupId::MASS), {});
    mass.setValue(0.1);
    EXPECT_EQ(design.nose->getOverrideMass(), 0.1);
    EXPECT_EQ(design.tubeA->getOverrideMass(), 0.1);
    EXPECT_EQ(design.tubeB->getOverrideMass(), 0.1);
}

TEST_F(MultiEdit, InvalidTextSetsNoComponent)
{
    // OpenRocket sets the main's last value again, which reaches the others: B's length becomes
    // A's (InvalidTextProbe.java: B 0.5 -> 0.3). U3: nothing is set.
    const TwoTubeRocket design;
    RocketEventRecorder events(*design.rocket);
    DoubleBinding length(selectionOf(*design.tubeA, std::array<RocketComponent*, 1>{design.tubeB}),
                         bodyTubeLength(), lengthUnits(), {});
    EXPECT_FALSE(length.commitText("abc"));
    EXPECT_EQ(design.tubeA->getLength(), 0.3);
    EXPECT_EQ(design.tubeB->getLength(), 0.5);
    EXPECT_EQ(events.count(), 0U);
}

TEST_F(MultiEdit, AnEditInsideAnOuterFreezeTakesNoSecondOne)
{
    const TwoTubeRocket design;
    RocketEventRecorder events(*design.rocket);
    DoubleBinding length(selectionOf(*design.tubeA, std::array<RocketComponent*, 1>{design.tubeB}),
                         bodyTubeLength(), lengthUnits(), {});
    design.rocket->freeze();
    EXPECT_NO_THROW(length.setValue(0.4));
    // The binding refreshed itself: it shows the new value before the events come.
    EXPECT_EQ(length.value(), 0.4);
    EXPECT_EQ(events.count(), 0U);
    design.rocket->thaw();
    EXPECT_EQ(events.count(), 1U);
    EXPECT_EQ(design.tubeB->getLength(), 0.4);
}

TEST_F(MultiEdit, TheEditOfEveryComponentIsOneUndoStep)
{
    // The dialog adds one undo position per session ("Modify components" with several selected:
    // ComponentCfgDlg.ModifyComponents); the bindings add none.
    TwoTubeRocket      design;
    OpenRocketDocument document(std::move(design.owner));
    const Uuid         idA      = design.tubeA->getId();
    const Uuid         idB      = design.tubeB->getId();
    const Uuid         idNose   = design.nose->getId();
    const double       noseWall = design.nose->getThickness();
    const auto         selection =
        selectionOf(*design.tubeA, std::array<RocketComponent*, 2>{design.tubeB, design.nose});
    DoubleBinding wall(selection, thickness(), lengthUnits(), {});
    DoubleBinding length(selection, bodyTubeLength(), lengthUnits(), {});

    document.addUndoPosition("Modify components");
    wall.setValue(0.0007);
    length.setValue(0.9);
    EXPECT_EQ(design.nose->getThickness(), 0.0007);
    EXPECT_EQ(document.getUndoDescription(), "Modify components");

    document.undo();
    EXPECT_FALSE(selection->isValid());
    EXPECT_FALSE(document.isUndoAvailable());
    Rocket&     rocket = document.getRocket();
    const auto* a      = dynamic_cast<const BodyTube*>(rocket.findComponent(idA));
    const auto* b      = dynamic_cast<const BodyTube*>(rocket.findComponent(idB));
    const auto* nose   = dynamic_cast<const NoseCone*>(rocket.findComponent(idNose));
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(nose, nullptr);
    EXPECT_EQ(a->getThickness(), 0.001);
    EXPECT_EQ(b->getThickness(), 0.002);
    EXPECT_EQ(nose->getThickness(), noseWall);
    EXPECT_EQ(a->getLength(), 0.3);
    EXPECT_EQ(b->getLength(), 0.5);
}

}  // namespace QtRocket::Gui
