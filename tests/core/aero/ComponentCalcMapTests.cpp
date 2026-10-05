#include "QtRocket/aero/barrowman/ComponentCalcMap.h"

#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/BarrowmanDragCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/barrowman/ComponentAssemblyCalc.h"
#include "QtRocket/aero/barrowman/FinSetCalc.h"
#include "QtRocket/aero/barrowman/LaunchLugCalc.h"
#include "QtRocket/aero/barrowman/RailButtonCalc.h"
#include "QtRocket/aero/barrowman/RocketComponentCalc.h"
#include "QtRocket/aero/barrowman/SymmetricComponentCalc.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Uuid.h"
#include "aero/BarrowmanTestRockets.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::BarrowmanDragCalculator;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::ComponentAssemblyCalc;
using QtRocket::ComponentCalcMap;
using QtRocket::FinSetCalc;
using QtRocket::FlightConditions;
using QtRocket::FlightConfiguration;
using QtRocket::LaunchLugCalc;
using QtRocket::ModId;
using QtRocket::RailButton;
using QtRocket::RailButtonCalc;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::RocketComponentCalc;
using QtRocket::SymmetricComponentCalc;
using QtRocket::TrapezoidFinSet;
using QtRocket::Uuid;
using QtRocket::Warning;
using QtRocket::WarningSet;
using QtRocket::Test::allComponents;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::TestFalcon9Heavy;

/// The pressure CD the calculation @p calc gives at Mach 0.3 (a rail button's calculation reads
/// its button for it).
[[nodiscard]] double pressureCD(RocketComponentCalc& calc)
{
    const FlightConditions conditions;
    WarningSet             warnings;
    return calc.calculatePressureCD(conditions, BarrowmanDragCalculator::calculateStagnationCD(0.3),
                                    BarrowmanDragCalculator::calculateBaseCD(0.3), warnings);
}

/// The number of components of @p rocket the map has a calculation for.
[[nodiscard]] std::size_t countFound(ComponentCalcMap& map, Rocket& rocket)
{
    std::size_t found = 0;
    for (const RocketComponent* const component : allComponents(rocket))
    {
        if (map.get(*component) != nullptr)
        {
            found++;
        }
    }
    return found;
}

/// The geometry warnings of the calculation the map has for the fin set @p fins, each text on
/// a line ("no FinSetCalc" when it has none).
[[nodiscard]] std::string finWarnings(ComponentCalcMap& map, const RocketComponent& fins)
{
    const auto* const calc = dynamic_cast<const FinSetCalc*>(map.get(fins));
    if (calc == nullptr)
    {
        return "no FinSetCalc";
    }
    std::string texts;
    for (const Warning& warning : calc->getGeometryWarnings())
    {
        texts += warning.toString();
        texts += '\n';
    }
    return texts;
}

TEST(ComponentCalcMap, StartsUnbuiltAndEmpty)
{
    const TestEstesAlphaIII alpha;
    ComponentCalcMap        map;
    EXPECT_FALSE(map.isBuilt());
    EXPECT_EQ(map.size(), 0U);
    // Java: calcMap is null until ensureCalcMap(); here every lookup comes back empty.
    EXPECT_EQ(map.get(*alpha.body), nullptr);
    EXPECT_EQ(map.get(*alpha.rocket), nullptr);
    EXPECT_FALSE(map.isBuilt());
}

TEST(ComponentCalcMap, HoldsACalculationForEveryAerodynamicComponentAndAssembly)
{
    const TestEstesAlphaIII alpha;
    ComponentCalcMap        map;
    map.ensureBuilt(alpha.rocket->getSelectedConfiguration());

    EXPECT_TRUE(map.isBuilt());
    // the rocket, the stage, the nose cone, the body tube, the fins and the launch lug
    EXPECT_EQ(map.size(), 6U);
    EXPECT_NE(dynamic_cast<ComponentAssemblyCalc*>(map.get(*alpha.rocket)), nullptr);
    EXPECT_NE(dynamic_cast<ComponentAssemblyCalc*>(map.get(*alpha.stage)), nullptr);
    EXPECT_NE(dynamic_cast<SymmetricComponentCalc*>(map.get(*alpha.nose)), nullptr);
    EXPECT_NE(dynamic_cast<SymmetricComponentCalc*>(map.get(*alpha.body)), nullptr);
    EXPECT_NE(dynamic_cast<FinSetCalc*>(map.get(*alpha.fins)), nullptr);
    EXPECT_NE(dynamic_cast<LaunchLugCalc*>(map.get(*alpha.lug)), nullptr);

    // An internal component has none.
    EXPECT_EQ(map.get(*alpha.inner), nullptr);
    EXPECT_EQ(map.get(*alpha.block), nullptr);
    EXPECT_EQ(map.get(*alpha.chute), nullptr);
    EXPECT_EQ(map.get(*alpha.rings), nullptr);
    EXPECT_EQ(countFound(map, *alpha.rocket), 6U);

    // The same calculation every time.
    EXPECT_EQ(map.get(*alpha.fins), map.get(*alpha.fins));
}

TEST(ComponentCalcMap, CoversInactiveStagesToo)
{
    // The map is built from every component of the rocket, whatever the configuration flies.
    const TestFalcon9Heavy falcon;
    FlightConfiguration&   config = falcon.rocket->getSelectedConfiguration();
    config.setStageActive(TestFalcon9Heavy::kBoosterStageNumber, false);
    ASSERT_FALSE(config.isComponentActive(*falcon.boosterFins));

    ComponentCalcMap map;
    map.build(config);
    EXPECT_EQ(map.size(), 13U);
    EXPECT_NE(dynamic_cast<FinSetCalc*>(map.get(*falcon.boosterFins)), nullptr);
    EXPECT_NE(dynamic_cast<ComponentAssemblyCalc*>(map.get(*falcon.boosterStage)), nullptr);
}

TEST(ComponentCalcMap, EnsureBuiltBuildsOnce)
{
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    ComponentCalcMap           map;
    map.ensureBuilt(config);
    const RocketComponentCalc* const first = map.get(*alpha.fins);
    const auto* const                calc  = dynamic_cast<const FinSetCalc*>(first);
    ASSERT_NE(calc, nullptr);
    const double span = calc->getSpan();

    // The rocket changes; the map stays what it was (Java: until calcMap is set to null).
    alpha.fins->setHeight(2 * alpha.fins->getHeight());
    map.ensureBuilt(config);
    EXPECT_EQ(map.get(*alpha.fins), first);
    EXPECT_EQ(calc->getSpan(), span);

    // build() makes everything anew, from the components as they are.
    map.build(config);
    const auto* const rebuilt = dynamic_cast<const FinSetCalc*>(map.get(*alpha.fins));
    ASSERT_NE(rebuilt, nullptr);
    EXPECT_EQ(rebuilt->getSpan(), 2 * span);
    EXPECT_EQ(map.size(), 6U);
}

TEST(ComponentCalcMap, ClearDropsEverything)
{
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    ComponentCalcMap           map;
    map.ensureBuilt(config);
    ASSERT_NE(map.get(*alpha.fins), nullptr);

    map.clear();
    EXPECT_FALSE(map.isBuilt());
    EXPECT_EQ(map.size(), 0U);
    EXPECT_EQ(map.get(*alpha.fins), nullptr);

    map.ensureBuilt(config);
    EXPECT_TRUE(map.isBuilt());
    EXPECT_NE(map.get(*alpha.fins), nullptr);
}

TEST(ComponentCalcMap, AComponentTheMapWasNotBuiltWithHasNoCalculation)
{
    // Java: calcMap.get() of a component whose id the map does not hold is null.
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    ComponentCalcMap           map;
    map.ensureBuilt(config);

    const TrapezoidFinSet& added =
        alpha.body->addChild(std::make_unique<TrapezoidFinSet>(3, 0.02, 0.02, 0, 0.02));
    EXPECT_EQ(map.get(added), nullptr);
    EXPECT_EQ(map.size(), 6U);
    // The others are still served, by the calculations they had.
    EXPECT_NE(map.get(*alpha.fins), nullptr);

    // A component of another rocket, or of none.
    const TestEstesAlphaIII other;
    EXPECT_EQ(map.get(*other.fins), nullptr);
    const BodyTube alone;
    EXPECT_EQ(map.get(alone), nullptr);
    EXPECT_EQ(map.size(), 6U);

    map.build(config);
    EXPECT_NE(map.get(added), nullptr);
    EXPECT_EQ(map.size(), 7U);
}

TEST(ComponentCalcMap, AKeyOutlivesItsComponentUnused)
{
    // A component may leave its rocket and be destroyed while the map has its entry: the key
    // is only compared (AddressSanitizer is the judge).
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    const RailButton&          button = alpha.body->addChild(std::make_unique<RailButton>());
    ComponentCalcMap           map;
    map.ensureBuilt(config);
    ASSERT_NE(dynamic_cast<RailButtonCalc*>(map.get(button)), nullptr);
    EXPECT_EQ(map.size(), 7U);

    static_cast<void>(alpha.body->removeChild(&button));
    static_cast<void>(alpha.body->removeChild(alpha.fins));
    EXPECT_EQ(map.size(), 7U);
    EXPECT_NE(map.get(*alpha.body), nullptr);

    // New components, quite possibly at the addresses of the old ones, are not the old ones.
    const RailButton& newButton = alpha.body->addChild(std::make_unique<RailButton>());
    EXPECT_EQ(map.get(newButton), nullptr);
    map.build(config);
    RocketComponentCalc* const calc = map.get(newButton);
    ASSERT_NE(calc, nullptr);
    EXPECT_GT(pressureCD(*calc), 0);
    EXPECT_EQ(map.size(), 6U);
}

TEST(ComponentCalcMap, AnEntryIsOnlyForTheComponentWithItsId)
{
    // The entry at a component's address is checked against the component's id: with another
    // id there, the component is one the map does not know (Java: equals() compares the ids).
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    ComponentCalcMap           map;
    map.ensureBuilt(config);
    RocketComponentCalc* const calc = map.get(*alpha.lug);
    ASSERT_NE(calc, nullptr);

    const Uuid id = alpha.lug->getId();
    alpha.lug->setId(Uuid::random());
    EXPECT_EQ(map.get(*alpha.lug), nullptr);

    alpha.lug->setId(id);
    EXPECT_EQ(map.get(*alpha.lug), calc);
}

TEST(ComponentCalcMap, ACopyOfTheRocketWithItsIdsGetsCalculationsOfItsOwn)
{
    // Java's map, which compares components by class and id, finds the original's calculation
    // for the copy of a component. Here the map is rebuilt for the copy's objects: a rail
    // button's calculation reads its button, which may not be there any more.
    TestEstesAlphaIII alpha;
    alpha.body->addChild(std::make_unique<RailButton>());
    ComponentCalcMap map;
    map.ensureBuilt(alpha.rocket->getSelectedConfiguration());
    const std::vector<RocketComponent*> originals      = allComponents(*alpha.rocket);
    RocketComponentCalc* const          originalButton = map.get(*originals.back());
    ASSERT_NE(dynamic_cast<RailButtonCalc*>(originalButton), nullptr);
    const double cd = pressureCD(*originalButton);
    EXPECT_GT(cd, 0);

    const std::unique_ptr<Rocket>       copy   = alpha.rocket->copyRocketWithOriginalId();
    const std::vector<RocketComponent*> copies = allComponents(*copy);
    ASSERT_EQ(copies.size(), originals.size());
    ASSERT_NE(copies.back(), originals.back());
    ASSERT_EQ(copies.back()->getId(), originals.back()->getId());

    // The original goes; the copy's components are served, each by a calculation of its own.
    alpha.rocket.reset();
    RocketComponentCalc* const copiedButton = map.get(*copies.back());
    ASSERT_NE(dynamic_cast<RailButtonCalc*>(copiedButton), nullptr);
    EXPECT_EQ(pressureCD(*copiedButton), cd);
    EXPECT_EQ(countFound(map, *copy), 7U);
    EXPECT_EQ(map.size(), 7U);
    EXPECT_TRUE(map.isBuilt());
}

TEST(ComponentCalcMap, ComponentObjectsReplacedByAnUndoGetNewCalculations)
{
    // Rocket::loadFrom() replaces every component object by a copy with the same id, and may
    // leave the rocket's aerodynamic and tree modification ids as they were.
    const TestEstesAlphaIII alpha;
    alpha.body->addChild(std::make_unique<RailButton>());
    Rocket&          rocket = *alpha.rocket;
    ComponentCalcMap map;
    map.ensureBuilt(rocket.getSelectedConfiguration());
    const double cd = pressureCD(*map.get(*allComponents(rocket).back()));

    const std::unique_ptr<Rocket> saved  = rocket.copyRocketWithOriginalId();
    const ModId                   aeroId = rocket.getAerodynamicModId();
    const ModId                   treeId = rocket.getTreeModId();
    rocket.loadFrom(*saved);
    ASSERT_EQ(rocket.getAerodynamicModId(), aeroId);
    ASSERT_EQ(rocket.getTreeModId(), treeId);

    // (the fixture's pointers dangle now)
    const std::vector<RocketComponent*> components = allComponents(rocket);
    RocketComponentCalc* const          button     = map.get(*components.back());
    ASSERT_NE(dynamic_cast<RailButtonCalc*>(button), nullptr);
    EXPECT_EQ(pressureCD(*button), cd);
    EXPECT_EQ(countFound(map, rocket), 7U);
    EXPECT_EQ(map.size(), 7U);
}

TEST(ComponentCalcMap, BuildIsAllOrNothing)
{
    // A fin set whose root chord is NaN cannot be calculated (see FinSetCalc; with the rocket's
    // events enabled the fin set itself would refuse the chord): the map stays as it was.
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    ComponentCalcMap           map;
    map.ensureBuilt(config);
    RocketComponentCalc* const calc = map.get(*alpha.body);

    alpha.rocket->enableEvents(false);
    alpha.fins->setFinShape(std::numeric_limits<double>::quiet_NaN(), 0.03, 0.02, 0.05, 0.0032);
    EXPECT_THROW(map.build(config), BugError);
    EXPECT_TRUE(map.isBuilt());
    EXPECT_EQ(map.size(), 6U);
    EXPECT_EQ(map.get(*alpha.body), calc);

    // An unbuilt map stays unbuilt.
    ComponentCalcMap unbuilt;
    EXPECT_THROW(unbuilt.ensureBuilt(config), BugError);
    EXPECT_FALSE(unbuilt.isBuilt());
    EXPECT_EQ(unbuilt.size(), 0U);
}

TEST(ComponentCalcMap, ARenamedComponentGetsANewCalculation)
{
    // A FinSetCalc makes its geometry warnings at construction, with the fin set's name of
    // then in their source; OpenRocket's warning prints the current name (the probe
    // VerifyRename.java: the same calculator says "New fins" after the rename). A rename changes
    // neither the aerodynamic nor the tree modification id, so nothing voids the calculators'
    // caches: ensureBuilt() makes the calculation of the renamed component anew.
    const TestEstesAlphaIII alpha;
    alpha.fins->setThickness(0.008);
    alpha.fins->setName("Old fins");
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    ComponentCalcMap           map;
    map.ensureBuilt(config);
    RocketComponentCalc* const oldFins = map.get(*alpha.fins);
    RocketComponentCalc* const body    = map.get(*alpha.body);
    RocketComponentCalc* const lug     = map.get(*alpha.lug);
    RocketComponentCalc* const stage   = map.get(*alpha.stage);
    EXPECT_EQ(finWarnings(map, *alpha.fins),
              "Thick fins may not simulate accurately:  \"Old fins\"\n");

    const ModId aeroId = alpha.rocket->getAerodynamicModId();
    const ModId treeId = alpha.rocket->getTreeModId();
    alpha.fins->setName("New fins");
    ASSERT_EQ(alpha.rocket->getAerodynamicModId(), aeroId);
    ASSERT_EQ(alpha.rocket->getTreeModId(), treeId);

    // get() does not compare names (it is the calculators' hot path): ensureBuilt() does.
    EXPECT_EQ(map.get(*alpha.fins), oldFins);
    map.ensureBuilt(config);
    EXPECT_EQ(finWarnings(map, *alpha.fins),
              "Thick fins may not simulate accurately:  \"New fins\"\n");
    RocketComponentCalc* const newFins = map.get(*alpha.fins);
    ASSERT_NE(newFins, nullptr);
    EXPECT_EQ(map.get(*alpha.fins), newFins);
    // The other calculations are the objects they were.
    EXPECT_EQ(map.get(*alpha.body), body);
    EXPECT_EQ(map.get(*alpha.lug), lug);
    EXPECT_EQ(map.get(*alpha.stage), stage);
    EXPECT_EQ(map.size(), 6U);
    EXPECT_TRUE(map.isBuilt());

    // Without a rename every calculation stays, whatever else changes in the rocket.
    alpha.chute->setOverrideMass(0.01);
    alpha.inner->setName("Renamed motor mount");  // no calculation: nothing to make anew
    map.ensureBuilt(config);
    map.ensureBuilt(config);
    EXPECT_EQ(map.get(*alpha.fins), newFins);
    EXPECT_EQ(map.get(*alpha.body), body);
    EXPECT_EQ(map.size(), 6U);

    // A component whose warnings name nobody gets a new calculation too, and works.
    alpha.body->setName("Renamed body");
    map.ensureBuilt(config);
    EXPECT_NE(dynamic_cast<SymmetricComponentCalc*>(map.get(*alpha.body)), nullptr);
    EXPECT_EQ(map.get(*alpha.fins), newFins);
    EXPECT_EQ(countFound(map, *alpha.rocket), 6U);

    // The name cleared: the component's default name.
    alpha.fins->setName("");
    map.ensureBuilt(config);
    EXPECT_EQ(finWarnings(map, *alpha.fins),
              "Thick fins may not simulate accurately:  \"Trapezoidal Fin Set\"\n");
}

TEST(ComponentCalcMap, ARenameWithoutEventsIsSeenWhenTheyAreEnabledAgain)
{
    // With the rocket's events disabled no modification id changes, so the map does not look
    // at the names (nor do the calculators at the rocket): see the class comment.
    const TestEstesAlphaIII alpha;
    alpha.fins->setThickness(0.008);
    alpha.fins->setName("Old fins");
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    ComponentCalcMap           map;
    map.ensureBuilt(config);

    alpha.rocket->enableEvents(false);
    alpha.fins->setName("Quiet fins");
    map.ensureBuilt(config);
    EXPECT_EQ(finWarnings(map, *alpha.fins),
              "Thick fins may not simulate accurately:  \"Old fins\"\n");

    alpha.rocket->enableEvents();
    map.ensureBuilt(config);
    EXPECT_EQ(finWarnings(map, *alpha.fins),
              "Thick fins may not simulate accurately:  \"Quiet fins\"\n");
}

TEST(ComponentCalcMap, ARenamedComponentThatCannotBeCalculatedKeepsItsCalculation)
{
    // The new calculation is made first: when its constructor throws, the entry is what it was,
    // and the next ensureBuilt() tries again.
    const TestEstesAlphaIII alpha;
    alpha.fins->setName("Old fins");
    alpha.fins->setThickness(0.008);
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    ComponentCalcMap           map;
    map.ensureBuilt(config);
    RocketComponentCalc* const fins = map.get(*alpha.fins);

    // A change that renews the rocket's modification id, which the map has not seen yet; then,
    // without events, a rename and a root chord of NaN, which the fin set refuses while the
    // events are enabled and which FinSetCalc cannot calculate (see BuildIsAllOrNothing).
    const ModId builtAt = alpha.rocket->getModId();
    alpha.inner->setName("Renamed motor mount");
    ASSERT_NE(alpha.rocket->getModId(), builtAt);
    alpha.rocket->enableEvents(false);
    alpha.fins->setFinShape(std::numeric_limits<double>::quiet_NaN(), 0.03, 0.02, 0.05, 0.008);
    alpha.fins->setName("New fins");
    EXPECT_THROW(map.ensureBuilt(config), BugError);
    EXPECT_TRUE(map.isBuilt());
    EXPECT_EQ(map.size(), 6U);
    EXPECT_EQ(map.get(*alpha.fins), fins);
    EXPECT_EQ(finWarnings(map, *alpha.fins),
              "Thick fins may not simulate accurately:  \"Old fins\"\n");
    EXPECT_THROW(map.ensureBuilt(config), BugError);

    // Once the fin set can be calculated again, it is.
    alpha.fins->setFinShape(0.05, 0.03, 0.02, 0.05, 0.008);
    alpha.rocket->enableEvents();
    map.ensureBuilt(config);
    EXPECT_EQ(finWarnings(map, *alpha.fins),
              "Thick fins may not simulate accurately:  \"New fins\"\n");
    EXPECT_NE(map.get(*alpha.body), nullptr);
    EXPECT_EQ(map.size(), 6U);
}

TEST(ComponentCalcMap, MovesWithItsCalculations)
{
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    ComponentCalcMap           map;
    map.ensureBuilt(config);
    RocketComponentCalc* const calc = map.get(*alpha.fins);

    ComponentCalcMap moved{std::move(map)};
    EXPECT_TRUE(moved.isBuilt());
    EXPECT_EQ(moved.get(*alpha.fins), calc);
    EXPECT_EQ(moved.size(), 6U);

    // The source is cleared, not left built and empty (a calculator that was moved from would
    // then calculate without any component): it is built again when asked.
    // NOLINTBEGIN(bugprone-use-after-move,clang-analyzer-cplusplus.Move): moved from on purpose
    EXPECT_FALSE(map.isBuilt());
    EXPECT_EQ(map.size(), 0U);
    EXPECT_EQ(map.get(*alpha.fins), nullptr);
    map.ensureBuilt(config);
    EXPECT_TRUE(map.isBuilt());
    EXPECT_EQ(map.size(), 6U);
    EXPECT_NE(dynamic_cast<FinSetCalc*>(map.get(*alpha.fins)), nullptr);
    EXPECT_NE(map.get(*alpha.fins), calc);
    // NOLINTEND(bugprone-use-after-move,clang-analyzer-cplusplus.Move)

    // Move assignment: the target's own calculations go, the source is cleared.
    ComponentCalcMap target;
    target.ensureBuilt(config);
    target = std::move(moved);
    EXPECT_TRUE(target.isBuilt());
    EXPECT_EQ(target.get(*alpha.fins), calc);
    EXPECT_EQ(target.size(), 6U);
    // NOLINTBEGIN(bugprone-use-after-move,clang-analyzer-cplusplus.Move): moved from on purpose
    EXPECT_FALSE(moved.isBuilt());
    EXPECT_EQ(moved.size(), 0U);
    EXPECT_EQ(moved.get(*alpha.fins), nullptr);
    moved.ensureBuilt(config);
    EXPECT_EQ(moved.size(), 6U);
    // NOLINTEND(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
}

}  // namespace
