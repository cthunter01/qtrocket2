// The operations on the simulations of the "Flight simulations" tab (Java: SimulationPanel's
// newSimulation(), duplicateSimulation(), copySimulationsAction() with copySimulationValues(),
// pasteSimulationsAction(), deleteSimulations() with verifyDeleteSimulation(), and
// CutSimulationAction). OpenRocket has no JUnit test of them. The names, places and undo texts are
// those of OpenRocket's own panel in the sim-tab scout's transcript (SimTabProbe's OP lines, on
// "A simple model rocket": "duplicate 0,2; selected=[5, 6]", "Duplicate Simulation 9", "paste
// copies of 0 and 3 at 1", "pasted into a new design", "Delete Simulation 1", ...) where QtRocket
// does as OpenRocket; the selections and the places in a sorted table are QtRocket's fix of
// OpenRocket bug 24, a Paste without a selected row and into a design without simulations its fix
// of bug 27, and a paste into another design its fix of bug 22 (the transcript's FRESH-RENAME:
// OpenRocket marks the design the simulation came from unsaved).

#include "QtRocket/gui/simulation/SimulationEditing.h"

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QClipboard>
#include <QGuiApplication>
#include <QPointer>
#include <QSignalSpy>
#include <QString>
#include <QStringList>

#include <gtest/gtest.h>

#include "ForwardingUserPrompts.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/document/SimulationCopy.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/simulation/SimulationTableCsvExport.h"
#include "QtRocket/gui/simulation/SimulationTableModel.h"
#include "QtRocket/gui/simulation/SimulationTableView.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/TableSortKeys.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"
#include "simulation/SimulationTablePins.h"
#include "simulation/SimulationTableTestSupport.h"

class QWidget;

namespace QtRocket::Gui
{

namespace
{

using Column    = SimulationColumns::Column;
using Names     = std::vector<std::string>;
using SortKey   = TableSortKeys::SortKey;
using SortOrder = TableSortKeys::SortOrder;
using Status    = Simulation::Status;
using Gui::Test::ScriptedUserPrompts;
using Gui::Test::SimulationTableFixture;
using Gui::Test::TestGuiServices;
using SimulationEditingTest = Gui::Test::GuiTest;

/// "A simple model rocket" as loaded: five simulations with stored data ("Simulation 1",
/// "Simulation 2", "Simulation 3 - too short delay", "Simulation 4", "Simulation 5").
[[nodiscard]] std::unique_ptr<OpenRocketDocument> simpleModelRocket()
{
    return Gui::Test::loadSimulationExample("A simple model rocket.ork");
}

/// A design's simulations table with its editing, as SimulationPanel holds them: the table (its
/// model under QAbstractItemModelTester, not shown) and a SimulationEditing, a child of its view,
/// through @p clipboard, with @p services' preferences and scripted questions.
class EditedTable
{
public:
    EditedTable(std::unique_ptr<OpenRocketDocument> document, OpenRocketClipboard& clipboard,
                TestGuiServices& services)
      : m_table(std::move(document)),
        m_editing(makeChild<SimulationEditing>(&m_table.view(), m_table.view(), clipboard,
                                               services.preferences(), services.prompts()))
    {
    }

    [[nodiscard]] OpenRocketDocument&   document() const noexcept { return m_table.document(); }
    [[nodiscard]] SimulationTableModel& model() const noexcept
    {
        return m_table.view().tableModel();
    }
    [[nodiscard]] SimulationTableView& view() const noexcept { return m_table.view(); }
    [[nodiscard]] SimulationEditing&   editing() const noexcept { return *m_editing; }

    /// Selects the simulations of the table's rows @p rows (and nothing else).
    void selectRows(std::initializer_list<int> rows) const
    {
        std::vector<std::shared_ptr<Simulation>> simulations;
        for (const int row : rows)
        {
            simulations.push_back(model().simulation(row));
        }
        m_table.view().selectSimulations(simulations);
    }

    /// The names of the document's simulations, in its order.
    [[nodiscard]] Names documentNames() const
    {
        Names names;
        for (const std::shared_ptr<Simulation>& simulation : document().getSimulations())
        {
            names.push_back(simulation->getName());
        }
        return names;
    }

    /// The names of the table's rows, from the top.
    [[nodiscard]] Names rowNames() const { return m_table.rowNames(); }
    /// The names of the selected simulations, from the top.
    [[nodiscard]] Names selectedNames() const { return m_table.selectedNames(); }

    /// The document's simulation named @p name; null and a failure when it has none.
    [[nodiscard]] std::shared_ptr<Simulation> named(std::string_view name) const
    {
        for (const std::shared_ptr<Simulation>& simulation : document().getSimulations())
        {
            if (simulation->getName() == name)
            {
                return simulation;
            }
        }
        ADD_FAILURE() << "no simulation " << name;
        return nullptr;
    }

    /// The undo and redo descriptions ("null" for none, as the probe writes them).
    [[nodiscard]] std::string undoText() const
    {
        return document().getUndoDescription().value_or("null");
    }
    [[nodiscard]] std::string redoText() const
    {
        return document().getRedoDescription().value_or("null");
    }

private:
    SimulationTableFixture m_table;
    SimulationEditing*     m_editing;
};

/// The system clipboard's text, which is then cleared (a test leaves nothing there).
[[nodiscard]] QString takeClipboardText()
{
    QString text = QGuiApplication::clipboard()->text();
    QGuiApplication::clipboard()->clear();
    return text;
}

/// The names of the simulations on @p clipboard.
[[nodiscard]] Names clipboardNames(const OpenRocketClipboard& clipboard)
{
    Names                                       names;
    const std::shared_ptr<const SimulationCopy> copy = clipboard.simulations();
    for (std::size_t i = 0; i < copy->size(); ++i)
    {
        names.push_back(copy->getEntry(i).name);
    }
    return names;
}

/// The deletion question as ScriptedUserPrompts records it, for @p count simulations.
[[nodiscard]] ScriptedUserPrompts::Prompt deletionQuestion(int count)
{
    return {.kind  = ScriptedUserPrompts::Kind::DELETE_SIMULATIONS,
            .title = QString(),
            .texts = QStringList{QString::number(count)}};
}

/// Whether @p a and @p b are the same recorded question.
[[nodiscard]] bool sameQuestion(const ScriptedUserPrompts::Prompt& a,
                                const ScriptedUserPrompts::Prompt& b)
{
    return a.kind == b.kind && a.title == b.title && a.texts == b.texts;
}

// ============================================================================ New

TEST_F(SimulationEditingTest, NewAddsTheNextSimulationAndSelectsIt)
{
    TestGuiServices     services;
    OpenRocketClipboard clipboard;
    const EditedTable   table(simpleModelRocket(), clipboard, services);
    // A new simulation takes the preferences' launch conditions (Java's
    // DefaultSimulationOptionFactory).
    services.preferences().setLaunchRodLength(2.5);
    table.document().setSaved(true);

    const std::shared_ptr<Simulation> added = table.editing().newSimulation();
    ASSERT_NE(added, nullptr);
    EXPECT_EQ(added->getName(), "Simulation 6");
    EXPECT_EQ(table.document().getSimulationIndex(*added), std::optional<std::size_t>(5));
    EXPECT_EQ(table.undoText(), "Add Simulation 6");
    EXPECT_EQ(table.selectedNames(), Names{"Simulation 6"});
    EXPECT_EQ(added->getStatus(), Status::NOT_SIMULATED);
    EXPECT_EQ(added->getFlightConfigurationId(),
              table.document().getRocket().getSelectedConfiguration().getId());
    EXPECT_DOUBLE_EQ(added->getOptions().getLaunchRodLength(), 2.5);
    EXPECT_EQ(added->getDocument(), &table.document());
    EXPECT_FALSE(table.document().isSaved());

    // One undo step.
    table.document().undo();
    EXPECT_EQ(table.document().getSimulationCount(), 5U);
    EXPECT_EQ(table.redoText(), "Add Simulation 6");
    table.document().redo();
    EXPECT_EQ(table.documentNames().back(), "Simulation 6");
}

TEST_F(SimulationEditingTest, NewSelectsTheNewSimulationInASortedTable)
{
    // OpenRocket bug 24, fixed: Java selects the new simulation's list index (5) as a view row,
    // here "Simulation 1" in a table sorted by name descending.
    TestGuiServices     services;
    OpenRocketClipboard clipboard;
    const EditedTable   table(simpleModelRocket(), clipboard, services);
    table.model().setSortKeys({SortKey{.column = SimulationColumns::indexOf(Column::NAME),
                                       .order  = SortOrder::DESCENDING}});

    ASSERT_NE(table.editing().newSimulation(), nullptr);
    EXPECT_EQ(table.rowNames().front(), "Simulation 6");
    EXPECT_EQ(table.selectedNames(), Names{"Simulation 6"});
    EXPECT_EQ(table.view().currentIndex().row(), 0);
}

// ====================================================================== Duplicate

TEST_F(SimulationEditingTest, DuplicatesTheSelectionAtTheEnd)
{
    TestGuiServices     services;
    OpenRocketClipboard clipboard;
    const EditedTable   table(simpleModelRocket(), clipboard, services);
    table.selectRows({0, 2});
    const std::shared_ptr<Simulation> third = table.model().simulation(2);

    // The transcript's "duplicate 0,2; selected=[5, 6]".
    EXPECT_TRUE(table.editing().duplicate());
    EXPECT_EQ(
        table.documentNames(),
        (Names{"Simulation 1", "Simulation 2", "Simulation 3 - too short delay", "Simulation 4",
               "Simulation 5", "Simulation 6", "Simulation 3 - too short delay"}));
    EXPECT_EQ(table.undoText(), "Duplicate simulations");
    EXPECT_EQ(table.selectedNames(), (Names{"Simulation 6", "Simulation 3 - too short delay"}));
    const std::shared_ptr<Simulation> duplicate = table.document().getSimulation(6);
    EXPECT_NE(duplicate, third);
    EXPECT_EQ(duplicate->getStatus(), Status::NOT_SIMULATED);
    EXPECT_EQ(duplicate->getFlightConfigurationId(), third->getFlightConfigurationId());
    EXPECT_DOUBLE_EQ(duplicate->getOptions().getLaunchRodLength(),
                     third->getOptions().getLaunchRodLength());
    EXPECT_EQ(duplicate->getDocument(), &table.document());
    // Nothing on the clipboard.
    EXPECT_TRUE(clipboard.isEmpty());

    table.document().undo();
    EXPECT_EQ(table.document().getSimulationCount(), 5U);
    table.document().redo();
    EXPECT_EQ(table.document().getSimulationCount(), 7U);
}

TEST_F(SimulationEditingTest, DuplicateRenamesTheNumberedNamesOnly)
{
    TestGuiServices                   services;
    OpenRocketClipboard               clipboard;
    const EditedTable                 table(simpleModelRocket(), clipboard, services);
    const std::shared_ptr<Simulation> second = table.model().simulation(1);

    // The transcript's "sim1 named 'Simulation 9', duplicated".
    second->setName("Simulation 9");
    table.selectRows({1});
    EXPECT_TRUE(table.editing().duplicate());
    EXPECT_EQ(table.documentNames().back(), "Simulation 10");
    EXPECT_EQ(table.undoText(), "Duplicate Simulation 9");

    // A name with trailing spaces is a numbered one too ("Simulation [0-9]+ *"), which
    // getNextSimulationName() does not count; another name stays.
    second->setName("Simulation 9 ");
    table.selectRows({table.model().rowOf(*second), 2});
    EXPECT_TRUE(table.editing().duplicate());
    EXPECT_EQ(table.documentNames().at(6), "Simulation 11");
    EXPECT_EQ(table.documentNames().at(7), "Simulation 3 - too short delay");
    EXPECT_EQ(table.undoText(), "Duplicate simulations");
}

TEST_F(SimulationEditingTest, DuplicateSelectsTheDuplicatesInASortedTable)
{
    // OpenRocket bug 24, fixed: the duplicates themselves, wherever the order shows them.
    TestGuiServices     services;
    OpenRocketClipboard clipboard;
    const EditedTable   table(simpleModelRocket(), clipboard, services);
    table.model().setSortKeys({SortKey{.column = SimulationColumns::indexOf(Column::APOGEE),
                                       .order  = SortOrder::DESCENDING}});
    table.selectRows({0, 4});
    const Names original = table.selectedNames();

    EXPECT_TRUE(table.editing().duplicate());
    // Without data, a missing apogee comes last descending.
    EXPECT_EQ(table.rowNames().at(5), "Simulation 6");
    EXPECT_EQ(table.selectedNames(), (Names{"Simulation 6", "Simulation 7"}));
    EXPECT_EQ(table.documentNames().at(5), "Simulation 6");
    EXPECT_EQ(original, (Names{"Simulation 5", "Simulation 1"}));
}

// =========================================================================== Copy

TEST_F(SimulationEditingTest, CopyPutsTheSimulationsAndTheirTextOnTheClipboards)
{
    TestGuiServices     services;
    OpenRocketClipboard clipboard;
    const EditedTable   table(simpleModelRocket(), clipboard, services);
    const QSignalSpy    changed(&clipboard, &OpenRocketClipboard::changed);
    table.document().setSaved(true);
    table.selectRows({1, 3});
    const std::vector<std::shared_ptr<Simulation>> selected = table.view().selectedSimulations();

    EXPECT_TRUE(table.editing().copy());
    EXPECT_EQ(changed.count(), 1);
    EXPECT_EQ(clipboardNames(clipboard), (Names{"Simulation 2", "Simulation 4"}));
    EXPECT_TRUE(clipboard.components().empty());
    // Java puts the rows' text on the system clipboard as well.
    const QString text = takeClipboardText();
    EXPECT_EQ(text,
              QtStrings::fromUtf8(SimulationTableCsvExport::copyText(table.model(), selected)));
    EXPECT_TRUE(text.startsWith(QStringLiteral("Status\tWarnings\tName\t")));
    EXPECT_EQ(text.count(QStringLiteral("\n")), 2);
    // No undo step, no change.
    EXPECT_EQ(table.undoText(), "null");
    EXPECT_TRUE(table.document().isSaved());
    EXPECT_EQ(table.selectedNames(), (Names{"Simulation 2", "Simulation 4"}));
}

TEST_F(SimulationEditingTest, CopyOfEveryRowIsOpenRocketsText)
{
    const Gui::Test::SimulationTablePins* pins = Gui::Test::simulationTablePinsOrFail();
    ASSERT_NE(pins, nullptr);
    const Gui::Test::SimulationTablePinDesign* pin =
        pins->findDesign("example_a_simple_model_rocket");
    ASSERT_NE(pin, nullptr);
    ASSERT_TRUE(pin->copy.has_value());
    TestGuiServices     services;
    OpenRocketClipboard clipboard;
    const EditedTable   table(simpleModelRocket(), clipboard, services);
    table.selectRows({0, 1, 2, 3, 4});

    EXPECT_TRUE(table.editing().copy());
    EXPECT_EQ(QtStrings::toUtf8(takeClipboardText()), pin->copy.value_or(""));
}

// ========================================================================== Paste

TEST_F(SimulationEditingTest, PastesAfterTheTopmostSelectedSimulation)
{
    TestGuiServices     services;
    OpenRocketClipboard clipboard;
    const EditedTable   table(simpleModelRocket(), clipboard, services);
    table.selectRows({0, 3});
    ASSERT_TRUE(table.editing().copy());
    QGuiApplication::clipboard()->clear();
    const QSignalSpy changed(&clipboard, &OpenRocketClipboard::changed);

    // The transcript's "paste copies of 0 and 3 at 1": numbered names become the next ones.
    table.selectRows({0, 2});
    EXPECT_TRUE(table.editing().canPaste());
    EXPECT_TRUE(table.editing().paste());
    EXPECT_EQ(table.documentNames(),
              (Names{"Simulation 1", "Simulation 6", "Simulation 7", "Simulation 2",
                     "Simulation 3 - too short delay", "Simulation 4", "Simulation 5"}));
    EXPECT_EQ(table.undoText(), "Duplicate simulations");
    EXPECT_EQ(table.selectedNames(), (Names{"Simulation 6", "Simulation 7"}));
    const std::shared_ptr<Simulation> pasted = table.named("Simulation 7");
    ASSERT_NE(pasted, nullptr);
    EXPECT_EQ(pasted->getStatus(), Status::NOT_SIMULATED);
    EXPECT_EQ(pasted->getFlightConfigurationId(),
              table.named("Simulation 4")->getFlightConfigurationId());
    EXPECT_EQ(pasted->getDocument(), &table.document());

    // The clipboard keeps them: they paste again, after the topmost selected one.
    EXPECT_EQ(changed.count(), 0);
    EXPECT_TRUE(table.editing().paste());
    EXPECT_EQ(table.document().getSimulationCount(), 9U);
    EXPECT_EQ(table.documentNames().at(2), "Simulation 8");
    EXPECT_EQ(table.documentNames().at(3), "Simulation 9");

    // One undo step each.
    table.document().undo();
    EXPECT_EQ(table.document().getSimulationCount(), 7U);
    table.document().undo();
    EXPECT_EQ(table.document().getSimulationCount(), 5U);
    EXPECT_EQ(table.redoText(), "Duplicate simulations");
}

TEST_F(SimulationEditingTest, PastesOneSimulationUnderItsOwnName)
{
    TestGuiServices     services;
    OpenRocketClipboard clipboard;
    const EditedTable   table(simpleModelRocket(), clipboard, services);
    table.selectRows({2});
    ASSERT_TRUE(table.editing().copy());
    QGuiApplication::clipboard()->clear();

    table.selectRows({4});
    EXPECT_TRUE(table.editing().paste());
    EXPECT_EQ(table.documentNames().back(), "Simulation 3 - too short delay");
    EXPECT_EQ(table.document().getSimulationCount(), 6U);
    EXPECT_EQ(table.undoText(), "Duplicate Simulation 3 - too short delay");
    EXPECT_EQ(table.selectedNames(), Names{"Simulation 3 - too short delay"});
    EXPECT_EQ(table.view().currentIndex().row(), 5);
}

TEST_F(SimulationEditingTest, PastesWithoutASelectedRowAtTheEnd)
{
    // OpenRocket bug 27, fixed: Java's Paste needs a selected row.
    TestGuiServices     services;
    OpenRocketClipboard clipboard;
    const EditedTable   table(simpleModelRocket(), clipboard, services);
    table.selectRows({0});
    ASSERT_TRUE(table.editing().copy());
    QGuiApplication::clipboard()->clear();

    table.view().clearSelection();
    ASSERT_FALSE(table.editing().hasSelection());
    EXPECT_TRUE(table.editing().canPaste());
    EXPECT_TRUE(table.editing().paste());
    EXPECT_EQ(table.documentNames().back(), "Simulation 6");
    EXPECT_EQ(table.selectedNames(), Names{"Simulation 6"});
    EXPECT_EQ(table.undoText(), "Duplicate Simulation 1");
}

TEST_F(SimulationEditingTest, PastesIntoADesignWithoutSimulations)
{
    // OpenRocket bug 27, fixed (Java's Paste needs a selected row, which an empty list cannot
    // have); the transcript's "pasted into a new design": the configurations the copies name are
    // made in the rocket, inside the undo step, without motors.
    TestGuiServices     services;
    OpenRocketClipboard clipboard;
    {
        const EditedTable source(simpleModelRocket(), clipboard, services);
        source.selectRows({0, 3});
        ASSERT_TRUE(source.editing().copy());
        QGuiApplication::clipboard()->clear();
    }
    const EditedTable table(OpenRocketDocumentFactory::createNewRocket(), clipboard, services);
    const int         configurations = table.document().getRocket().getFlightConfigurationCount();
    EXPECT_EQ(configurations, 0);
    EXPECT_TRUE(table.editing().canPaste());

    EXPECT_TRUE(table.editing().paste());
    EXPECT_EQ(table.documentNames(), (Names{"Simulation 1", "Simulation 2"}));
    EXPECT_EQ(table.rowNames(), (Names{"Simulation 1", "Simulation 2"}));
    EXPECT_EQ(table.selectedNames(), (Names{"Simulation 1", "Simulation 2"}));
    EXPECT_EQ(table.document().getRocket().getFlightConfigurationCount(), 2);
    EXPECT_EQ(table.model().status(0), Status::CANT_RUN);
    EXPECT_EQ(table.model().cellText(0, Column::CONFIGURATION), QStringLiteral("[No motors]"));
    EXPECT_EQ(table.undoText(), "Duplicate simulations");

    table.document().undo();
    EXPECT_EQ(table.document().getSimulationCount(), 0U);
    EXPECT_EQ(table.document().getRocket().getFlightConfigurationCount(), 0);
}

TEST_F(SimulationEditingTest, PastesAfterTheSelectedSimulationOfASortedTable)
{
    // OpenRocket bug 24, fixed: sorted by name descending, the second row is "Simulation 4" (the
    // list's fourth); Java pastes at its view row + 1 taken as a list index (after "Simulation 2")
    // and selects view rows 2 and 3 (the transcript's SORTED-PASTE).
    TestGuiServices     services;
    OpenRocketClipboard clipboard;
    const EditedTable   table(simpleModelRocket(), clipboard, services);
    table.selectRows({0, 1});
    ASSERT_TRUE(table.editing().copy());
    QGuiApplication::clipboard()->clear();
    table.model().setSortKeys({SortKey{.column = SimulationColumns::indexOf(Column::NAME),
                                       .order  = SortOrder::DESCENDING}});
    table.selectRows({1});
    ASSERT_EQ(table.selectedNames(), Names{"Simulation 4"});

    EXPECT_TRUE(table.editing().paste());
    EXPECT_EQ(table.documentNames(),
              (Names{"Simulation 1", "Simulation 2", "Simulation 3 - too short delay",
                     "Simulation 4", "Simulation 6", "Simulation 7", "Simulation 5"}));
    EXPECT_EQ(table.selectedNames(), (Names{"Simulation 7", "Simulation 6"}));
    EXPECT_EQ(table.rowNames().front(), "Simulation 7");
}

TEST_F(SimulationEditingTest, APastedSimulationBelongsToItsNewDesign)
{
    // OpenRocket bug 22, fixed: the transcript's FRESH-RENAME and FRESH-OPTIONS (Java: the design
    // the simulation came from becomes unsaved, the one it was pasted into stays saved).
    TestGuiServices     services;
    OpenRocketClipboard clipboard;
    const EditedTable   source(simpleModelRocket(), clipboard, services);
    const EditedTable   target(simpleModelRocket(), clipboard, services);
    source.document().setSaved(true);
    target.document().setSaved(true);
    source.selectRows({0});
    ASSERT_TRUE(source.editing().copy());
    QGuiApplication::clipboard()->clear();
    target.selectRows({4});
    ASSERT_TRUE(target.editing().paste());
    EXPECT_TRUE(source.document().isSaved());
    EXPECT_FALSE(target.document().isSaved());
    target.document().setSaved(true);
    const std::shared_ptr<Simulation> pasted = target.named("Simulation 6");
    ASSERT_NE(pasted, nullptr);
    EXPECT_EQ(pasted->getDocument(), &target.document());

    pasted->setName("Renamed");
    EXPECT_TRUE(source.document().isSaved());
    EXPECT_FALSE(target.document().isSaved());
    target.document().setSaved(true);
    pasted->getOptions().setLaunchRodLength(pasted->getOptions().getLaunchRodLength() + 0.5);
    EXPECT_TRUE(source.document().isSaved());
    EXPECT_FALSE(target.document().isSaved());
    EXPECT_EQ(target.model().cellText(5, Column::NAME), QStringLiteral("Renamed"));
}

TEST_F(SimulationEditingTest, PastesAfterTheDesignItCameFromWasClosed)
{
    TestGuiServices     services;
    OpenRocketClipboard clipboard;
    {
        const EditedTable source(simpleModelRocket(), clipboard, services);
        source.selectRows({2});
        ASSERT_TRUE(source.editing().copy());
        QGuiApplication::clipboard()->clear();
    }
    const EditedTable table(simpleModelRocket(), clipboard, services);
    table.selectRows({0});
    EXPECT_TRUE(table.editing().paste());
    EXPECT_EQ(table.documentNames().at(1), "Simulation 3 - too short delay");
    EXPECT_EQ(table.document().getSimulation(1)->getFlightConfigurationId(),
              table.document().getSimulation(3)->getFlightConfigurationId());
}

// ========================================================================= Delete

TEST_F(SimulationEditingTest, DeleteAsksThenDeletes)
{
    TestGuiServices     services;
    OpenRocketClipboard clipboard;
    const EditedTable   table(simpleModelRocket(), clipboard, services);
    // OpenRocket asks by default (ApplicationPreferences.getConfirmSimDeletion()).
    ASSERT_TRUE(services.preferences().getConfirmSimDeletion());
    services.prompts().answerDeleteSimulations({.confirmed = true, .doNotAskAgain = false});
    table.selectRows({0});

    // The transcript's "delete sim 0".
    EXPECT_TRUE(table.editing().deleteSelection());
    ASSERT_EQ(services.prompts().prompts().size(), 1U);
    EXPECT_TRUE(sameQuestion(services.prompts().prompts().front(), deletionQuestion(1)));
    EXPECT_EQ(table.documentNames(), (Names{"Simulation 2", "Simulation 3 - too short delay",
                                            "Simulation 4", "Simulation 5"}));
    EXPECT_EQ(table.undoText(), "Delete Simulation 1");
    EXPECT_TRUE(table.selectedNames().empty());
    EXPECT_TRUE(services.preferences().getConfirmSimDeletion());

    table.document().undo();
    EXPECT_EQ(table.document().getSimulationCount(), 5U);
    EXPECT_EQ(table.redoText(), "Delete Simulation 1");
    table.document().redo();
    EXPECT_EQ(table.document().getSimulationCount(), 4U);
}

TEST_F(SimulationEditingTest, DeletesSeveralInOneStep)
{
    TestGuiServices     services;
    OpenRocketClipboard clipboard;
    const EditedTable   table(simpleModelRocket(), clipboard, services);
    services.prompts().answerDeleteSimulations({.confirmed = true, .doNotAskAgain = false});
    table.selectRows({0, 1, 4});

    EXPECT_TRUE(table.editing().deleteSelection());
    ASSERT_EQ(services.prompts().prompts().size(), 1U);
    EXPECT_TRUE(sameQuestion(services.prompts().prompts().front(), deletionQuestion(3)));
    EXPECT_EQ(table.documentNames(), (Names{"Simulation 3 - too short delay", "Simulation 4"}));
    EXPECT_EQ(table.undoText(), "Delete simulations");
    table.document().undo();
    EXPECT_EQ(table.document().getSimulationCount(), 5U);
}

TEST_F(SimulationEditingTest, ACancelledDeleteDeletesNothing)
{
    TestGuiServices     services;
    OpenRocketClipboard clipboard;
    const EditedTable   table(simpleModelRocket(), clipboard, services);
    services.prompts().answerDeleteSimulations({.confirmed = false, .doNotAskAgain = false});
    table.document().setSaved(true);
    table.selectRows({0});

    EXPECT_FALSE(table.editing().deleteSelection());
    EXPECT_EQ(table.document().getSimulationCount(), 5U);
    EXPECT_EQ(table.undoText(), "null");
    EXPECT_TRUE(table.document().isSaved());
    EXPECT_EQ(table.selectedNames(), Names{"Simulation 1"});
    EXPECT_TRUE(services.preferences().getConfirmSimDeletion());
}

TEST_F(SimulationEditingTest, DoNotAskAgainCountsWhateverTheAnswer)
{
    TestGuiServices     services;
    OpenRocketClipboard clipboard;
    const EditedTable   table(simpleModelRocket(), clipboard, services);
    // verifyDeleteSimulation(): the box is stored also with Cancel.
    services.prompts().answerDeleteSimulations({.confirmed = false, .doNotAskAgain = true});
    table.selectRows({0});

    EXPECT_FALSE(table.editing().deleteSelection());
    EXPECT_EQ(table.document().getSimulationCount(), 5U);
    EXPECT_FALSE(services.preferences().getConfirmSimDeletion());

    // No question any more (the script has no answer for one).
    EXPECT_TRUE(table.editing().deleteSelection());
    EXPECT_EQ(table.document().getSimulationCount(), 4U);
    EXPECT_EQ(services.prompts().prompts().size(), 1U);
}

TEST_F(SimulationEditingTest, DeletesWithoutAskingWhenTheQuestionIsOff)
{
    TestGuiServices     services;
    OpenRocketClipboard clipboard;
    const EditedTable   table(simpleModelRocket(), clipboard, services);
    services.preferences().setConfirmSimDeletion(false);
    table.selectRows({3, 4});

    EXPECT_TRUE(table.editing().deleteSelection());
    EXPECT_TRUE(services.prompts().prompts().empty());
    EXPECT_EQ(table.document().getSimulationCount(), 3U);
    EXPECT_EQ(table.undoText(), "Delete simulations");
}

/// The deletion question of a test that does something while it is asked (its event loop), then
/// answers OK with "Do not ask me again" ticked.
class DuringTheQuestion final : public Gui::Test::ForwardingUserPrompts
{
public:
    DuringTheQuestion(UserPrompts& delegate, std::function<void()> action)
      : ForwardingUserPrompts(delegate), m_action(std::move(action))
    {
    }

    [[nodiscard]] DeleteSimulationsAnswer askDeleteSimulations(QWidget* /*parent*/,
                                                               std::size_t /*count*/) override
    {
        m_action();
        return {.confirmed = true, .doNotAskAgain = true};
    }

private:
    std::function<void()> m_action;
};

TEST_F(SimulationEditingTest, NothingMoreIsDoneWhenTheTableGoesDuringTheQuestion)
{
    TestGuiServices                           services;
    OpenRocketClipboard                       clipboard;
    const std::unique_ptr<OpenRocketDocument> document = simpleModelRocket();
    ASSERT_NE(document, nullptr);
    SimulationTableModel model(*document, Gui::Test::simulationExamplePreferences(), nullptr);
    auto                 view = std::make_unique<SimulationTableView>(model, nullptr);
    DuringTheQuestion    prompts(services.prompts(), [&view] { view = nullptr; });
    auto* const          editing =
        makeChild<SimulationEditing>(view.get(), *view, clipboard, services.preferences(), prompts);
    const QPointer<SimulationEditing> alive(editing);
    view->selectSimulations({model.simulation(0)});

    EXPECT_FALSE(editing->deleteSelection());
    EXPECT_TRUE(alive.isNull());
    EXPECT_EQ(document->getSimulationCount(), 5U);
    EXPECT_FALSE(document->getUndoDescription().has_value());
    // The box counts all the same.
    EXPECT_FALSE(services.preferences().getConfirmSimDeletion());
}

TEST_F(SimulationEditingTest, ASimulationThatLeftDuringTheQuestionIsNotDeletedAgain)
{
    TestGuiServices                     services;
    OpenRocketClipboard                 clipboard;
    std::unique_ptr<OpenRocketDocument> document = simpleModelRocket();
    ASSERT_NE(document, nullptr);
    const std::shared_ptr<Simulation> first  = document->getSimulation(0);
    OpenRocketDocument&               design = *document;
    DuringTheQuestion      prompts(services.prompts(),
                                   [&design, &first] { design.removeSimulation(*first); });
    SimulationTableFixture table(std::move(document));
    auto* editing = makeChild<SimulationEditing>(&table.view(), table.view(), clipboard,
                                                 services.preferences(), prompts);
    table.view().selectSimulations({first, table.model().simulation(1)});

    // The other one goes, in a step of its own.
    EXPECT_TRUE(editing->deleteSelection());
    EXPECT_EQ(table.rowNames(),
              (Names{"Simulation 3 - too short delay", "Simulation 4", "Simulation 5"}));
    EXPECT_EQ(table.document().getUndoDescription(),
              std::optional<std::string>("Delete Simulation 2"));
}

// ============================================================================ Cut

TEST_F(SimulationEditingTest, CutCopiesThenDeletes)
{
    TestGuiServices     services;
    OpenRocketClipboard clipboard;
    const EditedTable   table(simpleModelRocket(), clipboard, services);
    services.prompts().answerDeleteSimulations({.confirmed = true, .doNotAskAgain = false});
    table.selectRows({2});

    EXPECT_TRUE(table.editing().cut());
    EXPECT_EQ(clipboardNames(clipboard), Names{"Simulation 3 - too short delay"});
    EXPECT_TRUE(takeClipboardText().contains(QStringLiteral("\tSimulation 3 - too short delay\t")));
    EXPECT_EQ(table.document().getSimulationCount(), 4U);
    // Java's Cut has no undo text of its own: Delete's.
    EXPECT_EQ(table.undoText(), "Delete Simulation 3 - too short delay");

    // And pastes back.
    table.selectRows({1});
    EXPECT_TRUE(table.editing().paste());
    EXPECT_EQ(table.documentNames().at(2), "Simulation 3 - too short delay");
}

TEST_F(SimulationEditingTest, ACancelledCutKeepsTheCopy)
{
    TestGuiServices     services;
    OpenRocketClipboard clipboard;
    const EditedTable   table(simpleModelRocket(), clipboard, services);
    services.prompts().answerDeleteSimulations({.confirmed = false, .doNotAskAgain = false});
    table.selectRows({0, 1});

    EXPECT_FALSE(table.editing().cut());
    EXPECT_EQ(clipboardNames(clipboard), (Names{"Simulation 1", "Simulation 2"}));
    QGuiApplication::clipboard()->clear();
    EXPECT_EQ(table.document().getSimulationCount(), 5U);
    EXPECT_EQ(table.undoText(), "null");
}

// =========================================================================== rules

TEST_F(SimulationEditingTest, OperationsWithNothingToWorkOnDoNothing)
{
    TestGuiServices     services;
    OpenRocketClipboard clipboard;
    const EditedTable   table(simpleModelRocket(), clipboard, services);
    const QSignalSpy    changed(&clipboard, &OpenRocketClipboard::changed);

    EXPECT_FALSE(table.editing().hasSelection());
    EXPECT_FALSE(table.editing().canPaste());
    EXPECT_FALSE(table.editing().duplicate());
    EXPECT_FALSE(table.editing().copy());
    EXPECT_FALSE(table.editing().cut());
    EXPECT_FALSE(table.editing().paste());
    EXPECT_FALSE(table.editing().deleteSelection());

    EXPECT_EQ(changed.count(), 0);
    EXPECT_EQ(table.document().getSimulationCount(), 5U);
    EXPECT_EQ(table.undoText(), "null");
    EXPECT_TRUE(services.prompts().prompts().empty());
    EXPECT_TRUE(QGuiApplication::clipboard()->text().isEmpty());

    table.selectRows({0});
    EXPECT_TRUE(table.editing().hasSelection());
    EXPECT_EQ(&table.editing().document(), &table.document());
    EXPECT_EQ(&table.editing().table(), &table.view());
    EXPECT_EQ(&table.editing().clipboard(), &clipboard);
}

}  // namespace

}  // namespace QtRocket::Gui
