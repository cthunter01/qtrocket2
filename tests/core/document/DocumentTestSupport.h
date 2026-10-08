#pragma once

// What the tests of OpenRocketDocument share: a recorder of what a document emits, the state of
// a document as the Java probes print it, and a document of TestRockets' Estes Alpha III with
// simulations made as OpenRocket's tests make them. Test-only.

#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/Signal.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationRunSupport.h"

namespace QtRocket::Test
{

/// @p parts joined with @p separator.
[[nodiscard]] inline std::string joined(const std::vector<std::string>& parts,
                                        std::string_view                separator)
{
    std::string text;
    for (const std::string& part : parts)
    {
        if (!text.empty())
        {
            text += separator;
        }
        text += part;
    }
    return text;
}

/// A description as Java prints a String: the text, or "null".
[[nodiscard]] inline std::string orNull(const std::optional<std::string>& text)
{
    return text.value_or("null");
}

/// Records what a document emits, in the notation of the Java probes (DocumentProbe.java and
/// the others of the tier 9 probes): "U" for undoRedoChanged(), "D(source)" for a
/// documentChanged() of kind DOCUMENT and "S(source)" for one of kind SIMULATION,
/// "SAVING(source)" for documentSaving() and "ERR(text)" for an undo error. The source is the
/// simple name of its Java class: that of a component ("NoseCone", "Rocket"), "Simulation",
/// "OpenRocketDocument" or "DocumentPreferences". With @p withSaved a D or S that is emitted
/// while the document is saved has ",saved" after the source, as DocumentProbe3.java prints it.
///
/// The Java probes also print "C[...]" for a listener of the rocket; where it stands among the
/// document's events is arbitrary in Java (the rocket's listeners are a HashSet), so it is not
/// recorded here.
class DocumentRecorder
{
public:
    explicit DocumentRecorder(OpenRocketDocument& document, bool withSaved = false)
      : m_document(&document),
        m_withSaved(withSaved),
        m_undoRedo(document.undoRedoChanged().connect([this] { m_log.emplace_back("U"); })),
        m_changed(document.documentChanged().connect([this](const DocumentChangeEvent& event) {
            record(event.isSimulationChange() ? "S" : "D", event);
        })),
        m_saving(document.documentSaving().connect(
            [this](const DocumentChangeEvent& event) { record("SAVING", event); })),
        m_errors(document.undoErrorOccurred().connect([this](const std::string& message) {
            m_log.push_back(std::format("ERR({})", message));
        }))
    {
    }

    /// What was recorded since the last call, separated by spaces; empty when nothing was.
    [[nodiscard]] std::string take()
    {
        std::string text = joined(m_log, " ");
        m_log.clear();
        return text;
    }

    /// Forgets what was recorded.
    void clear() noexcept { m_log.clear(); }

    /// The last event of documentChanged() or documentSaving(); nullopt before the first.
    [[nodiscard]] const std::optional<DocumentChangeEvent>& lastEvent() const noexcept
    {
        return m_lastEvent;
    }

private:
    void record(std::string_view letter, const DocumentChangeEvent& event)
    {
        m_lastEvent = event;
        m_log.push_back(std::format("{}({}{})", letter, sourceName(event),
                                    m_withSaved && m_document->isSaved() ? ",saved" : ""));
    }

    /// The simple name of the Java class of the source; a document or the preferences of
    /// another document than the recorded one get a question mark.
    [[nodiscard]] std::string sourceName(const DocumentChangeEvent& event) const
    {
        if (const OpenRocketDocument* const document = event.getDocument())
        {
            return document == m_document ? "OpenRocketDocument" : "OpenRocketDocument?";
        }
        if (const RocketComponent* const component = event.getComponent())
        {
            return std::string(className(component->kind()));
        }
        if (event.getSimulation() != nullptr)
        {
            return "Simulation";
        }
        if (const DocumentPreferences* const preferences = event.getPreferences())
        {
            return preferences == &m_document->getDocumentPreferences() ? "DocumentPreferences"
                                                                        : "DocumentPreferences?";
        }
        return "null";
    }

    OpenRocketDocument*                                  m_document;
    bool                                                 m_withSaved;
    std::vector<std::string>                             m_log;
    std::optional<DocumentChangeEvent>                   m_lastEvent;
    Signal<>::ScopedConnection                           m_undoRedo;
    Signal<const DocumentChangeEvent&>::ScopedConnection m_changed;
    Signal<const DocumentChangeEvent&>::ScopedConnection m_saving;
    Signal<const std::string&>::ScopedConnection         m_errors;
};

/// Records the change events of a rocket as DocumentProbe3.java prints them:
/// "C[NoseCone,texture,nonfunctional]", the Java class of the source and the two flags that
/// apply.
class RocketEventRecorder
{
public:
    explicit RocketEventRecorder(Rocket& rocket)
      : m_connection(rocket.addComponentChangeListener([this](const ComponentChangeEvent& event) {
            m_log.push_back(std::format("C[{}{}{}]", className(event.getSource()->kind()),
                                        event.isTextureChange() ? ",texture" : "",
                                        event.isNonFunctionalChange() ? ",nonfunctional" : ""));
        }))
    {
    }

    /// What was recorded since the last call, separated by spaces.
    [[nodiscard]] std::string take()
    {
        std::string text = joined(m_log, " ");
        m_log.clear();
        return text;
    }

private:
    std::vector<std::string>                m_log;
    ComponentChangeSignal::ScopedConnection m_connection;
};

/// The undo state of @p document as the Java probes print it:
/// "pos=0 hist=1 desc=[null] next=null stored=null".
[[nodiscard]] inline std::string undoState(const OpenRocketDocument& document)
{
    const OpenRocketDocument::UndoDetail detail = document.getUndoDetail();
    std::vector<std::string>             descriptions;
    descriptions.reserve(detail.descriptions.size());
    for (const std::optional<std::string>& description : detail.descriptions)
    {
        descriptions.push_back(orNull(description));
    }
    return std::format("pos={} hist={} desc=[{}] next={} stored={}", detail.position,
                       detail.descriptions.size(), joined(descriptions, ", "),
                       orNull(detail.nextDescription), orNull(detail.storedDescription));
}

/// The state of @p document as DocumentProbe.java's state() prints it:
/// "pos=0 hist=1 desc=[null] next=null stored=null undoAvail=false undoDesc=null
/// redoAvail=false redoDesc=null saved=true sims=0" (one line).
[[nodiscard]] inline std::string state(const OpenRocketDocument& document)
{
    return std::format("{} undoAvail={} undoDesc={} redoAvail={} redoDesc={} saved={} sims={}",
                       undoState(document), document.isUndoAvailable(),
                       orNull(document.getUndoDescription()), document.isRedoAvailable(),
                       orNull(document.getRedoDescription()), document.isSaved(),
                       document.getSimulationCount());
}

/// state() followed by " configs=<the number of flight configurations>", as DocumentProbe3.java
/// and DocumentProbe4.java print it.
[[nodiscard]] inline std::string stateWithConfigs(const OpenRocketDocument& document)
{
    return std::format("{} configs={}", state(document),
                       document.getRocket().getFlightConfigurationCount());
}

/// A document of TestRockets' Estes Alpha III, the rocket of OpenRocket's document tests and of
/// the probes (Java: OpenRocketDocumentFactory.createDocumentFromRocket(
/// TestRockets.makeEstesAlphaIII()), or new OpenRocketDocument(rocket)), with the preferences
/// OpenRocket's tests run under.
///
/// An undo or a redo replaces every component, so the components are looked up when they are
/// asked for, as the probes do (rocket.getChild(0).getChild(0)); a reference from before an undo
/// is dangling.
class AlphaDocument
{
public:
    AlphaDocument() : m_document(OpenRocketDocumentFactory::createDocumentFromRocket(takeRocket()))
    {
    }

    [[nodiscard]] OpenRocketDocument& document() const noexcept { return *m_document; }
    [[nodiscard]] Rocket&             rocket() const noexcept { return m_document->getRocket(); }

    /// rocket.getChild(0): the stage.
    [[nodiscard]] AxialStage& stage() const
    {
        return dynamic_cast<AxialStage&>(rocket().getChild(0));
    }
    /// rocket.getChild(0).getChild(0): the nose cone.
    [[nodiscard]] NoseCone& nose() const { return dynamic_cast<NoseCone&>(stage().getChild(0)); }
    /// rocket.getChild(0).getChild(1): the body tube.
    [[nodiscard]] BodyTube& body() const { return dynamic_cast<BodyTube&>(stage().getChild(1)); }
    /// The parachute, the fourth child of the body tube.
    [[nodiscard]] Parachute& chute() const { return dynamic_cast<Parachute&>(body().getChild(3)); }

    /// A simulation of the document's rocket as the Java tests and probes make one:
    /// `new Simulation(document, rocket)` under the test preferences, with the ISA atmosphere
    /// and a time step of 0.05 s (with the test preferences' time step of 0 a run never ends).
    /// The random seed is fixed, so that a run does not depend on the seed the options drew.
    /// It is not added to the document.
    [[nodiscard]] std::shared_ptr<Simulation> newSimulation() const
    {
        std::shared_ptr<Simulation> simulation =
            std::make_shared<Simulation>(m_document.get(), rocket(), m_preferences->store);
        simulation->getOptions().setIsaAtmosphere(true);
        simulation->getOptions().setTimeStep(0.05);
        simulation->getOptions().setRandomSeed(0);
        return simulation;
    }

    /// newSimulation() for the flight configuration @p id, named @p name (the probes' sim()).
    [[nodiscard]] std::shared_ptr<Simulation> newSimulation(std::string_view             name,
                                                            const FlightConfigurationId& id) const
    {
        std::shared_ptr<Simulation> simulation = newSimulation();
        simulation->setFlightConfigurationId(id);
        simulation->setName(name);
        return simulation;
    }

    /// The names of the simulations of the document, each followed by a comma ("C,A,B,").
    [[nodiscard]] std::string simulationNames() const
    {
        std::string names;
        for (const std::shared_ptr<Simulation>& simulation : m_document->getSimulations())
        {
            names += simulation->getName();
            names += ',';
        }
        return names;
    }

    /// The simulations as DocumentProbe3.java's names() prints them: "A(UPTODATE,true),", the
    /// name, the status worked out anew and whether there is simulated data.
    [[nodiscard]] std::string simulationStates() const
    {
        std::string names;
        for (const std::shared_ptr<Simulation>& simulation : m_document->getSimulations())
        {
            names += std::format("{}({},{}),", simulation->getName(), name(simulation->getStatus()),
                                 simulation->getSimulatedData() != nullptr);
        }
        return names;
    }

private:
    /// The rocket of a new Estes Alpha III.
    [[nodiscard]] static std::unique_ptr<Rocket> takeRocket()
    {
        TestEstesAlphaIII alpha;
        return std::move(alpha.rocket);
    }

    /// The preferences of the simulations; declared first, since they must outlive them. On the
    /// heap, so that a simulation's pointer to them stays valid if the fixture is moved.
    std::unique_ptr<JavaTestPreferences> m_preferences = std::make_unique<JavaTestPreferences>();
    std::unique_ptr<OpenRocketDocument>  m_document;
};

}  // namespace QtRocket::Test
