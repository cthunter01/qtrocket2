#include "QtRocket/simulation/extension/SimulationExtensionRegistry.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtensionProvider.h"
#include "QtRocket/simulation/extension/UnknownSimulationExtension.h"
#include "QtRocket/simulation/extension/example/AirStart.h"
#include "QtRocket/simulation/extension/example/AirStartProvider.h"
#include "QtRocket/simulation/extension/example/RollControl.h"
#include "QtRocket/simulation/extension/example/RollControlProvider.h"
#include "QtRocket/simulation/extension/impl/JavaCode.h"
#include "QtRocket/simulation/extension/impl/ScriptingExtension.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Strings.h"

namespace
{

using QtRocket::AirStart;
using QtRocket::JavaCode;
using QtRocket::RollControl;
using QtRocket::ScriptingExtension;
using QtRocket::SimulationExtension;
using QtRocket::SimulationExtensionProvider;
using QtRocket::SimulationExtensionRegistry;
using QtRocket::UnknownSimulationExtension;

// The registry has no counterpart in OpenRocket, which asks its injector for the set of
// providers. What is OpenRocket's is the loop that picks the provider of an id (importt/
// SingleSimulationHandler.closeElement()), and the ids and menu names of the four providers
// QtRocket ships (probes/tier9a-extensions/java: ExtPins2.java, the "provider" lines).

// The providers are owned: the registry moves and does not copy.
static_assert(!std::is_copy_constructible_v<SimulationExtensionRegistry>);
static_assert(!std::is_copy_assignable_v<SimulationExtensionRegistry>);
static_assert(std::is_nothrow_move_constructible_v<SimulationExtensionRegistry>);
static_assert(std::is_nothrow_move_assignable_v<SimulationExtensionRegistry>);

/// What a counting provider did.
struct Calls
{
    std::vector<std::string> asked;  ///< the ids getInstance() was called with
};

/// A provider of the ids @p ids whose extensions are UnknownSimulationExtensions with the id
/// @p made (a holder that says who made it), or nothing when @p made is empty. It notes its
/// getInstance() calls in @p calls.
class CountingProvider final : public SimulationExtensionProvider
{
public:
    CountingProvider(std::vector<std::string> ids, std::string made, std::shared_ptr<Calls> calls)
      : m_ids(std::move(ids)), m_made(std::move(made)), m_calls(std::move(calls))
    {
    }

    [[nodiscard]] std::vector<std::string> getIds() const override { return m_ids; }

    [[nodiscard]] std::optional<std::vector<std::string>> getName(
        std::string_view /*id*/) const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::unique_ptr<SimulationExtension> getInstance(
        std::string_view id) const override
    {
        m_calls->asked.emplace_back(id);
        if (m_made.empty())
        {
            return nullptr;
        }
        return std::make_unique<UnknownSimulationExtension>(m_made);
    }

private:
    std::vector<std::string> m_ids;
    std::string              m_made;
    std::shared_ptr<Calls>   m_calls;
};

/// The id of the extension @p registry makes for @p id; "<none>" when it makes none.
[[nodiscard]] std::string madeFor(const SimulationExtensionRegistry& registry, std::string_view id)
{
    const std::unique_ptr<SimulationExtension> extension = registry.create(id);
    return extension == nullptr ? "<none>" : extension->getId();
}

/// Each of @p providers as "<its ids, comma separated> | <the menu name of its first id, its
/// parts separated by ' > '>" ("<no name>" without one).
[[nodiscard]] std::vector<std::string> described(
    const std::vector<const SimulationExtensionProvider*>& providers)
{
    std::vector<std::string> texts;
    for (const SimulationExtensionProvider* provider : providers)
    {
        const std::vector<std::string>                ids = provider->getIds();
        const std::optional<std::vector<std::string>> name =
            ids.empty() ? std::nullopt : provider->getName(ids.front());
        texts.push_back(QtRocket::Strings::join(", ", ids) + " | " +
                        (name.has_value() ? QtRocket::Strings::join(" > ", *name) : "<no name>"));
    }
    return texts;
}

TEST(SimulationExtensionRegistry, AnEmptyRegistryKnowsNoExtension)
{
    const SimulationExtensionRegistry registry;
    EXPECT_TRUE(registry.empty());
    EXPECT_EQ(registry.size(), 0U);
    EXPECT_TRUE(registry.getProviders().empty());
    EXPECT_EQ(registry.create(AirStart::kId), nullptr);
    EXPECT_EQ(registry.create(""), nullptr);
}

TEST(SimulationExtensionRegistry, ListsItsProvidersInTheOrderTheyWereAdded)
{
    SimulationExtensionRegistry        registry;
    auto                               roll = std::make_unique<QtRocket::RollControlProvider>();
    auto                               air  = std::make_unique<QtRocket::AirStartProvider>();
    const SimulationExtensionProvider* rollPointer = roll.get();
    const SimulationExtensionProvider* airPointer  = air.get();
    registry.add(std::move(roll));
    registry.add(std::move(air));

    EXPECT_FALSE(registry.empty());
    EXPECT_EQ(registry.size(), 2U);
    EXPECT_EQ(registry.getProviders(),
              (std::vector<const SimulationExtensionProvider*>{rollPointer, airPointer}));

    // The providers stay where they are when the registry is moved.
    const SimulationExtensionRegistry moved = std::move(registry);
    EXPECT_EQ(moved.getProviders(),
              (std::vector<const SimulationExtensionProvider*>{rollPointer, airPointer}));
    EXPECT_NE(dynamic_cast<AirStart*>(moved.create(AirStart::kId).get()), nullptr);
}

TEST(SimulationExtensionRegistry, ANullProviderIsABug)
{
    SimulationExtensionRegistry registry;
    EXPECT_THROW(registry.add(nullptr), QtRocket::BugError);
    EXPECT_TRUE(registry.empty());
}

// The ids and the menu names of OpenRocket's own providers of the four extensions (ExtPins2,
// "provider" lines), in the order OpenRocket's plugin system lists them.
TEST(SimulationExtensionRegistry, TheBundledRegistryHasTheFourExtensionsQtRocketShips)
{
    const SimulationExtensionRegistry registry = SimulationExtensionRegistry::bundled();
    const std::vector<const SimulationExtensionProvider*> providers = registry.getProviders();
    ASSERT_EQ(providers.size(), 4U);

    // "<the ids> | <the menu name of the first id>" of each provider.
    EXPECT_EQ(described(providers),
              (std::vector<std::string>{
                  "info.openrocket.core.simulation.extension.example.AirStart | "
                  "Launch conditions > Air-start",
                  "info.openrocket.core.simulation.extension.example.RollControl | "
                  "Control Enhancements > Roll Control",
                  "info.openrocket.core.simulation.extension.impl.JavaCode | "
                  "Scripts > Java listeners",
                  "info.openrocket.core.simulation.extension.impl.ScriptingExtension | "
                  "Scripts > JavaScript"}));

    // Each id gives a new extension of its class, with that id.
    const std::unique_ptr<SimulationExtension> air    = registry.create(AirStart::kId);
    const std::unique_ptr<SimulationExtension> roll   = registry.create(RollControl::kId);
    const std::unique_ptr<SimulationExtension> java   = registry.create(JavaCode::kId);
    const std::unique_ptr<SimulationExtension> script = registry.create(ScriptingExtension::kId);
    EXPECT_NE(dynamic_cast<AirStart*>(air.get()), nullptr);
    EXPECT_NE(dynamic_cast<RollControl*>(roll.get()), nullptr);
    EXPECT_NE(dynamic_cast<JavaCode*>(java.get()), nullptr);
    EXPECT_NE(dynamic_cast<ScriptingExtension*>(script.get()), nullptr);
    EXPECT_EQ(madeFor(registry, AirStart::kId), AirStart::kId);
    EXPECT_EQ(madeFor(registry, RollControl::kId), RollControl::kId);
    EXPECT_NE(registry.create(AirStart::kId), air);

    // Two bundled registries are two sets of providers: there is no global one.
    const SimulationExtensionRegistry other = SimulationExtensionRegistry::bundled();
    EXPECT_NE(other.getProviders(), providers);
}

TEST(SimulationExtensionRegistry, AnIdNoProviderKnowsMakesNothing)
{
    const SimulationExtensionRegistry registry = SimulationExtensionRegistry::bundled();
    // The other extensions of OpenRocket, which QtRocket does not have.
    EXPECT_EQ(registry.create("info.openrocket.core.simulation.extension.example.StopSimulation"),
              nullptr);
    EXPECT_EQ(registry.create("info.openrocket.core.simulation.extension.example.DampingMoment"),
              nullptr);
    EXPECT_EQ(registry.create(""), nullptr);
    // The id is compared as it is: exactly, and without the renaming of old files, which the
    // .ork reader does before it asks.
    EXPECT_EQ(registry.create("info.openrocket.core.simulation.extension.example.airstart"),
              nullptr);
    EXPECT_EQ(registry.create(" info.openrocket.core.simulation.extension.example.AirStart"),
              nullptr);
    EXPECT_EQ(registry.create("net.sf.openrocket.simulation.extension.example.AirStart"), nullptr);
    // OpenRocket's own warning case: release 15.03 stored AirStart under impl.
    EXPECT_EQ(registry.create("info.openrocket.core.simulation.extension.impl.AirStart"), nullptr);
}

// SingleSimulationHandler: `for (p : providers) if (p.getIds().contains(id)) extension =
// p.getInstance(id);`: every provider that knows the id is asked, and the last one's wins.
TEST(SimulationExtensionRegistry, TheLastProviderThatKnowsTheIdMakesTheExtension)
{
    const std::shared_ptr<Calls> first  = std::make_shared<Calls>();
    const std::shared_ptr<Calls> second = std::make_shared<Calls>();
    const std::shared_ptr<Calls> third  = std::make_shared<Calls>();
    SimulationExtensionRegistry  registry;
    registry.add(std::make_unique<CountingProvider>(std::vector<std::string>{"x.A", "x.B"},
                                                    "made by the first", first));
    registry.add(std::make_unique<CountingProvider>(std::vector<std::string>{"x.B", "x.C"},
                                                    "made by the second", second));
    registry.add(std::make_unique<CountingProvider>(std::vector<std::string>{"x.D"},
                                                    "made by the third", third));

    EXPECT_EQ(madeFor(registry, "x.A"), "made by the first");
    EXPECT_EQ(madeFor(registry, "x.C"), "made by the second");
    EXPECT_EQ(madeFor(registry, "x.D"), "made by the third");
    EXPECT_EQ(madeFor(registry, "x.E"), "<none>");
    EXPECT_EQ(first->asked, (std::vector<std::string>{"x.A"}));
    EXPECT_EQ(second->asked, (std::vector<std::string>{"x.C"}));
    EXPECT_EQ(third->asked, (std::vector<std::string>{"x.D"}));

    // An id that two providers know: both are asked, in order, and the later one's is taken.
    EXPECT_EQ(madeFor(registry, "x.B"), "made by the second");
    EXPECT_EQ(first->asked, (std::vector<std::string>{"x.A", "x.B"}));
    EXPECT_EQ(second->asked, (std::vector<std::string>{"x.C", "x.B"}));
    EXPECT_EQ(third->asked, (std::vector<std::string>{"x.D"}));
}

// As in Java, a provider that knows the id and returns nothing replaces what an earlier one
// made: the id then counts as not found.
TEST(SimulationExtensionRegistry, AProviderThatMakesNothingStillHasTheLastWord)
{
    const std::shared_ptr<Calls> calls = std::make_shared<Calls>();
    SimulationExtensionRegistry  registry;
    registry.add(
        std::make_unique<CountingProvider>(std::vector<std::string>{"x.A"}, "made", calls));
    EXPECT_EQ(madeFor(registry, "x.A"), "made");

    registry.add(std::make_unique<CountingProvider>(std::vector<std::string>{"x.A"}, "", calls));
    EXPECT_EQ(madeFor(registry, "x.A"), "<none>");

    registry.add(
        std::make_unique<CountingProvider>(std::vector<std::string>{"x.A"}, "made again", calls));
    EXPECT_EQ(madeFor(registry, "x.A"), "made again");
}

// A provider added later overrides a bundled one for the same id (the last one wins).
TEST(SimulationExtensionRegistry, AProviderAddedToTheBundledOnesCanReplaceAnExtension)
{
    const std::shared_ptr<Calls> calls    = std::make_shared<Calls>();
    SimulationExtensionRegistry  registry = SimulationExtensionRegistry::bundled();
    registry.add(std::make_unique<CountingProvider>(
        std::vector<std::string>{std::string(AirStart::kId)}, "another air start", calls));
    EXPECT_EQ(registry.size(), 5U);
    EXPECT_EQ(madeFor(registry, AirStart::kId), "another air start");
    EXPECT_EQ(madeFor(registry, RollControl::kId), RollControl::kId);
}

}  // namespace
