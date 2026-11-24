#include "QtRocket/gui/main/OpenRocketClipboard.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include <QSignalSpy>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/ComponentCopy.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/document/SimulationCopy.h"
#include "QtRocket/gui/simulation/SimulationClipboard.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/Error.h"
#include "tree/ProbeDesign.h"

namespace QtRocket::Gui
{

namespace
{

using Test::ProbeDesign;
using OpenRocketClipboardTest = Test::GuiTest;

/// A copy of the component @p name of @p design.
[[nodiscard]] ComponentCopy copyOf(const ProbeDesign& design, std::string_view name)
{
    Result<ComponentCopy> copy = ComponentCopy::copyOf(
        design.document(), std::vector<const RocketComponent*>{&design.find(name)});
    EXPECT_TRUE(copy.has_value());
    return copy.has_value() ? std::move(*copy) : ComponentCopy();
}

/// A copy of a simulation of @p document named @p name (a new one, never in its list).
[[nodiscard]] SimulationCopy simulationCopyOf(OpenRocketDocument& document, std::string_view name)
{
    Simulation simulation(&document, document.getRocket());
    simulation.setName(name);
    return SimulationCopy::copyOf(std::vector<const Simulation*>{&simulation});
}

TEST_F(OpenRocketClipboardTest, StartsEmpty)
{
    const OpenRocketClipboard clipboard;

    EXPECT_TRUE(clipboard.isEmpty());
    EXPECT_TRUE(clipboard.components().empty());
    ASSERT_NE(clipboard.simulations(), nullptr);
    EXPECT_TRUE(clipboard.simulations()->empty());
}

TEST_F(OpenRocketClipboardTest, HoldsWhatItIsGivenAndTellsSo)
{
    const ProbeDesign   design;
    OpenRocketClipboard clipboard;
    const QSignalSpy    changed(&clipboard, &OpenRocketClipboard::changed);

    clipboard.setComponents(copyOf(design, "BT1"));

    EXPECT_EQ(changed.count(), 1);
    EXPECT_FALSE(clipboard.isEmpty());
    EXPECT_EQ(design.clipboard(clipboard.components()), "[BT1*[IT*[EB*], CR1*, Fins*]]");

    // Every setting is told, as Java's setClipboard() tells its listeners every time.
    clipboard.setComponents(copyOf(design, "NC"));
    EXPECT_EQ(changed.count(), 2);
    EXPECT_EQ(design.clipboard(clipboard.components()), "[NC*]");

    clipboard.clear();
    EXPECT_EQ(changed.count(), 3);
    EXPECT_TRUE(clipboard.isEmpty());
}

TEST_F(OpenRocketClipboardTest, KeepsItsCopiesWhenTheDocumentGoes)
{
    OpenRocketClipboard clipboard;
    {
        const ProbeDesign design;
        clipboard.setComponents(copyOf(design, "BT2"));
    }

    ASSERT_EQ(clipboard.components().size(), 1U);
    EXPECT_EQ(clipboard.components().getComponent(0).getName(), "BT2");
    EXPECT_EQ(clipboard.components().getComponent(0).getChild(0).getName(), "Chute");
}

TEST_F(OpenRocketClipboardTest, HoldsSimulationsAndTellsSo)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    OpenRocketClipboard clipboard;
    const QSignalSpy    changed(&clipboard, &OpenRocketClipboard::changed);
    // The simulations table sees it as its SimulationClipboard.
    SimulationClipboard& simulations = clipboard;

    simulations.setSimulations(simulationCopyOf(*document, "Simulation 1"));
    EXPECT_EQ(changed.count(), 1);
    EXPECT_FALSE(clipboard.isEmpty());
    ASSERT_EQ(clipboard.simulations()->size(), 1U);
    EXPECT_EQ(clipboard.simulations()->getEntry(0).name, "Simulation 1");

    // A paste holds on to what it pastes while the clipboard changes.
    const std::shared_ptr<const SimulationCopy> held = simulations.simulations();
    clipboard.setSimulations(simulationCopyOf(*document, "Simulation 2"));
    EXPECT_EQ(changed.count(), 2);
    EXPECT_EQ(held->getEntry(0).name, "Simulation 1");
    EXPECT_EQ(clipboard.simulations()->getEntry(0).name, "Simulation 2");

    clipboard.clear();
    EXPECT_EQ(changed.count(), 3);
    EXPECT_TRUE(clipboard.isEmpty());
    EXPECT_TRUE(clipboard.simulations()->empty());
}

TEST_F(OpenRocketClipboardTest, HoldsComponentsOrSimulationsNeverBoth)
{
    // Java: setClipboard(components) sets the simulations to null, setClipboard(simulations)
    // clears the components.
    const ProbeDesign   design;
    OpenRocketClipboard clipboard;

    clipboard.setComponents(copyOf(design, "NC"));
    clipboard.setSimulations(simulationCopyOf(design.document(), "Simulation 1"));
    EXPECT_TRUE(clipboard.components().empty());
    EXPECT_EQ(clipboard.simulations()->size(), 1U);

    clipboard.setComponents(copyOf(design, "NC"));
    EXPECT_EQ(clipboard.components().size(), 1U);
    EXPECT_TRUE(clipboard.simulations()->empty());
    EXPECT_FALSE(clipboard.isEmpty());
}

TEST_F(OpenRocketClipboardTest, KeepsItsSimulationsWhenTheDocumentGoes)
{
    OpenRocketClipboard clipboard;
    {
        const std::unique_ptr<OpenRocketDocument> document =
            OpenRocketDocumentFactory::createNewRocket();
        clipboard.setSimulations(simulationCopyOf(*document, "Detached"));
    }

    ASSERT_EQ(clipboard.simulations()->size(), 1U);
    const std::unique_ptr<OpenRocketDocument> other = OpenRocketDocumentFactory::createNewRocket();
    const std::shared_ptr<Simulation> pasted = clipboard.simulations()->makeSimulation(0, *other);
    EXPECT_EQ(pasted->getName(), "Detached");
    EXPECT_EQ(pasted->getDocument(), other.get());
}

TEST_F(OpenRocketClipboardTest, TheApplicationHasOne)
{
    OpenRocketClipboard& application = OpenRocketClipboard::instance();

    EXPECT_EQ(&OpenRocketClipboard::instance(), &application);
    // Process-wide: no test may leave anything on it.
    EXPECT_TRUE(application.isEmpty());
    const ProbeDesign design;
    application.setComponents(copyOf(design, "NC"));
    EXPECT_FALSE(OpenRocketClipboard::instance().isEmpty());
    application.clear();
    EXPECT_TRUE(OpenRocketClipboard::instance().isEmpty());
    // Simulations count too.
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    application.setSimulations(simulationCopyOf(*document, "Simulation 1"));
    EXPECT_FALSE(OpenRocketClipboard::instance().isEmpty());
    application.clear();
    EXPECT_TRUE(OpenRocketClipboard::instance().isEmpty());
}

}  // namespace

}  // namespace QtRocket::Gui
