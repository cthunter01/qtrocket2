#include "QtRocket/aero/barrowman/CalcFactory.h"

#include <algorithm>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/barrowman/ComponentAssemblyCalc.h"
#include "QtRocket/aero/barrowman/FinSetCalc.h"
#include "QtRocket/aero/barrowman/LaunchLugCalc.h"
#include "QtRocket/aero/barrowman/RailButtonCalc.h"
#include "QtRocket/aero/barrowman/RocketComponentCalc.h"
#include "QtRocket/aero/barrowman/SymmetricComponentCalc.h"
#include "QtRocket/aero/barrowman/TubeFinSetCalc.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Bulkhead.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/EllipticalFinSet.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/Streamer.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/TubeCoupler.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BugError.h"
#include "aero/BarrowmanTestRockets.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::ComponentAssemblyCalc;
using QtRocket::ComponentKind;
using QtRocket::FinSetCalc;
using QtRocket::LaunchLugCalc;
using QtRocket::RailButtonCalc;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::RocketComponentCalc;
using QtRocket::SymmetricComponentCalc;
using QtRocket::TrapezoidFinSet;
using QtRocket::TubeFinSetCalc;
using QtRocket::Test::allComponents;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::TestFalcon9Heavy;
namespace CalcFactory = QtRocket::CalcFactory;

/// The calculation classes of aerodynamics.barrowman, and none; and what the factory must
/// never give: a null pointer, and a calculation of another class.
enum class Calculation
{
    SYMMETRIC_COMPONENT,
    FIN_SET,
    TUBE_FIN_SET,
    LAUNCH_LUG,
    RAIL_BUTTON,
    COMPONENT_ASSEMBLY,
    NONE,          ///< Java: a BugException; here create() throws BugError
    NULL_POINTER,  ///< create() returned nullptr
    OTHER_CLASS,   ///< create() returned a calculation of none of the six classes
};

[[nodiscard]] std::string_view name(Calculation calculation)
{
    switch (calculation)
    {
        case Calculation::SYMMETRIC_COMPONENT:
            return "SymmetricComponentCalc";
        case Calculation::FIN_SET:
            return "FinSetCalc";
        case Calculation::TUBE_FIN_SET:
            return "TubeFinSetCalc";
        case Calculation::LAUNCH_LUG:
            return "LaunchLugCalc";
        case Calculation::RAIL_BUTTON:
            return "RailButtonCalc";
        case Calculation::COMPONENT_ASSEMBLY:
            return "ComponentAssemblyCalc";
        case Calculation::NONE:
            return "none (a BugError)";
        case Calculation::NULL_POINTER:
            return "a null pointer";
        case Calculation::OTHER_CLASS:
            return "a calculation of another class";
    }
    return "?";
}

/// The class Reflection.construct("...aerodynamics.barrowman", component, "Calc", component)
/// finds for a component of @p kind in OpenRocket: the class named after the component's class
/// or, failing that, after the nearest of its superclasses (NoseCone -> Transition ->
/// SymmetricComponent: SymmetricComponentCalc; TrapezoidFinSet -> FinSet: FinSetCalc;
/// ParallelStage -> AxialStage -> ComponentAssembly: ComponentAssemblyCalc; ...). For an
/// internal component the search ends at RocketComponentCalc, which is abstract: a
/// BugException ("Construction of ...RocketComponentCalc failed").
///
/// No default: a kind added to ComponentKind must be given its place here, as in the factory.
[[nodiscard]] Calculation javaCalculation(ComponentKind kind)
{
    switch (kind)
    {
        case ComponentKind::ROCKET:
        case ComponentKind::AXIAL_STAGE:
        case ComponentKind::PARALLEL_STAGE:
        case ComponentKind::POD_SET:
            return Calculation::COMPONENT_ASSEMBLY;
        case ComponentKind::BODY_TUBE:
        case ComponentKind::TRANSITION:
        case ComponentKind::NOSE_CONE:
            return Calculation::SYMMETRIC_COMPONENT;
        case ComponentKind::TRAPEZOID_FIN_SET:
        case ComponentKind::ELLIPTICAL_FIN_SET:
        case ComponentKind::FREEFORM_FIN_SET:
            return Calculation::FIN_SET;
        case ComponentKind::TUBE_FIN_SET:
            return Calculation::TUBE_FIN_SET;
        case ComponentKind::LAUNCH_LUG:
            return Calculation::LAUNCH_LUG;
        case ComponentKind::RAIL_BUTTON:
            return Calculation::RAIL_BUTTON;
        case ComponentKind::INNER_TUBE:
        case ComponentKind::TUBE_COUPLER:
        case ComponentKind::ENGINE_BLOCK:
        case ComponentKind::CENTERING_RING:
        case ComponentKind::BULKHEAD:
        case ComponentKind::MASS_COMPONENT:
        case ComponentKind::SHOCK_CORD:
        case ComponentKind::PARACHUTE:
        case ComponentKind::STREAMER:
            return Calculation::NONE;
    }
    return Calculation::NONE;
}

/// The class of @p calc.
[[nodiscard]] Calculation calculationOf(const RocketComponentCalc& calc)
{
    if (dynamic_cast<const SymmetricComponentCalc*>(&calc) != nullptr)
    {
        return Calculation::SYMMETRIC_COMPONENT;
    }
    if (dynamic_cast<const FinSetCalc*>(&calc) != nullptr)
    {
        return Calculation::FIN_SET;
    }
    if (dynamic_cast<const TubeFinSetCalc*>(&calc) != nullptr)
    {
        return Calculation::TUBE_FIN_SET;
    }
    if (dynamic_cast<const LaunchLugCalc*>(&calc) != nullptr)
    {
        return Calculation::LAUNCH_LUG;
    }
    if (dynamic_cast<const RailButtonCalc*>(&calc) != nullptr)
    {
        return Calculation::RAIL_BUTTON;
    }
    if (dynamic_cast<const ComponentAssemblyCalc*>(&calc) != nullptr)
    {
        return Calculation::COMPONENT_ASSEMBLY;
    }
    return Calculation::OTHER_CLASS;
}

/// What CalcFactory::create() makes of @p component: the class of the calculation, or NONE
/// when it throws BugError, and only then (a null pointer is NULL_POINTER, which nothing
/// expects).
[[nodiscard]] Calculation created(const RocketComponent& component)
{
    try
    {
        const std::unique_ptr<RocketComponentCalc> calc = CalcFactory::create(component);
        return calc == nullptr ? Calculation::NULL_POINTER : calculationOf(*calc);
    }
    catch (const BugError&)
    {
        return Calculation::NONE;
    }
}

/// A rocket that holds a component of every ComponentKind: a stage with a nose cone, a body
/// tube and a transition; on the body tube the three planar fin sets (apart from each other
/// along the tube), tube fins, a launch lug, a rail button, a booster set and a pod set (each
/// with a body tube), and in it every internal component.
struct EveryKindRocket
{
    Rocket rocket;

    EveryKindRocket()
    {
        AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
        stage.addChild(std::make_unique<QtRocket::NoseCone>());
        BodyTube& body = stage.addChild(std::make_unique<BodyTube>(1.0, 0.05, 0.002));
        stage.addChild(std::make_unique<QtRocket::Transition>());

        auto trapezoid = std::make_unique<TrapezoidFinSet>(3, 0.05, 0.03, 0.01, 0.04);
        trapezoid->setAxialMethod(AxialMethod::TOP);
        trapezoid->setAxialOffset(0.1);
        body.addChild(std::move(trapezoid));
        auto elliptical = std::make_unique<QtRocket::EllipticalFinSet>();
        elliptical->setAxialMethod(AxialMethod::TOP);
        elliptical->setAxialOffset(0.4);
        body.addChild(std::move(elliptical));
        auto freeform = std::make_unique<QtRocket::FreeformFinSet>();
        freeform->setAxialMethod(AxialMethod::TOP);
        freeform->setAxialOffset(0.7);
        body.addChild(std::move(freeform));
        body.addChild(std::make_unique<QtRocket::TubeFinSet>());
        body.addChild(std::make_unique<QtRocket::LaunchLug>());
        body.addChild(std::make_unique<QtRocket::RailButton>());

        auto& boosters = body.addChild(std::make_unique<QtRocket::ParallelStage>());
        boosters.addChild(std::make_unique<BodyTube>(0.3, 0.02, 0.001));
        auto& pods = body.addChild(std::make_unique<QtRocket::PodSet>());
        pods.addChild(std::make_unique<BodyTube>(0.2, 0.01, 0.001));

        auto& inner = body.addChild(std::make_unique<QtRocket::InnerTube>());
        inner.addChild(std::make_unique<QtRocket::EngineBlock>());
        body.addChild(std::make_unique<QtRocket::TubeCoupler>());
        body.addChild(std::make_unique<QtRocket::CenteringRing>());
        body.addChild(std::make_unique<QtRocket::Bulkhead>());
        body.addChild(std::make_unique<QtRocket::MassComponent>());
        body.addChild(std::make_unique<QtRocket::ShockCord>());
        body.addChild(std::make_unique<QtRocket::Parachute>());
        body.addChild(std::make_unique<QtRocket::Streamer>());

        rocket.enableEvents();
    }
};

/// The kinds of the components of @p rocket, each once.
[[nodiscard]] std::vector<ComponentKind> kindsIn(Rocket& rocket)
{
    std::vector<ComponentKind> kinds;
    for (const RocketComponent* const component : allComponents(rocket))
    {
        if (std::ranges::find(kinds, component->kind()) == kinds.end())
        {
            kinds.push_back(component->kind());
        }
    }
    return kinds;
}

/// The components of @p rocket for which create() does not give OpenRocket's calculation (for
/// a component without one: does not throw BugError), or for which hasCalculation() does not
/// answer the calculators' condition.
[[nodiscard]] std::string factoryDifferences(Rocket& rocket)
{
    std::string text;
    for (const RocketComponent* const component : allComponents(rocket))
    {
        const ComponentKind kind     = component->kind();
        const Calculation   expected = javaCalculation(kind);
        const Calculation   actual   = created(*component);
        if (actual != expected)
        {
            text += std::format("{} ({}): {} instead of {}\n", component->getName(),
                                QtRocket::componentKindName(kind), name(actual), name(expected));
        }
        // Java: comp.isAerodynamic() || comp instanceof ComponentAssembly
        const bool wanted = component->isAerodynamic() || QtRocket::isAssembly(kind);
        if (CalcFactory::hasCalculation(kind) != wanted ||
            wanted != (expected != Calculation::NONE))
        {
            text +=
                std::format("{} ({}): hasCalculation() is {}\n", component->getName(),
                            QtRocket::componentKindName(kind), CalcFactory::hasCalculation(kind));
        }
    }
    return text;
}

TEST(CalcFactory, EveryKindHasOpenRocketsCalculationOrNone)
{
    EveryKindRocket everyKind;

    // The rocket holds every kind there is: a kind added to ComponentKind must be added to it
    // (and to javaCalculation() above, and to the factory's switch).
    const std::vector<ComponentKind> kinds = kindsIn(everyKind.rocket);
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        EXPECT_TRUE(std::ranges::find(kinds, kind) != kinds.end())
            << QtRocket::componentKindName(kind);
    }
    EXPECT_EQ(kinds.size(), QtRocket::kAllComponentKinds.size());

    EXPECT_EQ(factoryDifferences(everyKind.rocket), "");
}

/// Whether CalcFactory::create() of @p component throws BugError.
[[nodiscard]] bool createIsABug(const RocketComponent& component)
{
    try
    {
        static_cast<void>(CalcFactory::create(component));
    }
    catch (const BugError&)
    {
        return true;
    }
    return false;
}

/// The components of @p rocket whose kind has no calculation in OpenRocket and for which
/// create() does not throw BugError, each name on a line; @p count is set to the number of
/// components of such a kind.
[[nodiscard]] std::string internalComponentsThatDoNotThrow(Rocket& rocket, int& count)
{
    std::string names;
    count = 0;
    for (const RocketComponent* const component : allComponents(rocket))
    {
        if (javaCalculation(component->kind()) != Calculation::NONE)
        {
            continue;
        }
        count++;
        if (!createIsABug(*component))
        {
            names += std::format("{} ({})\n", component->getName(),
                                 QtRocket::componentKindName(component->kind()));
        }
    }
    return names;
}

TEST(CalcFactory, EveryKindWithoutACalculationIsABug)
{
    // Java: Reflection.construct() ends at the abstract RocketComponentCalc and throws a
    // BugException. Here create() throws BugError for each of the nine internal kinds; it never
    // answers with a null pointer.
    EveryKindRocket everyKind;
    int             internalComponents = 0;
    EXPECT_EQ(internalComponentsThatDoNotThrow(everyKind.rocket, internalComponents), "");
    EXPECT_EQ(internalComponents, 9);
}

TEST(CalcFactory, HasCalculationForExternalComponentsAndAssembliesOnly)
{
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        EXPECT_EQ(CalcFactory::hasCalculation(kind),
                  QtRocket::isExternal(kind) || QtRocket::isAssembly(kind))
            << QtRocket::componentKindName(kind);
        EXPECT_EQ(CalcFactory::hasCalculation(kind), javaCalculation(kind) != Calculation::NONE)
            << QtRocket::componentKindName(kind);
        EXPECT_NE(CalcFactory::hasCalculation(kind), QtRocket::isInternal(kind))
            << QtRocket::componentKindName(kind);
    }
}

TEST(CalcFactory, TheTestRocketsGetOpenRocketsCalculations)
{
    // What Reflection.construct() gives for every component of TestRockets.makeFalcon9Heavy()
    // (the probe Explore.java) and the Estes Alpha III.
    const TestFalcon9Heavy falcon;
    EXPECT_EQ(factoryDifferences(*falcon.rocket), "");
    EXPECT_EQ(created(*falcon.rocket), Calculation::COMPONENT_ASSEMBLY);
    EXPECT_EQ(created(*falcon.payloadStage), Calculation::COMPONENT_ASSEMBLY);
    EXPECT_EQ(created(*falcon.boosterStage), Calculation::COMPONENT_ASSEMBLY);
    EXPECT_EQ(created(*falcon.payloadNose), Calculation::SYMMETRIC_COMPONENT);
    EXPECT_EQ(created(*falcon.payloadBody), Calculation::SYMMETRIC_COMPONENT);
    EXPECT_EQ(created(*falcon.payloadTransition), Calculation::SYMMETRIC_COMPONENT);
    EXPECT_EQ(created(*falcon.boosterFins), Calculation::FIN_SET);
    EXPECT_EQ(created(*falcon.parachute), Calculation::NONE);
    EXPECT_EQ(created(*falcon.shockCord), Calculation::NONE);
    EXPECT_EQ(created(*falcon.boosterMotorTubes), Calculation::NONE);

    const TestEstesAlphaIII alpha;
    EXPECT_EQ(factoryDifferences(*alpha.rocket), "");
    EXPECT_EQ(created(*alpha.lug), Calculation::LAUNCH_LUG);
}

TEST(CalcFactory, AComponentWithoutACalculationIsABug)
{
    const TestEstesAlphaIII alpha;
    EXPECT_THROW(static_cast<void>(CalcFactory::create(*alpha.inner)), BugError);
    EXPECT_THROW(static_cast<void>(CalcFactory::create(*alpha.block)), BugError);
    EXPECT_THROW(static_cast<void>(CalcFactory::create(*alpha.chute)), BugError);
    EXPECT_THROW(static_cast<void>(CalcFactory::create(*alpha.rings)), BugError);
}

TEST(CalcFactory, EveryCallMakesANewCalculationOfTheComponentAsItIs)
{
    const TestEstesAlphaIII                    alpha;
    const std::unique_ptr<RocketComponentCalc> first  = CalcFactory::create(*alpha.fins);
    const std::unique_ptr<RocketComponentCalc> second = CalcFactory::create(*alpha.fins);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first.get(), second.get());

    const auto* const before = dynamic_cast<const FinSetCalc*>(first.get());
    ASSERT_NE(before, nullptr);
    EXPECT_EQ(before->getSpan(), alpha.fins->getSpan());

    // A calculation copies the fin set's geometry: the first one keeps the old span.
    alpha.fins->setHeight(2 * alpha.fins->getHeight());
    const std::unique_ptr<RocketComponentCalc> third = CalcFactory::create(*alpha.fins);
    const auto* const                          after = dynamic_cast<const FinSetCalc*>(third.get());
    ASSERT_NE(after, nullptr);
    EXPECT_EQ(after->getSpan(), alpha.fins->getSpan());
    EXPECT_EQ(after->getSpan(), 2 * before->getSpan());
}

TEST(CalcFactory, TheCalculationsOwnConstructorDecidesWhatIsABug)
{
    // A fin set without a parent cannot be calculated (see FinSetCalc); an assembly or a body
    // tube on its own can.
    const TrapezoidFinSet fins;
    EXPECT_THROW(static_cast<void>(CalcFactory::create(fins)), BugError);

    const AxialStage stage;
    EXPECT_EQ(created(stage), Calculation::COMPONENT_ASSEMBLY);
    const BodyTube tube;
    EXPECT_EQ(created(tube), Calculation::SYMMETRIC_COMPONENT);
}

}  // namespace
