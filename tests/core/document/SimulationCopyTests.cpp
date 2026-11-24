#include "QtRocket/document/SimulationCopy.h"

#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/LandingDispersionSettings.h"
#include "QtRocket/simulation/PlotAppearance.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/example/AirStart.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/Signal.h"
#include "document/TestDocuments.h"
#include "rocket/TestRockets.h"

// OpenRocket has no test of copying simulations (SimulationPanel's copy and paste, the
// clipboard's simulations); the expectations follow the Java of Simulation.copy() and
// duplicateSimulation() (Simulation.java:650-683, 802-838) and SimulationPanel's paste
// (SimulationPanel.java:799-835), and the sim-tab scouting's probe of the tab (SimTabProbe:
// duplicate names, the configurations a paste creates, bug 22).

namespace
{

using QtRocket::AirStart;
using QtRocket::BugError;
using QtRocket::Color;
using QtRocket::DocumentChangeEvent;
using QtRocket::FlightConfigurationId;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::InMemoryPreferences;
using QtRocket::LandingDispersionSettings;
using QtRocket::LineStyle;
using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketDocumentFactory;
using QtRocket::PlotAppearance;
using QtRocket::Preferences;
using QtRocket::Simulation;
using QtRocket::SimulationCopy;
using QtRocket::SimulationExtension;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;
using Status = Simulation::Status;

static_assert(!std::is_copy_constructible_v<SimulationCopy>);
static_assert(std::is_nothrow_move_constructible_v<SimulationCopy>);

/// The altitude type, whose appearance the source simulation overrides.
[[nodiscard]] const FlightDataType& altitudeType()
{
    return FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE);
}

/// The appearance the source simulation gives the altitude.
constexpr PlotAppearance kAltitudeAppearance{Color{1, 2, 3}, LineStyle::DASHED};

/// A design of TestRockets' Estes Alpha III with one simulation of @p preferences, "Original",
/// in the second test configuration, with a launch rod of 2.5 m, an air start at 100 m, landing
/// dispersion settings and an appearance of the altitude, and simulated (its data is what a
/// copy leaves behind).
class SourceDesign
{
public:
    explicit SourceDesign(Preferences& preferences)
      : m_document(OpenRocketDocumentFactory::createDocumentFromRocket(takeRocket())),
        m_simulation(
            std::make_shared<Simulation>(m_document.get(), m_document->getRocket(), preferences))
    {
        m_simulation->setName("Original");
        m_simulation->setFlightConfigurationId(testFcid(1));
        m_simulation->getOptions().setLaunchRodLength(2.5);
        m_simulation->getOptions().setTimeStep(0.05);
        const auto airStart = std::make_shared<AirStart>();
        airStart->setLaunchAltitude(100);
        m_simulation->getSimulationExtensions().push_back(airStart);
        m_simulation->setLandingDispersionSettings(
            LandingDispersionSettings({{"runs", "500"}, {"seed", "12345"}}));
        m_simulation->setPlotAppearance(altitudeType(), kAltitudeAppearance);
        m_document->addSimulation(m_simulation);
    }

    [[nodiscard]] OpenRocketDocument& document() const noexcept { return *m_document; }
    [[nodiscard]] Simulation&         simulation() const noexcept { return *m_simulation; }

    /// The copy of the simulation.
    [[nodiscard]] SimulationCopy copy() const
    {
        return SimulationCopy::copyOf({m_simulation.get()});
    }

private:
    [[nodiscard]] static std::unique_ptr<QtRocket::Rocket> takeRocket()
    {
        TestEstesAlphaIII alpha;
        return std::move(alpha.rocket);
    }

    std::unique_ptr<OpenRocketDocument> m_document;
    std::shared_ptr<Simulation>         m_simulation;
};

/// The air start altitude of @p extension, or -1 when it is no AirStart.
[[nodiscard]] double airStartAltitude(const SimulationExtension& extension)
{
    const auto* airStart = dynamic_cast<const AirStart*>(&extension);
    return airStart != nullptr ? airStart->getLaunchAltitude() : -1;
}

/// The names of the simulations of @p document, each followed by a comma.
[[nodiscard]] std::string simulationNames(const OpenRocketDocument& document)
{
    std::string names;
    for (const std::shared_ptr<Simulation>& simulation : document.getSimulations())
    {
        names += simulation->getName();
        names += ',';
    }
    return names;
}

/// Adds simulations of @p preferences named @p names to @p document, in that order.
void addSimulationsNamed(OpenRocketDocument& document, Preferences& preferences,
                         const std::vector<std::string>& names)
{
    for (const std::string& name : names)
    {
        const auto simulation =
            std::make_shared<Simulation>(&document, document.getRocket(), preferences);
        simulation->setName(name);
        document.addSimulation(simulation);
    }
}

// ------------------------------------------------------------------------------ the copy

TEST(SimulationCopy, ACopyOfNothingIsEmpty)
{
    const SimulationCopy nothing;
    EXPECT_TRUE(nothing.empty());
    EXPECT_EQ(nothing.size(), 0U);
    EXPECT_TRUE(SimulationCopy::copyOf({}).empty());
}

TEST(SimulationCopy, TheCopyKeepsWhatJavasCopyCarries)
{
    InMemoryPreferences  preferences;
    const SourceDesign   source(preferences);
    const SimulationCopy copy = source.copy();
    ASSERT_EQ(copy.size(), 1U);
    const SimulationCopy::Entry& entry = copy.getEntry(0);
    EXPECT_EQ(entry.name, "Original");
    EXPECT_EQ(entry.configurationId, testFcid(1));
    EXPECT_EQ(entry.options.getLaunchRodLength(), 2.5);
    // A copy of the options with their seeds: equal to the source's.
    EXPECT_TRUE(entry.options == source.simulation().getOptions());
    EXPECT_NE(&entry.options, &source.simulation().getOptions());
    ASSERT_EQ(entry.extensions.size(), 1U);
    EXPECT_NE(entry.extensions[0].get(), source.simulation().getSimulationExtensions()[0].get());
    EXPECT_EQ(entry.extensions[0]->getId(), AirStart::kId);
    EXPECT_EQ(airStartAltitude(*entry.extensions[0]), 100.0);
    EXPECT_EQ(entry.landingDispersionSettings, source.simulation().getLandingDispersionSettings());
    EXPECT_TRUE(entry.landingDispersionSettings.has_value());
    EXPECT_EQ(entry.plotAppearances, source.simulation().getPlotAppearances());
    EXPECT_EQ(entry.plotAppearances.size(), 1U);
    EXPECT_EQ(entry.preferences, &preferences);
}

TEST(SimulationCopy, TheCopyKeepsTheOrderOfTheSimulations)
{
    InMemoryPreferences preferences;
    const SourceDesign  source(preferences);
    addSimulationsNamed(source.document(), preferences, {"B", "C"});
    const std::shared_ptr<Simulation> b = source.document().getSimulation(1);
    const std::shared_ptr<Simulation> c = source.document().getSimulation(2);
    const SimulationCopy copy = SimulationCopy::copyOf({c.get(), &source.simulation(), b.get()});
    ASSERT_EQ(copy.size(), 3U);
    EXPECT_EQ(copy.getEntry(0).name, "C");
    EXPECT_EQ(copy.getEntry(1).name, "Original");
    EXPECT_EQ(copy.getEntry(2).name, "B");
}

TEST(SimulationCopy, TheCopyIsDetachedFromItsSource)
{
    InMemoryPreferences preferences;
    SimulationCopy      copy;
    {
        const SourceDesign source(preferences);
        copy = source.copy();
        // Later changes of the source do not reach the copy.
        source.simulation().setName("Renamed");
        source.simulation().getOptions().setLaunchRodLength(4.0);
        source.simulation().setPlotAppearance(altitudeType(), std::nullopt);
        dynamic_cast<AirStart&>(*source.simulation().getSimulationExtensions()[0])
            .setLaunchAltitude(200);
    }
    // The source design is gone; the copy goes on.
    const SimulationCopy::Entry& entry = copy.getEntry(0);
    EXPECT_EQ(entry.name, "Original");
    EXPECT_EQ(entry.options.getLaunchRodLength(), 2.5);
    EXPECT_EQ(entry.plotAppearances.size(), 1U);
    EXPECT_EQ(airStartAltitude(*entry.extensions[0]), 100.0);

    const std::unique_ptr<OpenRocketDocument>      target = QtRocket::Test::makeTestRocketV100();
    const std::vector<std::shared_ptr<Simulation>> pasted = copy.insertInto(*target, 0);
    ASSERT_EQ(pasted.size(), 1U);
    EXPECT_EQ(pasted[0]->getName(), "Original");
    EXPECT_EQ(pasted[0]->getOptions().getLaunchRodLength(), 2.5);
}

TEST(SimulationCopy, ACopyOfANullSimulationIsABug)
{
    EXPECT_THROW(static_cast<void>(SimulationCopy::copyOf({nullptr})), BugError);
}

TEST(SimulationCopy, AnEntryOutOfRangeIsABug)
{
    InMemoryPreferences  preferences;
    const SourceDesign   source(preferences);
    const SimulationCopy copy = source.copy();
    EXPECT_THROW(static_cast<void>(copy.getEntry(1)), BugError);
    EXPECT_THROW(static_cast<void>(copy.makeSimulation(1, source.document())), BugError);
}

// ---------------------------------------------------------------------------- the paste

TEST(SimulationCopy, AMadeSimulationIsANewSimulationOfTheTarget)
{
    // Java: duplicateSimulation(targetRocket) of the clipboard's copy: "name Original fcid same
    // ... status NOT_SIMULATED ... conditions null data null description null extension clone".
    InMemoryPreferences preferences;
    const SourceDesign  source(preferences);
    ASSERT_TRUE(source.simulation().simulate().has_value());
    ASSERT_TRUE(source.simulation().getSimulatedConfigurationDescription().has_value());
    const SimulationCopy copy = source.copy();
    const SourceDesign   target(preferences);

    const std::shared_ptr<Simulation> made = copy.makeSimulation(0, target.document());
    EXPECT_EQ(made->getDocument(), &target.document()) << "bug 22: Java's is the source design";
    EXPECT_EQ(&made->getRocket(), &target.document().getRocket());
    EXPECT_EQ(made->getPreferences(), &preferences);
    EXPECT_EQ(made->getName(), "Original");
    EXPECT_EQ(made->getFlightConfigurationId(), testFcid(1));
    EXPECT_EQ(made->getStoredStatus(), Status::NOT_SIMULATED);
    EXPECT_EQ(made->getSimulatedConditions(), nullptr);
    EXPECT_EQ(made->getSimulatedData(), nullptr);
    EXPECT_FALSE(made->getSimulatedConfigurationDescription().has_value());
    EXPECT_EQ(made->getOptions().getLaunchRodLength(), 2.5);
    EXPECT_EQ(made->getOptions().getTimeStep(), 0.05);
    ASSERT_EQ(made->getSimulationExtensions().size(), 1U);
    EXPECT_NE(made->getSimulationExtensions()[0].get(), copy.getEntry(0).extensions[0].get());
    EXPECT_EQ(airStartAltitude(*made->getSimulationExtensions()[0]), 100.0);
    EXPECT_EQ(made->getLandingDispersionSettings(),
              source.simulation().getLandingDispersionSettings());
    EXPECT_EQ(made->getPlotAppearance(altitudeType()), kAltitudeAppearance);
    // Not added: the target has its own simulation only.
    EXPECT_EQ(target.document().getSimulationCount(), 1U);
}

TEST(SimulationCopy, EveryPasteMakesSimulationsOfItsOwn)
{
    InMemoryPreferences               preferences;
    const SourceDesign                source(preferences);
    const SimulationCopy              copy   = source.copy();
    const std::shared_ptr<Simulation> first  = copy.makeSimulation(0, source.document());
    const std::shared_ptr<Simulation> second = copy.makeSimulation(0, source.document());
    EXPECT_NE(first.get(), second.get());
    EXPECT_NE(&first->getOptions(), &second->getOptions());
    EXPECT_NE(first->getSimulationExtensions()[0].get(),
              second->getSimulationExtensions()[0].get());
    // A change of a pasted simulation reaches neither the copy nor the other paste.
    first->setPlotAppearance(altitudeType(), std::nullopt);
    dynamic_cast<AirStart&>(*first->getSimulationExtensions()[0]).setLaunchAltitude(300);
    EXPECT_EQ(second->getPlotAppearance(altitudeType()), kAltitudeAppearance);
    EXPECT_EQ(airStartAltitude(*second->getSimulationExtensions()[0]), 100.0);
    EXPECT_EQ(copy.getEntry(0).plotAppearances.size(), 1U);
    EXPECT_EQ(airStartAltitude(*copy.getEntry(0).extensions[0]), 100.0);
}

TEST(SimulationCopy, AMadeSimulationOfACopyWithoutPreferencesHasNone)
{
    TestEstesAlphaIII                   alpha;
    std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createDocumentFromRocket(std::move(alpha.rocket));
    const auto simulation = std::make_shared<Simulation>(document.get(), document->getRocket());
    simulation->getOptions().setLaunchRodLength(3.5);
    const SimulationCopy copy = SimulationCopy::copyOf({simulation.get()});
    EXPECT_EQ(copy.getEntry(0).preferences, nullptr);
    const std::shared_ptr<Simulation> made = copy.makeSimulation(0, *document);
    EXPECT_EQ(made->getPreferences(), nullptr);
    EXPECT_EQ(made->getOptions().getLaunchRodLength(), 3.5);
}

TEST(SimulationCopy, APastedSimulationBelongsToTheDesignItWasPastedInto)
{
    // Bug 22: in OpenRocket the pasted simulation stays a listener of the design it was copied
    // from, so its changes mark that design unsaved and leave the target saved (SimTabProbe).
    InMemoryPreferences                            preferences;
    const SourceDesign                             source(preferences);
    const SourceDesign                             target(preferences);
    const SimulationCopy                           copy   = source.copy();
    const std::vector<std::shared_ptr<Simulation>> pasted = copy.insertInto(target.document(), 1);
    ASSERT_EQ(pasted.size(), 1U);
    source.document().setSaved(true);
    target.document().setSaved(true);

    pasted[0]->setName("Changed");
    EXPECT_FALSE(target.document().isSaved());
    EXPECT_TRUE(source.document().isSaved());
    target.document().setSaved(true);
    pasted[0]->getOptions().setLaunchRodLength(1.25);
    EXPECT_FALSE(target.document().isSaved());
    EXPECT_TRUE(source.document().isSaved());
}

TEST(SimulationCopy, InsertIntoPutsTheCopiesAtTheIndexInOrder)
{
    InMemoryPreferences preferences;
    const SourceDesign  source(preferences);
    addSimulationsNamed(source.document(), preferences, {"B"});
    const std::shared_ptr<Simulation> b = source.document().getSimulation(1);
    const SimulationCopy copy           = SimulationCopy::copyOf({b.get(), &source.simulation()});

    const std::vector<std::shared_ptr<Simulation>> pasted = copy.insertInto(source.document(), 1);
    EXPECT_EQ(simulationNames(source.document()), "Original,B,Original,B,");
    ASSERT_EQ(pasted.size(), 2U);
    EXPECT_EQ(pasted[0].get(), source.document().getSimulation(1).get());
    EXPECT_EQ(pasted[1].get(), source.document().getSimulation(2).get());
    // At the end.
    static_cast<void>(copy.insertInto(source.document(), 4));
    EXPECT_EQ(simulationNames(source.document()), "Original,B,Original,B,B,Original,");
}

TEST(SimulationCopy, InsertIntoBeyondTheEndIsABug)
{
    InMemoryPreferences  preferences;
    const SourceDesign   source(preferences);
    const SimulationCopy copy = source.copy();
    EXPECT_THROW(static_cast<void>(copy.insertInto(source.document(), 2)), BugError);
    EXPECT_EQ(source.document().getSimulationCount(), 1U);
}

TEST(SimulationCopy, ANumberedNameBecomesTheNextFreeOne)
{
    // SimTabProbe's duplicates: "Simulation 1" -> "Simulation 6", "Simulation 3 - too short
    // delay" keeps its name, "Simulation 9" -> "Simulation 10"; the next name is asked after each
    // simulation that went in, and "Simulation 9 " (a trailing space) matches the rename rule
    // though getNextSimulationName() does not count it.
    InMemoryPreferences                       preferences;
    const std::unique_ptr<OpenRocketDocument> source = QtRocket::Test::makeTestRocketV100();
    addSimulationsNamed(
        *source, preferences,
        {"Simulation 1", "Simulation 3 - too short delay", "Simulation 9 ", "Simulation 2"});
    std::vector<const Simulation*> simulations;
    for (const std::shared_ptr<Simulation>& simulation : source->getSimulations())
    {
        simulations.push_back(simulation.get());
    }
    const SimulationCopy copy = SimulationCopy::copyOf(simulations);

    const std::unique_ptr<OpenRocketDocument> target = QtRocket::Test::makeTestRocketV100();
    addSimulationsNamed(
        *target, preferences,
        {"Simulation 1", "Simulation 2", "Simulation 3", "Simulation 4", "Simulation 5"});
    static_cast<void>(copy.insertInto(*target, 5));
    EXPECT_EQ(simulationNames(*target),
              "Simulation 1,Simulation 2,Simulation 3,Simulation 4,Simulation 5,Simulation 6,"
              "Simulation 3 - too short delay,Simulation 7,Simulation 8,");
}

TEST(SimulationCopy, HasNumberedNameIsJavasRegex)
{
    // name.matches("Simulation [0-9]+ *"): the whole name.
    EXPECT_TRUE(SimulationCopy::hasNumberedName("Simulation 1"));
    EXPECT_TRUE(SimulationCopy::hasNumberedName("Simulation 12"));
    EXPECT_TRUE(SimulationCopy::hasNumberedName("Simulation 007"));
    EXPECT_TRUE(SimulationCopy::hasNumberedName("Simulation 9 "));
    EXPECT_TRUE(SimulationCopy::hasNumberedName("Simulation 9   "));
    EXPECT_FALSE(SimulationCopy::hasNumberedName("Simulation"));
    EXPECT_FALSE(SimulationCopy::hasNumberedName("Simulation "));
    EXPECT_FALSE(SimulationCopy::hasNumberedName("Simulation  1"));
    EXPECT_FALSE(SimulationCopy::hasNumberedName("Simulation -1"));
    EXPECT_FALSE(SimulationCopy::hasNumberedName("Simulation +1"));
    EXPECT_FALSE(SimulationCopy::hasNumberedName("Simulation 1a"));
    EXPECT_FALSE(SimulationCopy::hasNumberedName("Simulation 1 a"));
    EXPECT_FALSE(SimulationCopy::hasNumberedName("Simulation 1\t"));
    EXPECT_FALSE(SimulationCopy::hasNumberedName("simulation 1"));
    EXPECT_FALSE(SimulationCopy::hasNumberedName(" Simulation 1"));
    EXPECT_FALSE(SimulationCopy::hasNumberedName("Simulation 3 - too short delay"));
    EXPECT_FALSE(SimulationCopy::hasNumberedName(""));
    // [0-9] is ASCII: an Arabic-Indic digit is no digit to the regex.
    EXPECT_FALSE(SimulationCopy::hasNumberedName("Simulation \xD9\xA1"));
}

TEST(SimulationCopy, APasteIntoADesignWithoutTheConfigurationCreatesItInTheUndoStep)
{
    // SimTabProbe: into a design that lacks the configuration ids, the paste creates empty
    // configurations ("[No motors]") inside the undo step.
    InMemoryPreferences                       preferences;
    const SourceDesign                        source(preferences);
    const SimulationCopy                      copy   = source.copy();
    const std::unique_ptr<OpenRocketDocument> target = QtRocket::Test::makeTestRocketV100();
    ASSERT_FALSE(target->getRocket().containsFlightConfigurationId(testFcid(1)));
    const int configurations = target->getRocket().getFlightConfigurationCount();
    target->clearUndo();

    target->addUndoPosition("Duplicate Original");
    static_cast<void>(copy.insertInto(*target, 0));
    EXPECT_TRUE(target->getRocket().containsFlightConfigurationId(testFcid(1)));
    EXPECT_EQ(target->getRocket().getFlightConfigurationCount(), configurations + 1);
    EXPECT_FALSE(target->getRocket().getFlightConfiguration(testFcid(1)).hasMotors());
    EXPECT_EQ(target->getSimulationCount(), 1U);

    ASSERT_TRUE(target->isUndoAvailable());
    target->undo();
    EXPECT_EQ(target->getSimulationCount(), 0U);
    EXPECT_FALSE(target->getRocket().containsFlightConfigurationId(testFcid(1)));
    target->redo();
    EXPECT_EQ(target->getSimulationCount(), 1U);
    EXPECT_TRUE(target->getRocket().containsFlightConfigurationId(testFcid(1)));
}

/// Counts the SIMULATION events of a document while it lives.
class SimulationEventCounter
{
public:
    explicit SimulationEventCounter(OpenRocketDocument& document)
      : m_connection(document.documentChanged().connect([this](const DocumentChangeEvent& event) {
            m_count += event.isSimulationChange() ? 1 : 0;
        }))
    {
    }

    [[nodiscard]] int count() const noexcept { return m_count; }

private:
    int                                                            m_count{0};
    QtRocket::Signal<const DocumentChangeEvent&>::ScopedConnection m_connection;
};

TEST(SimulationCopy, APasteIsOneEventPerSimulation)
{
    InMemoryPreferences preferences;
    const SourceDesign  source(preferences);
    addSimulationsNamed(source.document(), preferences, {"B"});
    const std::shared_ptr<Simulation> b = source.document().getSimulation(1);
    const SimulationCopy         copy   = SimulationCopy::copyOf({&source.simulation(), b.get()});
    const SimulationEventCounter events(source.document());
    static_cast<void>(copy.insertInto(source.document(), 2));
    EXPECT_EQ(events.count(), 2);
}

}  // namespace
