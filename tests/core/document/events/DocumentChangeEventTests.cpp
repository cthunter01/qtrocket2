#include "QtRocket/document/events/DocumentChangeEvent.h"

#include <type_traits>
#include <variant>

#include <gtest/gtest.h>

#include "QtRocket/preferences/DocumentPreferences.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/Simulation.h"

// OpenRocket has no test of its two event classes (they are an EventObject and an empty subclass
// of it). These tests pin the one struct that stands for both.

namespace
{

using QtRocket::BodyTube;
using QtRocket::DocumentChangeEvent;
using QtRocket::DocumentPreferences;
using QtRocket::OpenRocketDocument;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Simulation;

using Kind = DocumentChangeEvent::Kind;

// An aggregate of a kind and a source: cheap to make and to pass by value.
static_assert(std::is_aggregate_v<DocumentChangeEvent>);
static_assert(std::is_trivially_copyable_v<DocumentChangeEvent>);
// The alternatives, in the order the class comment gives them.
static_assert(std::variant_size_v<DocumentChangeEvent::Source> == 5);
static_assert(
    std::is_same_v<std::variant_alternative_t<0, DocumentChangeEvent::Source>, std::monostate>);
static_assert(std::is_same_v<std::variant_alternative_t<1, DocumentChangeEvent::Source>,
                             OpenRocketDocument*>);
static_assert(
    std::is_same_v<std::variant_alternative_t<2, DocumentChangeEvent::Source>, RocketComponent*>);
static_assert(
    std::is_same_v<std::variant_alternative_t<3, DocumentChangeEvent::Source>, Simulation*>);
static_assert(std::is_same_v<std::variant_alternative_t<4, DocumentChangeEvent::Source>,
                             DocumentPreferences*>);

/// A pointer that stands for a document. OpenRocketDocument is only declared where the event is
/// defined, and an event only carries the pointer, so any address will do; it is never
/// dereferenced.
[[nodiscard]] OpenRocketDocument* documentAt(void* address) noexcept
{
    return static_cast<OpenRocketDocument*>(address);
}

TEST(DocumentChangeEvent, ADefaultEventIsADocumentChangeFromNothing)
{
    const DocumentChangeEvent event;
    EXPECT_EQ(event.kind, Kind::DOCUMENT);
    EXPECT_FALSE(event.isSimulationChange());
    EXPECT_TRUE(std::holds_alternative<std::monostate>(event.source));
    EXPECT_EQ(event.getDocument(), nullptr);
    EXPECT_EQ(event.getComponent(), nullptr);
    EXPECT_EQ(event.getSimulation(), nullptr);
    EXPECT_EQ(event.getPreferences(), nullptr);
}

TEST(DocumentChangeEvent, TheKindIsJavasClass)
{
    // Java: `event instanceof SimulationChangeEvent`.
    const DocumentChangeEvent document{.kind = Kind::DOCUMENT, .source = {}};
    const DocumentChangeEvent simulation{.kind = Kind::SIMULATION, .source = {}};
    EXPECT_FALSE(document.isSimulationChange());
    EXPECT_TRUE(simulation.isSimulationChange());
    // A simulation change from nothing: what a window fires after it ran a simulation.
    EXPECT_TRUE(std::holds_alternative<std::monostate>(simulation.source));
}

TEST(DocumentChangeEvent, AComponentAsTheSource)
{
    // A change event of the rocket, passed on.
    BodyTube                  tube;
    const DocumentChangeEvent event{.kind = Kind::DOCUMENT, .source = &tube};
    EXPECT_TRUE(std::holds_alternative<RocketComponent*>(event.source));
    EXPECT_EQ(event.getComponent(), &tube);
    EXPECT_EQ(event.getDocument(), nullptr);
    EXPECT_EQ(event.getSimulation(), nullptr);
    EXPECT_EQ(event.getPreferences(), nullptr);
}

TEST(DocumentChangeEvent, ASimulationAsTheSource)
{
    Rocket     rocket;
    Simulation simulation(rocket);

    // A change of the simulation is a document change; adding or removing it is a simulation
    // change. Both have the simulation as their source.
    const DocumentChangeEvent changed{.kind = Kind::DOCUMENT, .source = &simulation};
    const DocumentChangeEvent added{.kind = Kind::SIMULATION, .source = &simulation};
    EXPECT_EQ(changed.getSimulation(), &simulation);
    EXPECT_FALSE(changed.isSimulationChange());
    EXPECT_EQ(added.getSimulation(), &simulation);
    EXPECT_TRUE(added.isSimulationChange());
    EXPECT_EQ(added.getDocument(), nullptr);
    EXPECT_EQ(added.getComponent(), nullptr);
    EXPECT_EQ(added.getPreferences(), nullptr);
}

TEST(DocumentChangeEvent, TheDocumentAsTheSource)
{
    // Undo and redo reload the simulations (a simulation change); removing a decal image is a
    // document change.
    int                       place    = 0;
    OpenRocketDocument* const document = documentAt(&place);
    const DocumentChangeEvent reloaded{.kind = Kind::SIMULATION, .source = document};
    EXPECT_TRUE(std::holds_alternative<OpenRocketDocument*>(reloaded.source));
    EXPECT_EQ(reloaded.getDocument(), document);
    EXPECT_TRUE(reloaded.isSimulationChange());
    EXPECT_EQ(reloaded.getComponent(), nullptr);
    EXPECT_EQ(reloaded.getSimulation(), nullptr);
    EXPECT_EQ(reloaded.getPreferences(), nullptr);
}

TEST(DocumentChangeEvent, ThePreferencesAsTheSource)
{
    // OpenRocketDocumentPreferencesTest asserts this source for a changed preference.
    DocumentPreferences       preferences;
    const DocumentChangeEvent event{.kind = Kind::DOCUMENT, .source = &preferences};
    EXPECT_TRUE(std::holds_alternative<DocumentPreferences*>(event.source));
    EXPECT_EQ(event.getPreferences(), &preferences);
    EXPECT_EQ(event.getDocument(), nullptr);
    EXPECT_EQ(event.getComponent(), nullptr);
    EXPECT_EQ(event.getSimulation(), nullptr);
}

TEST(DocumentChangeEvent, EqualityIsTheKindAndTheSourceObject)
{
    BodyTube                  first;
    BodyTube                  second;
    DocumentPreferences       preferences;
    const DocumentChangeEvent event{.kind = Kind::DOCUMENT, .source = &first};
    EXPECT_TRUE(event == (DocumentChangeEvent{.kind = Kind::DOCUMENT, .source = &first}));
    EXPECT_FALSE(event == (DocumentChangeEvent{.kind = Kind::SIMULATION, .source = &first}));
    EXPECT_FALSE(event == (DocumentChangeEvent{.kind = Kind::DOCUMENT, .source = &second}));
    EXPECT_FALSE(event == (DocumentChangeEvent{.kind = Kind::DOCUMENT, .source = &preferences}));
    EXPECT_FALSE(event == DocumentChangeEvent{});
    EXPECT_TRUE(DocumentChangeEvent{} == DocumentChangeEvent{});

    // A copy is the same event.
    const DocumentChangeEvent copy = event;
    EXPECT_TRUE(copy == event);
    EXPECT_EQ(copy.getComponent(), &first);
}

}  // namespace
