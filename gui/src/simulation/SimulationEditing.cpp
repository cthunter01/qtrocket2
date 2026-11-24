#include "QtRocket/gui/simulation/SimulationEditing.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <QClipboard>
#include <QGuiApplication>
#include <QObject>
#include <QPointer>
#include <QString>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/SimulationCopy.h"
#include "QtRocket/gui/simulation/SimulationClipboard.h"
#include "QtRocket/gui/simulation/SimulationTableCsvExport.h"
#include "QtRocket/gui/simulation/SimulationTableModel.h"
#include "QtRocket/gui/simulation/SimulationTableView.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/Simulation.h"

namespace QtRocket::Gui
{

SimulationEditing::SimulationEditing(SimulationTableView& table, SimulationClipboard& clipboard,
                                     Preferences& preferences, UserPrompts& prompts,
                                     QObject* parent)
  : QObject(parent),
    m_table(&table),
    m_clipboard(&clipboard),
    m_preferences(&preferences),
    m_prompts(&prompts)
{
}

SimulationEditing::~SimulationEditing() = default;

OpenRocketDocument& SimulationEditing::document() const noexcept
{
    return m_table->tableModel().document();
}

bool SimulationEditing::hasSelection() const
{
    return m_table->firstSelectedSimulation() != nullptr;
}

bool SimulationEditing::canPaste() const
{
    // OpenRocket bug 27, fixed: no selected row needed (see the header).
    const std::shared_ptr<const SimulationCopy> copy = m_clipboard->simulations();
    return copy != nullptr && !copy->empty();
}

// -------------------------------------------------------------------------- the operations

std::shared_ptr<Simulation> SimulationEditing::newSimulation()
{
    OpenRocketDocument& doc = document();
    // Java: new Simulation(document, document.getRocket()), the defaults of the preferences.
    auto simulation = std::make_shared<Simulation>(&doc, doc.getRocket(), *m_preferences);
    simulation->setName(doc.getNextSimulationName());
    //: Not translated in OpenRocket (SimulationPanel.java): the undo step of New simulation,
    //: "Add " and the new simulation's name
    doc.addUndoPosition(
        QtStrings::toUtf8(tr("Add %1").arg(QtStrings::fromUtf8(simulation->getName()))));
    doc.addSimulation(simulation);
    // OpenRocket bug 24, fixed: the new simulation itself, not its list index as a view row.
    m_table->selectSimulations({simulation});
    return simulation;
}

bool SimulationEditing::duplicate()
{
    const std::vector<std::shared_ptr<Simulation>> simulations = selected();
    if (simulations.empty())
    {
        return false;
    }
    OpenRocketDocument& doc = document();
    addDuplicateUndoPosition(simulations.size(),
                             QtStrings::fromUtf8(simulations.front()->getName()));
    std::vector<std::shared_ptr<Simulation>> duplicates;
    duplicates.reserve(simulations.size());
    for (const std::shared_ptr<Simulation>& simulation : simulations)
    {
        std::shared_ptr<Simulation> duplicate = simulation->duplicateSimulation(doc.getRocket());
        if (SimulationCopy::hasNumberedName(duplicate->getName()))
        {
            duplicate->setName(doc.getNextSimulationName());
        }
        // Java: at the end of the list (its index count, count + 1, ...).
        doc.addSimulation(duplicate);
        duplicates.push_back(std::move(duplicate));
    }
    // OpenRocket bug 24, fixed: the duplicates themselves, not their list indices as view rows.
    m_table->selectSimulations(duplicates);
    return true;
}

bool SimulationEditing::copy()
{
    const std::vector<std::shared_ptr<Simulation>> simulations = selected();
    if (simulations.empty())
    {
        return false;
    }
    copySimulations(simulations);
    return true;
}

bool SimulationEditing::cut()
{
    std::vector<std::shared_ptr<Simulation>> simulations = selected();
    if (simulations.empty())
    {
        return false;
    }
    const QPointer<SimulationEditing> alive(this);
    copySimulations(simulations);
    if (alive.isNull())
    {
        return false;  // a slot of the clipboard's change took the table away
    }
    return deleteSimulations(std::move(simulations));
}

bool SimulationEditing::paste()
{
    // Held, whatever happens to the clipboard while the copies go in (see SimulationClipboard).
    const std::shared_ptr<const SimulationCopy> copy = m_clipboard->simulations();
    if (copy == nullptr || copy->empty())
    {
        return false;
    }
    OpenRocketDocument& doc = document();
    // OpenRocket bugs 24 and 27, fixed: after the topmost selected simulation in the list (Java:
    // its view row + 1 taken as a list index), and at the end without a selected row (Java: no
    // Paste then).
    std::size_t index = doc.getSimulationCount();
    if (const std::shared_ptr<Simulation> first = m_table->firstSelectedSimulation();
        first != nullptr)
    {
        if (const std::optional<std::size_t> at = doc.getSimulationIndex(*first); at.has_value())
        {
            index = *at + 1;
        }
    }
    addDuplicateUndoPosition(copy->size(), QtStrings::fromUtf8(copy->getEntry(0).name));
    const std::vector<std::shared_ptr<Simulation>> pasted = copy->insertInto(doc, index);
    // OpenRocket bug 24, fixed: the pasted simulations themselves.
    m_table->selectSimulations(pasted);
    return true;
}

bool SimulationEditing::deleteSelection()
{
    return deleteSimulations(selected());
}

// ------------------------------------------------------------------------------- helpers

std::vector<std::shared_ptr<Simulation>> SimulationEditing::selected() const
{
    return m_table->selectedSimulations();
}

void SimulationEditing::copySimulations(const std::vector<std::shared_ptr<Simulation>>& simulations)
{
    std::vector<const Simulation*> sources;
    sources.reserve(simulations.size());
    for (const std::shared_ptr<Simulation>& simulation : simulations)
    {
        sources.push_back(simulation.get());
    }
    SimulationCopy copied = SimulationCopy::copyOf(sources);
    // copySimulationValues("\t"): the rows' text, made before the clipboard tells of its change.
    const QString text =
        QtStrings::fromUtf8(SimulationTableCsvExport::copyText(m_table->tableModel(), simulations));
    m_clipboard->setSimulations(std::move(copied));
    QGuiApplication::clipboard()->setText(text);
}

bool SimulationEditing::deleteSimulations(std::vector<std::shared_ptr<Simulation>> simulations)
{
    if (simulations.empty())
    {
        return false;
    }
    if (!confirmDeletion(simulations.size()))
    {
        return false;
    }
    OpenRocketDocument& doc = document();
    // The question ran an event loop: a simulation that left the list meanwhile stays out of it.
    std::erase_if(simulations, [&doc](const std::shared_ptr<Simulation>& simulation) {
        return !doc.getSimulationIndex(*simulation).has_value();
    });
    if (simulations.empty())
    {
        return false;
    }
    if (simulations.size() == 1)
    {
        //: Not translated in OpenRocket (SimulationPanel.java): the undo step of Delete of one
        //: simulation, "Delete " and its name
        doc.addUndoPosition(QtStrings::toUtf8(
            tr("Delete %1").arg(QtStrings::fromUtf8(simulations.front()->getName()))));
    }
    else
    {
        //: Not translated in OpenRocket (SimulationPanel.java): the undo step of Delete of more
        //: than one simulation
        doc.addUndoPosition(QtStrings::toUtf8(tr("Delete simulations")));
    }
    for (const std::shared_ptr<Simulation>& simulation : simulations)
    {
        doc.removeSimulation(*simulation);
    }
    return true;
}

bool SimulationEditing::confirmDeletion(std::size_t count)
{
    // The store outlives this object, which the question's event loop may destroy.
    Preferences& preferences = *m_preferences;
    if (!preferences.getConfirmSimDeletion())
    {
        return true;
    }
    const QPointer<SimulationEditing>          alive(this);
    const UserPrompts::DeleteSimulationsAnswer answer =
        m_prompts->askDeleteSimulations(m_table, count);
    // verifyDeleteSimulation(): the box counts whatever the answer.
    if (answer.doNotAskAgain)
    {
        preferences.setConfirmSimDeletion(false);
    }
    return answer.confirmed && !alive.isNull();
}

void SimulationEditing::addDuplicateUndoPosition(std::size_t count, const QString& firstName) const
{
    if (count == 1)
    {
        //: Not translated in OpenRocket (SimulationPanel.java): the undo step of Duplicate and
        //: Paste of one simulation, "Duplicate " and its name
        document().addUndoPosition(QtStrings::toUtf8(tr("Duplicate %1").arg(firstName)));
    }
    else
    {
        //: Not translated in OpenRocket (SimulationPanel.java): the undo step of Duplicate and
        //: Paste of more than one simulation
        document().addUndoPosition(QtStrings::toUtf8(tr("Duplicate simulations")));
    }
}

}  // namespace QtRocket::Gui
