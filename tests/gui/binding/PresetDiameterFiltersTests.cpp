// The diameters of the parts library's match boxes (Java: ComponentPresetChooserDialog's
// constructor and getFilterCheckboxes(), swing
// gui/dialogs/preset/ComponentPresetChooserDialog.java). OpenRocket has no JUnit test of either.
//
// Expectations marked "scouting" are OpenRocket's own dialog's, printed by the Milestone 3
// scouting's PresetChooserProbe (OpenRocket 5f164fd0e, JDK 17: the dialog made under xvfb for each
// component of the rocket below, its fields read): the columns (aftDiameterColumnIndex,
// foreDiameterColumnIndex) and the boxes' filter values.

#include "QtRocket/gui/binding/PresetDiameterFilters.h"

#include <memory>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedKey.h"

namespace QtRocket::Gui
{

namespace
{

using PresetDiameterFiltersTest = Test::GuiTest;
using Type                      = ComponentPresetType;
using Preset                    = ComponentPreset;

/// The scouting's rocket: a nose cone (base radius 25 mm), a body tube of 12.4 mm, a transition
/// with automatic radii (so 12.4 to 20.8 mm), a body tube of 20.8 mm and a flipped nose cone; an
/// inner tube, a centering ring and a parachute in the first tube.
struct ScoutingRocket
{
    std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
    NoseCone*                           nose{nullptr};
    BodyTube*                           first{nullptr};
    Transition*                         transition{nullptr};
    BodyTube*                           second{nullptr};
    NoseCone*                           tail{nullptr};

    ScoutingRocket()
    {
        RocketComponent& stage = document->getRocket().getChild(0);
        nose                   = &stage.addChild(std::make_unique<NoseCone>());
        first                  = &stage.addChild(std::make_unique<BodyTube>());
        first->setOuterRadiusAutomatic(false);
        first->setOuterRadius(0.0124);
        first->setLength(0.3);
        transition = &stage.addChild(std::make_unique<Transition>());
        second     = &stage.addChild(std::make_unique<BodyTube>());
        second->setOuterRadiusAutomatic(false);
        second->setOuterRadius(0.0208);
        second->setLength(0.3);
        tail = &stage.addChild(std::make_unique<NoseCone>());
        tail->setFlipped(true);
    }
};

/// The name of @p key, "none" for nullopt.
[[nodiscard]] std::string keyName(const std::optional<AnyTypedKey>& key)
{
    return key.has_value() ? std::string(key->getName()) : std::string("none");
}

/// The fore and the aft columns of @p type as "<fore> <aft>".
[[nodiscard]] std::string columnsOf(Type type)
{
    return keyName(PresetDiameterFilters::foreDiameterKey(type)) + " " +
           keyName(PresetDiameterFilters::aftDiameterKey(type));
}

/// Whether @p component has neither box (and is no flipped nose cone).
[[nodiscard]] bool hasNoBox(const RocketComponent& component)
{
    const PresetDiameterFilters filters = PresetDiameterFilters::forComponent(component);
    return !filters.foreDiameter.has_value() && !filters.aftDiameter.has_value() &&
           !filters.flippedNoseCone;
}

TEST_F(PresetDiameterFiltersTest, TheColumnsAreTheTypesLastOuterDiameterColumns)
{
    // scouting: aftDiameterColumnIndex 5 and foreDiameterColumnIndex 5 (Outer Diameter) for the
    // body tube, its inner tube, tube fin set, centering ring, bulkhead, engine block, tube
    // coupler, launch lug and rail button; 7 (Aft Outer Diameter) and -1 for the nose cone; 7 and
    // 6 (Fore Outer Diameter) for the transition; -1 and -1 for the streamer and the parachute.
    EXPECT_EQ(columnsOf(Type::BODY_TUBE), "OuterDiameter OuterDiameter");
    EXPECT_EQ(columnsOf(Type::TUBE_COUPLER), "OuterDiameter OuterDiameter");
    EXPECT_EQ(columnsOf(Type::BULK_HEAD), "OuterDiameter OuterDiameter");
    EXPECT_EQ(columnsOf(Type::CENTERING_RING), "OuterDiameter OuterDiameter");
    EXPECT_EQ(columnsOf(Type::ENGINE_BLOCK), "OuterDiameter OuterDiameter");
    EXPECT_EQ(columnsOf(Type::LAUNCH_LUG), "OuterDiameter OuterDiameter");
    EXPECT_EQ(columnsOf(Type::RAIL_BUTTON), "OuterDiameter OuterDiameter");
    EXPECT_EQ(columnsOf(Type::NOSE_CONE), "none AftOuterDiameter");
    EXPECT_EQ(columnsOf(Type::TRANSITION), "ForeOuterDiameter AftOuterDiameter");
    EXPECT_EQ(columnsOf(Type::STREAMER), "none none");
    EXPECT_EQ(columnsOf(Type::PARACHUTE), "none none");
    EXPECT_EQ(PresetDiameterFilters::aftDiameterKey(Type::TRANSITION),
              std::optional<AnyTypedKey>(Preset::kAftOuterDiameter));
}

TEST_F(PresetDiameterFiltersTest, TheBoxesCompareWithTheNeighboursMeetingDiameters)
{
    const ScoutingRocket rocket;
    // scouting: "rocket: nose aft R 0.025, bt1 R 0.0124, tr fore R 0.0124 aft R 0.0208, bt2 R
    // 0.0208, tail fore R 0.025 aft R 0.0".
    ASSERT_EQ(rocket.nose->getAftRadius(), 0.025);
    ASSERT_EQ(rocket.transition->getForeRadius(), 0.0124);
    ASSERT_EQ(rocket.transition->getAftRadius(), 0.0208);
    ASSERT_EQ(rocket.tail->getForeRadius(), 0.025);
    ASSERT_EQ(rocket.tail->getAftRadius(), 0.0);

    // scouting: the nose cone's aft filter value 0.0248, no fore box (no fore column).
    const PresetDiameterFilters nose = PresetDiameterFilters::forComponent(*rocket.nose);
    EXPECT_EQ(nose.foreDiameter, std::nullopt);
    EXPECT_EQ(nose.aftDiameter, std::optional<double>(0.0248));
    EXPECT_FALSE(nose.flippedNoseCone);
    // scouting: fore filter value 0.05, aft 0.0248.
    const PresetDiameterFilters first = PresetDiameterFilters::forComponent(*rocket.first);
    EXPECT_EQ(first.foreDiameter, std::optional<double>(0.05));
    EXPECT_EQ(first.aftDiameter, std::optional<double>(0.0248));
    // scouting: fore 0.0248, aft 0.0416.
    const PresetDiameterFilters transition =
        PresetDiameterFilters::forComponent(*rocket.transition);
    EXPECT_EQ(transition.foreDiameter, std::optional<double>(0.0248));
    EXPECT_EQ(transition.aftDiameter, std::optional<double>(0.0416));
    // scouting: fore 0.0416, aft 0.05 (the flipped nose cone's base, at its fore end).
    const PresetDiameterFilters second = PresetDiameterFilters::forComponent(*rocket.second);
    EXPECT_EQ(second.foreDiameter, std::optional<double>(0.0416));
    EXPECT_EQ(second.aftDiameter, std::optional<double>(0.05));
    // scouting: the flipped nose cone's "aft" box, labelled "Match fore diameter", value 0.0416
    // (the tube ahead of it: its fore and aft radii are equal); no fore box.
    const PresetDiameterFilters tail = PresetDiameterFilters::forComponent(*rocket.tail);
    EXPECT_EQ(tail.foreDiameter, std::nullopt);
    EXPECT_EQ(tail.aftDiameter, std::optional<double>(0.0416));
    EXPECT_TRUE(tail.flippedNoseCone);
}

TEST_F(PresetDiameterFiltersTest, AFlippedNoseConeComparesWithThePreviousComponentsAftDiameter)
{
    // Bug 16 (fixed): a flipped nose cone right behind a transition of 12.4 to 20.8 mm. Its base
    // meets the transition's aft end: 41.6 mm. OpenRocket compares with the transition's fore
    // radius (getPreviousSymmetricComponent().getForeRadius() * 2.0): 24.8 mm.
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    RocketComponent& stage = document->getRocket().getChild(0);
    stage.addChild(std::make_unique<NoseCone>());
    BodyTube& tube = stage.addChild(std::make_unique<BodyTube>());
    tube.setOuterRadiusAutomatic(false);
    tube.setOuterRadius(0.0124);
    Transition& transition = stage.addChild(std::make_unique<Transition>());
    transition.setAftRadiusAutomatic(false);
    transition.setAftRadius(0.0208);
    NoseCone& tail = stage.addChild(std::make_unique<NoseCone>());
    tail.setFlipped(true);
    ASSERT_EQ(transition.getForeRadius(), 0.0124);
    ASSERT_EQ(transition.getAftRadius(), 0.0208);

    const PresetDiameterFilters filters = PresetDiameterFilters::forComponent(tail);
    EXPECT_TRUE(filters.flippedNoseCone);
    EXPECT_EQ(filters.foreDiameter, std::nullopt);
    EXPECT_EQ(filters.aftDiameter, std::optional<double>(0.0416));
}

TEST_F(PresetDiameterFiltersTest, OnlySymmetricComponentsWithNeighboursHaveBoxes)
{
    const ScoutingRocket rocket;
    // scouting: "foreDiameterFilterCheckBox: none" for the inner tube, the centering ring and the
    // parachute (not symmetric components).
    const InnerTube&     inner = rocket.first->addChild(std::make_unique<InnerTube>());
    const CenteringRing& ring  = rocket.first->addChild(std::make_unique<CenteringRing>());
    const Parachute&     chute = rocket.first->addChild(std::make_unique<Parachute>());
    EXPECT_TRUE(hasNoBox(inner));
    EXPECT_TRUE(hasNoBox(ring));
    EXPECT_TRUE(hasNoBox(chute));
    // A component without presets (a fin set) has none either.
    const auto& fins = rocket.second->addChild(std::make_unique<TrapezoidFinSet>());
    EXPECT_EQ(PresetDiameterFilters::forComponent(fins).aftDiameter, std::nullopt);
    // A body tube alone in its stage has no neighbour: no box.
    const std::unique_ptr<OpenRocketDocument> alone = OpenRocketDocumentFactory::createNewRocket();
    const auto& tube = alone->getRocket().getChild(0).addChild(std::make_unique<BodyTube>());
    EXPECT_TRUE(hasNoBox(tube));
}

}  // namespace

}  // namespace QtRocket::Gui
