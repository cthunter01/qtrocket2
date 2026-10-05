#include "QtRocket/simulation/extension/SimulationExtension.h"

#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/extension/AbstractSimulationExtension.h"
#include "QtRocket/simulation/extension/AbstractSimulationExtensionProvider.h"
#include "QtRocket/simulation/extension/SimulationExtensionProvider.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Signal.h"

namespace
{

using QtRocket::AbstractSimulationExtension;
using QtRocket::AbstractSimulationExtensionProvider;
using QtRocket::BugError;
using QtRocket::Config;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::SimulationExtension;
using QtRocket::SimulationExtensionProvider;

// OpenRocket has no test of the extension classes themselves. The extensions below have the
// shape of OpenRocket's own ones (example/AirStart and its provider), without the simulation
// listener: initialize() and documentLoaded() take SimulationConditions, Simulation and
// OpenRocketDocument, which later tiers define, so no test here can call them.

static_assert(std::is_abstract_v<SimulationExtension>);
static_assert(std::has_virtual_destructor_v<SimulationExtension>);
static_assert(std::is_abstract_v<AbstractSimulationExtension>);
static_assert(std::is_abstract_v<SimulationExtensionProvider>);
static_assert(std::has_virtual_destructor_v<SimulationExtensionProvider>);
// An extension is copied through clone() only, and the abstract provider only by a subclass.
static_assert(!std::is_copy_constructible_v<AbstractSimulationExtension>);
static_assert(!std::is_copy_assignable_v<AbstractSimulationExtension>);
static_assert(!std::is_constructible_v<AbstractSimulationExtensionProvider, std::string,
                                       AbstractSimulationExtensionProvider::Factory,
                                       std::vector<std::string>>);

/// OpenRocket's class name of the extension the tests port the shape of.
constexpr std::string_view kAirStartId =
    "info.openrocket.core.simulation.extension.example.AirStart";

/// Counts the emissions of a signal while it lives.
class ChangeCounter
{
public:
    explicit ChangeCounter(QtRocket::Signal<>& signal)
      : m_connection(signal.connect([this] { ++m_count; }))
    {
    }

    [[nodiscard]] int count() const noexcept { return m_count; }

private:
    int                                  m_count{0};
    QtRocket::Signal<>::ScopedConnection m_connection;
};

/// The shape of OpenRocket's AirStart: the configuration lives in the Config, the name is made
/// from it, and every setter announces its change.
class AirStart final : public AbstractSimulationExtension
{
public:
    AirStart() : AbstractSimulationExtension(std::string(kAirStartId)) { }

    [[nodiscard]] bool isMonteCarloSafe() const override { return true; }

    void initialize(QtRocket::SimulationConditions& /*conditions*/) override { }

    [[nodiscard]] std::string getName() const override
    {
        return std::format("Air-start ({} m)", getLaunchAltitude());
    }

    [[nodiscard]] std::optional<std::string> getDescription() const override
    {
        return "Start simulation with a configurable altitude and velocity";
    }

    [[nodiscard]] std::unique_ptr<SimulationExtension> clone() const override
    {
        return std::make_unique<AirStart>(*this);
    }

    [[nodiscard]] double getLaunchAltitude() const
    {
        return m_config.getDouble("launchAltitude", 100.0);
    }
    void setLaunchAltitude(double launchAltitude)
    {
        m_config.put("launchAltitude", launchAltitude);
        fireChangeEvent();
    }

    [[nodiscard]] double getLaunchVelocity() const
    {
        return m_config.getDouble("launchVelocity", 50.0);
    }
    void setLaunchVelocity(double launchVelocity)
    {
        m_config.put("launchVelocity", launchVelocity);
        fireChangeEvent();
    }
};

/// An extension that overrides nothing it does not have to, with a fixed name.
class Named final : public AbstractSimulationExtension
{
public:
    Named(std::string id, std::string name)
      : AbstractSimulationExtension(std::move(id), std::move(name))
    {
    }

    void initialize(QtRocket::SimulationConditions& /*conditions*/) override { }

    [[nodiscard]] std::unique_ptr<SimulationExtension> clone() const override
    {
        return std::make_unique<Named>(*this);
    }
};

/// An extension that overrides nothing it does not have to, named after its id.
class Unnamed final : public AbstractSimulationExtension
{
public:
    explicit Unnamed(std::string id) : AbstractSimulationExtension(std::move(id)) { }

    void initialize(QtRocket::SimulationConditions& /*conditions*/) override { }

    [[nodiscard]] std::unique_ptr<SimulationExtension> clone() const override
    {
        return std::make_unique<Unnamed>(*this);
    }
};

/// An extension written on the interface alone (as OpenRocket's are free to be), which makes a
/// flight data type.
class Bare final : public SimulationExtension
{
public:
    [[nodiscard]] std::string                getId() const override { return "test.Bare"; }
    [[nodiscard]] std::string                getName() const override { return "Bare"; }
    [[nodiscard]] std::optional<std::string> getDescription() const override
    {
        return std::nullopt;
    }
    void documentLoaded(QtRocket::OpenRocketDocument& /*document*/,
                        QtRocket::Simulation& /*simulation*/,
                        QtRocket::WarningSet& /*warnings*/) override
    {
    }
    void initialize(QtRocket::SimulationConditions& /*conditions*/) override { }
    [[nodiscard]] std::vector<const FlightDataType*> getFlightDataTypes() const override
    {
        return {&FlightDataType::builtin(FlightDataTypeId::TYPE_TIME)};
    }
    [[nodiscard]] std::unique_ptr<SimulationExtension> clone() const override
    {
        return std::make_unique<Bare>(*this);
    }
    [[nodiscard]] Config getConfig() const override { return m_config; }
    void                 setConfig(const Config& config) override { m_config = config; }

private:
    Config m_config;
};

/// OpenRocket's AirStartProvider.
class AirStartProvider final : public AbstractSimulationExtensionProvider
{
public:
    AirStartProvider()
      : AbstractSimulationExtensionProvider(std::string(kAirStartId),
                                            [] { return std::make_unique<AirStart>(); },
                                            {"Launch conditions", "Air-start"})
    {
    }

    /// The protected id, for the test.
    [[nodiscard]] const std::string& extensionId() const noexcept { return getExtensionId(); }
};

/// A provider with the given ids (possibly none) around the abstract class's defaults.
class ManyIdsProvider final : public AbstractSimulationExtensionProvider
{
public:
    explicit ManyIdsProvider(std::vector<std::string> ids)
      : AbstractSimulationExtensionProvider("test.First",
                                            [] { return std::make_unique<Unnamed>("test.First"); },
                                            {"Menu", "Entry"}),
        m_ids(std::move(ids))
    {
    }

    [[nodiscard]] std::vector<std::string> getIds() const override { return m_ids; }

private:
    std::vector<std::string> m_ids;
};

/// A provider whose factory is whatever the test passes.
class FactoryProvider final : public AbstractSimulationExtensionProvider
{
public:
    explicit FactoryProvider(Factory factory)
      : AbstractSimulationExtensionProvider("test.Factory", std::move(factory), {})
    {
    }
};

/// A factory of Bare extensions.
[[nodiscard]] std::unique_ptr<SimulationExtension> makeBare()
{
    return std::make_unique<Bare>();
}

/// A factory that makes nothing.
[[nodiscard]] std::unique_ptr<SimulationExtension> noExtension()
{
    return nullptr;
}

// ------------------------------------------------------------ AbstractSimulationExtension

TEST(AbstractSimulationExtension, TheIdIsTheConstructorArgument)
{
    const AirStart airStart;
    EXPECT_EQ(airStart.getId(), "info.openrocket.core.simulation.extension.example.AirStart");
    const Named named("some.package.Thing", "A thing");
    EXPECT_EQ(named.getId(), "some.package.Thing");
}

TEST(AbstractSimulationExtension, TheNameDefaultsToTheLastPartOfTheId)
{
    // Java: getClass().getSimpleName()
    EXPECT_EQ(Unnamed("info.openrocket.core.simulation.extension.example.AirStart").getName(),
              "AirStart");
    EXPECT_EQ(Unnamed("Outer.Inner").getName(), "Inner");
    EXPECT_EQ(Unnamed("NoPackage").getName(), "NoPackage");
    EXPECT_EQ(Unnamed("trailing.").getName(), "");
    EXPECT_EQ(Unnamed("").getName(), "");
}

TEST(AbstractSimulationExtension, TheNameGivenToTheConstructor)
{
    const Named named("some.package.Thing", "A thing");
    EXPECT_EQ(named.getName(), "A thing");
    EXPECT_EQ(Named("some.package.Thing", "").getName(), "");
}

TEST(AbstractSimulationExtension, ASubclassMakesItsNameFromItsConfiguration)
{
    AirStart airStart;
    EXPECT_EQ(airStart.getName(), "Air-start (100 m)");
    airStart.setLaunchAltitude(150);
    EXPECT_EQ(airStart.getName(), "Air-start (150 m)");
}

TEST(AbstractSimulationExtension, Defaults)
{
    const Named named("some.package.Thing", "A thing");
    EXPECT_EQ(named.getDescription(), std::nullopt);
    EXPECT_TRUE(named.getFlightDataTypes().empty());
    EXPECT_FALSE(named.isMonteCarloSafe());
    EXPECT_TRUE(named.getConfig().keySet().empty());
}

TEST(AbstractSimulationExtension, ASubclassOverridesTheDefaults)
{
    const AirStart airStart;
    EXPECT_EQ(airStart.getDescription(),
              "Start simulation with a configurable altitude and velocity");
    EXPECT_TRUE(airStart.isMonteCarloSafe());
    EXPECT_EQ(airStart.getLaunchAltitude(), 100.0);
    EXPECT_EQ(airStart.getLaunchVelocity(), 50.0);
}

TEST(AbstractSimulationExtension, GetConfigReturnsACopy)
{
    AirStart airStart;
    airStart.setLaunchAltitude(150);
    Config config = airStart.getConfig();
    EXPECT_EQ(config.keySet(), (std::vector<std::string>{"launchAltitude"}));
    EXPECT_EQ(config.getDouble("launchAltitude"), 150.0);
    config.put("launchAltitude", 999.0);
    config.put("extra", true);
    EXPECT_EQ(airStart.getLaunchAltitude(), 150.0);
    EXPECT_FALSE(airStart.getConfig().containsKey("extra"));
}

TEST(AbstractSimulationExtension, SetConfigCopiesAndAnnounces)
{
    AirStart            airStart;
    const ChangeCounter events(airStart.changed());
    Config              config;
    config.put("launchAltitude", 250.0);
    config.put("launchVelocity", 75.0);

    airStart.setConfig(config);
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(airStart.getLaunchAltitude(), 250.0);
    EXPECT_EQ(airStart.getLaunchVelocity(), 75.0);

    // The extension has its own copy.
    config.put("launchAltitude", 1.0);
    EXPECT_EQ(airStart.getLaunchAltitude(), 250.0);

    // The same configuration again is announced again, as in Java.
    airStart.setConfig(airStart.getConfig());
    EXPECT_EQ(events.count(), 2);

    // An empty configuration brings the defaults back.
    airStart.setConfig(Config{});
    EXPECT_EQ(events.count(), 3);
    EXPECT_EQ(airStart.getLaunchAltitude(), 100.0);
}

TEST(AbstractSimulationExtension, ASubclassSetterAnnouncesItsChange)
{
    AirStart            airStart;
    const ChangeCounter events(airStart.changed());
    airStart.setLaunchAltitude(150);
    EXPECT_EQ(events.count(), 1);
    airStart.setLaunchVelocity(10);
    EXPECT_EQ(events.count(), 2);
    // The value keeps the type it was put with: a Double.
    EXPECT_TRUE(airStart.getConfig().get("launchVelocity") == Config::Value{10.0});
    EXPECT_FALSE(airStart.getConfig().get("launchVelocity") == Config::Value{10});
}

TEST(AbstractSimulationExtension, CloneCopiesTheConfigurationDeeply)
{
    AirStart airStart;
    airStart.setLaunchAltitude(150);
    const std::unique_ptr<SimulationExtension> copy = airStart.clone();
    ASSERT_NE(copy, nullptr);
    EXPECT_NE(copy.get(), &airStart);
    EXPECT_EQ(copy->getId(), airStart.getId());
    EXPECT_EQ(copy->getName(), "Air-start (150 m)");
    EXPECT_TRUE(copy->isMonteCarloSafe());
    EXPECT_EQ(copy->getConfig().getDouble("launchAltitude"), 150.0);

    // Neither sees the other's later changes.
    airStart.setLaunchAltitude(200);
    EXPECT_EQ(copy->getConfig().getDouble("launchAltitude"), 150.0);
    Config config;
    config.put("launchAltitude", 300.0);
    copy->setConfig(config);
    EXPECT_EQ(airStart.getLaunchAltitude(), 200.0);
}

TEST(AbstractSimulationExtension, CloneKeepsTheIdAndTheName)
{
    const Named                                named("some.package.Thing", "A thing");
    const std::unique_ptr<SimulationExtension> copy = named.clone();
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->getId(), "some.package.Thing");
    EXPECT_EQ(copy->getName(), "A thing");
    const Unnamed                              unnamed("a.b.Simple");
    const std::unique_ptr<SimulationExtension> second = unnamed.clone();
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->getName(), "Simple");
}

TEST(AbstractSimulationExtension, ACloneHasNoConnections)
{
    // Deviation: Java's Object.clone() shares the listener list with the original.
    AirStart            airStart;
    const ChangeCounter events(airStart.changed());
    AirStart            copy(airStart);
    EXPECT_TRUE(copy.changed().empty());
    copy.setLaunchAltitude(1);
    EXPECT_EQ(events.count(), 0);

    const ChangeCounter copyEvents(copy.changed());
    airStart.setLaunchAltitude(2);
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(copyEvents.count(), 0);
}

// ----------------------------------------------------- the interface and its ownership

TEST(SimulationExtension, AnExtensionOnTheInterfaceAlone)
{
    Bare bare;
    EXPECT_EQ(bare.getId(), "test.Bare");
    EXPECT_EQ(bare.getName(), "Bare");
    EXPECT_EQ(bare.getDescription(), std::nullopt);
    // The interface's default: an extension is not safe for a Monte Carlo analysis unless it
    // says so.
    EXPECT_FALSE(bare.isMonteCarloSafe());
    ASSERT_EQ(bare.getFlightDataTypes().size(), 1U);
    EXPECT_EQ(bare.getFlightDataTypes()[0], &FlightDataType::builtin(FlightDataTypeId::TYPE_TIME));

    Config config;
    config.put("answer", 42);
    bare.setConfig(config);
    EXPECT_EQ(bare.getConfig().getInt("answer"), 42);
    const std::unique_ptr<SimulationExtension> copy = bare.clone();
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->getConfig().getInt("answer"), 42);
}

TEST(SimulationExtension, ASimulationsListSharesItsExtensions)
{
    // document/Simulation.copyExtensionsFrom() puts the same objects into a second list, and
    // the equality of two simulations first asks whether two extensions are the same object.
    std::vector<std::shared_ptr<SimulationExtension>> first;
    first.push_back(std::make_shared<AirStart>());
    first.push_back(std::make_shared<Bare>());
    const std::vector<std::shared_ptr<SimulationExtension>> second = first;
    EXPECT_EQ(second[0], first[0]);
    EXPECT_EQ(second[1], first[1]);

    Config config;
    config.put("launchAltitude", 400.0);
    first[0]->setConfig(config);
    EXPECT_EQ(second[0]->getConfig().getDouble("launchAltitude"), 400.0);

    // A clone converts to the list's shared pointer and is another object.
    std::vector<std::shared_ptr<SimulationExtension>> cloned;
    cloned.push_back(first[0]->clone());
    ASSERT_NE(cloned[0], nullptr);
    EXPECT_NE(cloned[0], first[0]);
    EXPECT_EQ(cloned[0]->getId(), first[0]->getId());
    EXPECT_EQ(cloned[0]->getConfig().getDouble("launchAltitude"), 400.0);
}

// --------------------------------------------------- AbstractSimulationExtensionProvider

TEST(AbstractSimulationExtensionProvider, TheIdsAreTheOneId)
{
    const AirStartProvider provider;
    EXPECT_EQ(provider.getIds(), (std::vector<std::string>{std::string(kAirStartId)}));
    EXPECT_EQ(provider.extensionId(), kAirStartId);
}

TEST(AbstractSimulationExtensionProvider, TheNameIsGivenForTheIdOnly)
{
    const AirStartProvider provider;
    EXPECT_EQ(provider.getName(kAirStartId),
              (std::vector<std::string>{"Launch conditions", "Air-start"}));
    EXPECT_EQ(provider.getName("info.openrocket.core.simulation.extension.example.CSVSave"),
              std::nullopt);
    EXPECT_EQ(provider.getName(""), std::nullopt);
    EXPECT_EQ(provider.getName("AirStart"), std::nullopt);
}

TEST(AbstractSimulationExtensionProvider, GetInstanceMakesANewExtensionEachTime)
{
    const AirStartProvider                     provider;
    const std::unique_ptr<SimulationExtension> first  = provider.getInstance(kAirStartId);
    const std::unique_ptr<SimulationExtension> second = provider.getInstance(kAirStartId);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first, second);
    EXPECT_EQ(first->getId(), kAirStartId);
    // The default configuration.
    EXPECT_TRUE(first->getConfig().keySet().empty());
    EXPECT_EQ(first->getName(), "Air-start (100 m)");

    Config config;
    config.put("launchAltitude", 250.0);
    first->setConfig(config);
    EXPECT_EQ(second->getName(), "Air-start (100 m)");
}

TEST(AbstractSimulationExtensionProvider, GetInstanceDoesNotLookAtTheId)
{
    // Java: injector.getInstance(extensionClass), whatever the id.
    const AirStartProvider                     provider;
    const std::unique_ptr<SimulationExtension> extension = provider.getInstance("anything");
    ASSERT_NE(extension, nullptr);
    EXPECT_EQ(extension->getId(), kAirStartId);
}

TEST(AbstractSimulationExtensionProvider, ASubclassWithSeveralIdsNamesOnlyTheFirst)
{
    const ManyIdsProvider provider({"test.First", "test.LegacyName"});
    EXPECT_EQ(provider.getIds(), (std::vector<std::string>{"test.First", "test.LegacyName"}));
    EXPECT_EQ(provider.getName("test.First"), (std::vector<std::string>{"Menu", "Entry"}));
    EXPECT_EQ(provider.getName("test.LegacyName"), std::nullopt);
    const std::unique_ptr<SimulationExtension> extension = provider.getInstance("test.LegacyName");
    ASSERT_NE(extension, nullptr);
    EXPECT_EQ(extension->getId(), "test.First");
}

TEST(AbstractSimulationExtensionProvider, ASubclassWithoutIdsCannotBeAskedForAName)
{
    // Java: getIds().get(0) throws IndexOutOfBoundsException.
    const ManyIdsProvider provider({});
    EXPECT_TRUE(provider.getIds().empty());
    EXPECT_THROW(static_cast<void>(provider.getName("test.First")), BugError);
}

TEST(AbstractSimulationExtensionProvider, AnEmptyNameIsAnEmptyList)
{
    const FactoryProvider provider(&makeBare);
    EXPECT_EQ(provider.getName("test.Factory"), std::vector<std::string>{});
    const std::unique_ptr<SimulationExtension> extension = provider.getInstance("test.Factory");
    ASSERT_NE(extension, nullptr);
    EXPECT_EQ(extension->getId(), "test.Bare");
}

TEST(AbstractSimulationExtensionProvider, TheFactoryMustMakeAnExtension)
{
    EXPECT_THROW(static_cast<void>(FactoryProvider(AbstractSimulationExtensionProvider::Factory{})),
                 BugError);
    const FactoryProvider provider(&noExtension);
    EXPECT_THROW(static_cast<void>(provider.getInstance("test.Factory")), BugError);
}

TEST(SimulationExtensionProvider, AProviderIsUsedThroughTheInterface)
{
    const std::unique_ptr<SimulationExtensionProvider> provider =
        std::make_unique<AirStartProvider>();
    ASSERT_EQ(provider->getIds().size(), 1U);
    const std::string                          id        = provider->getIds()[0];
    const std::shared_ptr<SimulationExtension> extension = provider->getInstance(id);
    ASSERT_NE(extension, nullptr);
    EXPECT_EQ(extension->getId(), id);
    EXPECT_TRUE(provider->getName(id).has_value());
}

}  // namespace
