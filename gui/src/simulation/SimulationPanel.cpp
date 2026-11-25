#include "QtRocket/gui/simulation/SimulationPanel.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include <QAction>
#include <QChar>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMetaObject>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QStackedWidget>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/simulation/SimulationClipboard.h"
#include "QtRocket/gui/simulation/SimulationColumnVisibility.h"
#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/simulation/SimulationEditing.h"
#include "QtRocket/gui/simulation/SimulationTableModel.h"
#include "QtRocket/gui/simulation/SimulationTableView.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/MenuText.h"
#include "QtRocket/gui/util/SimulationDialogs.h"
#include "QtRocket/gui/util/SimulationExecutor.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/IconButton.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/Simulation.h"

namespace QtRocket::Gui
{

namespace
{

using Status = Simulation::Status;
using Tab    = SimulationDialogs::Tab;

/// The context of the panel's guarded slots.
[[nodiscard]] QString context()
{
    return QStringLiteral("Flight simulations");
}

/// An action of the panel named @p objectName with @p icon and @p text (its mnemonic marked
/// already), the tooltip @p toolTip (none when empty) and the keys @p keys, which it only shows
/// (see the class comment: a widget shortcut of no widget but the popup menu).
QAction* makeAction(QObject& parent, const QString& objectName, Icons::Id icon, const QString& text,
                    const QString& toolTip, const QKeySequence& keys)
{
    auto* action = makeChild<QAction>(&parent, Icons::icon(icon), text);
    action->setObjectName(objectName);
    action->setToolTip(toolTip);
    action->setShortcut(keys);
    action->setShortcutContext(Qt::WidgetShortcut);
    action->setMenuRole(QAction::NoRole);
    return action;
}

/// Ctrl (Cmd on macOS) with @p key: RocketActions' Menu+key strokes.
[[nodiscard]] QKeySequence menuShortcut(Qt::Key key)
{
    return {Qt::CTRL | key};
}

}  // namespace

SimulationPanel::SimulationPanel(OpenRocketDocument& document, const GuiServices& services,
                                 SimulationClipboard& clipboard, QWidget* parent)
  : QWidget(parent),
    m_document(&document),
    m_services(services),
    m_clipboard(&clipboard),
    // The table's model, the panel's first child: Qt destroys the children in the order they
    // came, and a view outlives its model's destruction (QAbstractItemView::_q_modelDestroyed()).
    m_model(makeChild<SimulationTableModel>(this, document, services.preferences())),
    m_cards(makeChild<QStackedWidget>(this)),
    m_helpLabel(makeChild<QLabel>(m_cards, noConfigurationText())),
    m_table(makeChild<SimulationTableView>(m_cards, *m_model)),
    // Restored as the tab is built (ColumnVisibilityController's constructor).
    m_columnVisibility(makeChild<SimulationColumnVisibility>(
        m_table, *m_table, document.getDocumentPreferences(), services.preferences())),
    m_editing(makeChild<SimulationEditing>(m_table, *m_table, clipboard, services.preferences(),
                                           services.prompts())),
    m_popup(makeChild<QMenu>(this)),
    //: simpanel.but.newsimulation
    m_newAction(makeAction(*this, QStringLiteral("newSimulationAction"), Icons::Id::FILE_NEW,
                           MenuText::withMnemonic(tr("New simulation"), QChar(u'N')), {}, {})),
    //: simpanel.pop.edit
    m_editAction(makeAction(*this, QStringLiteral("editSimulationAction"), Icons::Id::EDIT_EDIT,
                            MenuText::withMnemonic(tr("Edit"), QChar(u'E')),
                            //: simpanel.pop.edit.ttip
                            tr("Edit the selected simulation(s)"), menuShortcut(Qt::Key_E))),
    //: simpanel.pop.cut
    m_cutAction(makeAction(
        *this, QStringLiteral("cutSimulationAction"), Icons::Id::EDIT_CUT,
        MenuText::withMnemonic(tr("Cut"), QChar(u'X')),
        //: simpanel.pop.cut.ttip
        tr("Copy the selected simulation(s) to the clipboard and delete from this design"),
        QKeySequence(QKeySequence::Cut))),
    //: simpanel.pop.copy
    m_copyAction(makeAction(*this, QStringLiteral("copySimulationAction"), Icons::Id::EDIT_COPY,
                            MenuText::withMnemonic(tr("Copy"), QChar(u'C')),
                            //: simpanel.pop.copy.ttip
                            tr("Copy the selected simulation(s) to the clipboard"),
                            QKeySequence(QKeySequence::Copy))),
    //: simpanel.pop.paste
    m_pasteAction(makeAction(*this, QStringLiteral("pasteSimulationAction"), Icons::Id::EDIT_PASTE,
                             MenuText::withMnemonic(tr("Paste"), QChar(u'V')),
                             //: simpanel.pop.paste.ttip
                             tr("Paste the simulation(s) on the clipboard to the design."),
                             QKeySequence(QKeySequence::Paste))),
    //: simpanel.pop.duplicate
    m_duplicateAction(
        makeAction(*this, QStringLiteral("duplicateSimulationAction"), Icons::Id::EDIT_DUPLICATE,
                   MenuText::withMnemonic(tr("Duplicate"), QChar(u'D')),
                   //: simpanel.pop.duplicate.ttip
                   tr("Duplicate the selected simulation(s)"), menuShortcut(Qt::Key_D))),
    // The mnemonic of Delete is the Delete key, which no text holds.
    //: simpanel.pop.delete
    m_deleteAction(makeAction(*this, QStringLiteral("deleteSimulationAction"),
                              Icons::Id::EDIT_DELETE, MenuText::escaped(tr("Delete")),
                              //: simpanel.pop.delete.ttip
                              tr("Delete the selected simulation(s)"),
                              QKeySequence(Qt::Key_Delete))),
    //: simpanel.pop.run
    m_runAction(makeAction(*this, QStringLiteral("runSimulationAction"), Icons::Id::SIM_RUN,
                           MenuText::escaped(tr("Run")),
                           //: simpanel.pop.run.ttip
                           tr("Run the selected simulation(s)"), {})),
    //: simpanel.pop.plot
    m_plotAction(makeAction(*this, QStringLiteral("plotSimulationAction"), Icons::Id::SIM_PLOT,
                            MenuText::escaped(tr("Plot / Export")),
                            //: simpanel.pop.plot.ttip
                            tr("Plot or Export the selected simulation(s)"), {})),
    //: simpanel.pop.exportSimTableToCSV
    m_exportTableAction(
        makeAction(*this, QStringLiteral("simTableExportAction"), Icons::Id::SIM_TABLE_EXPORT,
                   MenuText::escaped(tr("Export simulation table as CSV file")), {}, {})),
    //: simpanel.pop.exportSelectedSimsToCSV
    m_exportSelectedAction(
        makeAction(*this, QStringLiteral("selectedSimsExportAction"), Icons::Id::SIM_TABLE_EXPORT,
                   MenuText::escaped(tr("Export simulation(s) as CSV file")), {}, {}))
{
    setObjectName(QStringLiteral("simulationPanel"));
    makeActions();
    makePopupMenu();

    // MigLayout("fill", "[grow][][][][][][][grow]"): the buttons centred in a row ("skip 1,
    // gapright para", "wrap para"), the cards under them ("spanx, grow, pushy").
    auto* layout = makeChild<QVBoxLayout>(this);
    layout->setSpacing(kParagraphGap);
    auto* row = makeChild<QWidget>(this);
    row->setObjectName(QStringLiteral("simulationButtons"));
    makeButtons(*row);
    layout->addWidget(row);

    // The help (CARD_HELP, the first card: CardLayout shows it until updateMotorState() switches)
    // and the table (CARD_TABLE).
    m_cards->setObjectName(QStringLiteral("cardPanel"));
    m_helpLabel->setObjectName(QStringLiteral("helpLabel"));
    m_helpLabel->setTextFormat(Qt::RichText);
    m_helpLabel->setAlignment(Qt::AlignCenter);
    m_cards->addWidget(m_helpLabel);
    m_cards->addWidget(m_table);
    layout->addWidget(m_cards, 1);

    connectParts();
    updateMotorState();
    updateActions();
}

SimulationPanel::~SimulationPanel()
{
    // The children go after this body (QWidget's destructor deletes them): none of their signals
    // may reach this object's slots then.
    disconnect(m_table, nullptr, this, nullptr);
    disconnect(m_model, nullptr, this, nullptr);
    disconnect(m_clipboard, nullptr, this, nullptr);
}

QString SimulationPanel::noConfigurationText()
{
    //: simpanel.lbl.noConfiguration (%s: the name of the tab "Motors & Configuration")
    QString help =
        tr("<html>No configuration with motors defined.<br>Please create a valid "
           "configuration in the '%1' tab.</html>");
    if (help.contains(QStringLiteral("%1")))
    {
        // Escaped for the rich text (Swing's HTML shows the bare '&' as it is; so does Qt's).
        //: BasicFrame.tab.Flightconfig
        help = help.arg(tr("Motors & Configuration").toHtmlEscaped());
    }
    return help;
}

bool SimulationPanel::isHelpShown() const
{
    return m_cards->currentWidget() == m_helpLabel;
}

// ------------------------------------------------------------------------------ the widgets

void SimulationPanel::makeActions()
{
    const auto connectAction = [this](QAction* action, auto operation) {
        connect(action, &QAction::triggered, this,
                guarded(std::move(operation), action->iconText()));
    };
    connectAction(m_newAction, [this] { newSimulation(); });
    connectAction(m_editAction, [this] { editSimulation(); });
    connectAction(m_cutAction, [this] { cutSimulations(); });
    connectAction(m_copyAction, [this] { copySimulations(); });
    connectAction(m_pasteAction, [this] { pasteSimulations(); });
    connectAction(m_duplicateAction, [this] { duplicateSimulations(); });
    connectAction(m_deleteAction, [this] { deleteSimulations(); });
    connectAction(m_runAction, [this] { runSimulation(); });
    connectAction(m_plotAction, [this] { plotSimulation(); });
    connectAction(m_exportTableAction, [this] { Q_EMIT csvExportRequested(false); });
    connectAction(m_exportSelectedAction, [this] { Q_EMIT csvExportRequested(true); });
}

void SimulationPanel::makeButtons(QWidget& row)
{
    auto* layout = makeChild<QHBoxLayout>(&row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(kParagraphGap);
    layout->addStretch(1);
    // RocketActions.tieActionToButton(button, action, text) and setToolTipText(): the button's
    // own text and tooltip over its action, which the popup shows with its own. A button action
    // of its own carries them (an IconButton shows its action's), enabled as the action is.
    const auto add = [&row, layout](QAction& action, const QString& name, const QString& text,
                                    const QString& toolTip) {
        auto* button = makeChild<IconButton>(&row);
        button->setObjectName(name);
        auto* tie = makeChild<QAction>(button, action.icon(), MenuText::escaped(text));
        tie->setToolTip(toolTip);
        tie->setEnabled(action.isEnabled());
        connect(&action, &QAction::enabledChanged, tie, &QAction::setEnabled);
        connect(tie, &QAction::triggered, &action, &QAction::trigger);
        button->setAction(tie);
        layout->addWidget(button);
        return button;
    };
    m_newButton  = add(*m_newAction, QStringLiteral("newButton"),
                       //: simpanel.but.newsimulation
                       tr("New simulation"),
                       //: simpanel.but.ttip.newsimulation
                       tr("Add a new simulation"));
    m_editButton = add(*m_editAction, QStringLiteral("editButton"),
                       //: simpanel.but.editsimulation
                       tr("Edit simulation"),
                       //: simpanel.but.ttip.editsim
                       tr("Edit the selected simulation"));
    m_runButton  = add(*m_runAction, QStringLiteral("runButton"),
                       //: simpanel.but.runsimulations
                       tr("Run simulations"),
                       //: simpanel.but.ttip.runsimu
                       tr("Re-run the selected simulations"));
    // The Monte Carlo button (simpanel.but.landingDispersion) comes with Milestone 7.
    m_deleteButton = add(*m_deleteAction, QStringLiteral("deleteButton"),
                         //: simpanel.but.deletesimulations
                         tr("Delete simulations"),
                         //: simpanel.but.ttip.deletesim
                         tr("Delete the selected simulations"));
    // No setToolTipText(): the action's (simpanel.pop.plot.ttip).
    m_plotButton = add(*m_plotAction, QStringLiteral("plotButton"),
                       //: simpanel.but.plotexport
                       tr("Plot / Export"), m_plotAction->toolTip());
    layout->addStretch(1);
}

void SimulationPanel::makePopupMenu()
{
    m_popup->setObjectName(QStringLiteral("pm"));
    // A JMenuItem shows its action's SHORT_DESCRIPTION.
    m_popup->setToolTipsVisible(true);
    m_popup->addAction(m_editAction);
    m_popup->addAction(m_cutAction);
    m_popup->addAction(m_copyAction);
    m_popup->addAction(m_pasteAction);
    m_popup->addAction(m_duplicateAction);
    m_popup->addAction(m_deleteAction);
    m_popup->addSeparator();
    m_popup->addAction(m_runAction);
    // The Monte Carlo item (landingDispersionAction) comes with Milestone 7.
    m_popup->addAction(m_plotAction);
    m_popup->addAction(m_exportSelectedAction);
}

void SimulationPanel::connectParts()
{
    // The table's mouse (SimulationPanel's mouseClicked() listener). The signals hand over
    // shared simulations: direct connections.
    connect(m_table, &SimulationTableView::cellClicked, this,
            guarded([this](const std::shared_ptr<Simulation>& simulation,
                           Column column) { cellClicked(simulation, column); },
                    context()));
    connect(m_table, &SimulationTableView::cellDoubleClicked, this,
            guarded([this](const std::shared_ptr<Simulation>& simulation,
                           Column column) { cellDoubleClicked(simulation, column); },
                    context()));
    connect(m_table, &SimulationTableView::contextMenuRequested, this,
            guarded([this](const QPoint& position) { showPopupMenu(position); }, context()));

    // What the rules read: the selection (the selection listener), the rows, the clipboard.
    const auto update = guarded([this] { updateActions(); }, context());
    connect(m_table, &SimulationTableView::selectedSimulationsChanged, this, update);
    connect(m_model, &SimulationTableModel::refreshed, this, update);
    connect(m_clipboard, &SimulationClipboard::changed, this, update);

    // The rocket's listener: the motors of the configurations.
    m_rocketChanges = m_document->getRocket().addComponentChangeListener(guarded(
        [this](const ComponentChangeEvent& event) {
            if (event.isMotorChange() || event.isTreeChange())
            {
                updateMotorState();
            }
        },
        context()));
}

void SimulationPanel::updateMotorState()
{
    const Rocket& rocket = m_document->getRocket();
    const bool    valid  = std::ranges::any_of(
        rocket.getIds(),
        [&rocket](const FlightConfigurationId& id) { return rocket.hasMotors(id); });
    if (valid == m_hasValidConfig)
    {
        return;
    }
    m_hasValidConfig = valid;
    m_cards->setCurrentWidget(valid ? static_cast<QWidget*>(m_table) : m_helpLabel);
    updateActions();
}

// ------------------------------------------------------------------------------ the actions

void SimulationPanel::updateActions()
{
    // Java's getSelectedRowCount().
    const std::size_t selected = m_table->selectedSimulations().size();
    const bool        any      = selected > 0;
    const bool        rows     = m_model->rowCount() > 0;
    m_newAction->setEnabled(m_hasValidConfig);
    m_editAction->setEnabled(any && m_hasValidConfig);
    m_cutAction->setEnabled(any);
    m_copyAction->setEnabled(any);
    // OpenRocket bug 27, fixed: no selected row needed while the table is shown (see the class
    // comment).
    m_pasteAction->setEnabled(m_editing->canPaste() && (any || m_hasValidConfig));
    m_duplicateAction->setEnabled(any);
    m_deleteAction->setEnabled(any && m_hasValidConfig);
    m_runAction->setEnabled(any && m_hasValidConfig);
    m_plotAction->setEnabled(selected == 1 && m_hasValidConfig);
    m_exportTableAction->setEnabled(rows);
    m_exportSelectedAction->setEnabled(rows && any);
}

// ------------------------------------------------------------------------- the window's part

void SimulationPanel::updatePreviousSelection()
{
    m_previousSelection = {};
    for (const std::shared_ptr<Simulation>& simulation : m_table->selectedSimulations())
    {
        m_previousSelection.simulations.emplace_back(simulation);
        m_previousSelection.rows.push_back(m_model->rowOf(*simulation));
    }
}

void SimulationPanel::takeTheSpotlight()
{
    // Java: SwingUtilities.invokeLater().
    QMetaObject::invokeMethod(this, guarded([this] { spotlight(); }, context()),
                              Qt::QueuedConnection);
}

void SimulationPanel::spotlight()
{
    // requestFocusInWindow().
    m_table->setFocus();
    if (m_model->rowCount() == 0 || !m_table->selectedSimulations().empty())
    {
        return;
    }
    m_table->selectSimulations(rememberedSimulations());
    updateActions();
}

std::vector<std::shared_ptr<Simulation>> SimulationPanel::rememberedSimulations() const
{
    const int rowCount = m_model->rowCount();
    // The remembered simulations still in the table (OpenRocket bug 28, see the class comment),
    // by their rows now.
    std::vector<std::pair<int, std::shared_ptr<Simulation>>> found;
    for (const std::weak_ptr<Simulation>& remembered : m_previousSelection.simulations)
    {
        std::shared_ptr<Simulation> simulation = remembered.lock();
        const int                   row = simulation == nullptr ? -1 : m_model->rowOf(*simulation);
        if (row >= 0)
        {
            found.emplace_back(row, std::move(simulation));
        }
    }
    if (found.empty() && m_previousSelection.simulations.empty())
    {
        // Nothing remembered: the first row.
        found.emplace_back(0, m_model->simulation(0));
    }
    else if (found.empty())
    {
        // None of them is in the table any more: Java's rows, those the table still has.
        for (const int row : m_previousSelection.rows)
        {
            if (row >= 0 && row < rowCount)
            {
                found.emplace_back(row, m_model->simulation(row));
            }
        }
    }
    // From the top: the last one is the lead (addRowSelectionInterval() in Java's order).
    std::ranges::sort(found, {}, &std::pair<int, std::shared_ptr<Simulation>>::first);
    std::vector<std::shared_ptr<Simulation>> simulations;
    simulations.reserve(found.size());
    for (auto& entry : found)
    {
        simulations.push_back(std::move(entry.second));
    }
    return simulations;
}

void SimulationPanel::activating()
{
    if (!m_services.preferences().getAutoRunSimulations())
    {
        return;
    }
    // Every simulation that is not simulated or out of date, in the table's order.
    std::vector<std::shared_ptr<Simulation>> outdated;
    for (int row = 0; row < m_model->rowCount(); ++row)
    {
        if (needsRun(row))
        {
            outdated.push_back(m_model->simulation(row));
        }
    }
    if (outdated.empty())
    {
        return;
    }
    if (runSimulations(std::move(outdated)))
    {
        maintainSelection();
    }
}

void SimulationPanel::showPopupMenu(const QPoint& globalPosition)
{
    m_popup->popup(globalPosition);
}

// --------------------------------------------------------------------------- the operations

void SimulationPanel::newSimulation()
{
    std::shared_ptr<Simulation> simulation = m_editing->newSimulation();
    updatePreviousSelection();
    openDialog(Tab::LAUNCH_CONDITIONS, true, {std::move(simulation)});
}

void SimulationPanel::editSimulation()
{
    std::vector<std::shared_ptr<Simulation>> simulations = m_table->selectedSimulations();
    if (simulations.empty())
    {
        return;
    }
    openDialog(Tab::LAUNCH_CONDITIONS, false, std::move(simulations));
}

void SimulationPanel::runSimulation()
{
    std::vector<std::shared_ptr<Simulation>> simulations = m_table->selectedSimulations();
    if (simulations.empty() || !runSimulations(std::move(simulations)))
    {
        return;
    }
    maintainSelection();
    takeTheSpotlight();
}

void SimulationPanel::plotSimulation()
{
    const std::shared_ptr<Simulation> simulation = m_table->firstSelectedSimulation();
    if (simulation == nullptr)
    {
        return;
    }
    // OpenRocket bug 24 (Plot / Export), fixed: the simulation itself, not its model index as a
    // view row.
    m_table->selectSimulations({simulation});
    if (!simulation->hasSimulationData() && !runSimulations({simulation}))
    {
        return;
    }
    maintainSelection();
    takeTheSpotlight();
    if (m_model->rowOf(*simulation) < 0)
    {
        return;  // it left the document during the run
    }
    // Without data after its run (see the class comment), the Launch conditions.
    openDialog(simulation->hasSimulationData() ? Tab::PLOT_DATA : Tab::LAUNCH_CONDITIONS, false,
               {simulation});
}

void SimulationPanel::cutSimulations()
{
    const QPointer<SimulationPanel> alive(this);
    const bool                      deleted = m_editing->cut();
    if (!deleted || alive.isNull())
    {
        return;
    }
    // Java's deleteSimulations(): the selection (none) remembered, the spotlight taken.
    updatePreviousSelection();
    takeTheSpotlight();
}

void SimulationPanel::copySimulations()
{
    m_editing->copy();
}

void SimulationPanel::pasteSimulations()
{
    m_editing->paste();
}

void SimulationPanel::duplicateSimulations()
{
    m_editing->duplicate();
}

void SimulationPanel::deleteSimulations()
{
    const QPointer<SimulationPanel> alive(this);
    const bool                      deleted = m_editing->deleteSelection();
    if (!deleted || alive.isNull())
    {
        return;
    }
    updatePreviousSelection();
    takeTheSpotlight();
}

// ------------------------------------------------------------------------- the table's mouse

void SimulationPanel::cellClicked(const std::shared_ptr<Simulation>& simulation, Column column)
{
    // A click on the status column re-runs the selected simulations when the first selected one
    // is not simulated or out of date.
    if (column != Column::STATUS || simulation == nullptr)
    {
        return;
    }
    const int row = m_model->rowOf(*simulation);
    if (row >= 0 && needsRun(row))
    {
        runSimulation();
    }
}

void SimulationPanel::cellDoubleClicked(const std::shared_ptr<Simulation>& simulation,
                                        Column                             column)
{
    if (simulation == nullptr || m_model->rowOf(*simulation) < 0)
    {
        return;
    }
    switch (column)
    {
        case Column::STATUS:
            return;
        case Column::WARNINGS:
            // The warnings of the simulation: its dialog on the Warnings tab, without
            // openDialog()'s afterwards (Java opens it itself there).
            if (SimulationDialogs* const dialogs = m_services.simulationDialogs();
                dialogs != nullptr)
            {
                dialogs->editSimulations(
                    this, *m_document,
                    {.simulations = {simulation}, .tab = Tab::WARNINGS, .newSimulation = false});
            }
            return;
        default:
            openDialog(simulation);
            return;
    }
}

// ---------------------------------------------------------------------------------- dialogs

void SimulationPanel::openDialog(const std::shared_ptr<Simulation>& simulation)
{
    openDialog(isDataUpToDate(*simulation) ? Tab::PLOT_DATA : Tab::LAUNCH_CONDITIONS, false,
               {simulation});
}

void SimulationPanel::openDialog(Tab tab, bool newSimulation,
                                 std::vector<std::shared_ptr<Simulation>> simulations)
{
    if (SimulationDialogs* const dialogs = m_services.simulationDialogs(); dialogs != nullptr)
    {
        const QPointer<SimulationPanel> alive(this);
        dialogs->editSimulations(
            this, *m_document,
            {.simulations = std::move(simulations), .tab = tab, .newSimulation = newSimulation});
        if (alive.isNull())
        {
            return;
        }
    }
    maintainSelection();
    takeTheSpotlight();
}

bool SimulationPanel::runSimulations(std::vector<std::shared_ptr<Simulation>> simulations)
{
    SimulationExecutor* const executor = m_services.simulationExecutor();
    if (executor == nullptr || simulations.empty())
    {
        return true;  // nothing runs without an executor (GuiServices)
    }
    const QPointer<SimulationPanel> alive(this);
    // The window's background estimate is not cancelled yet: run 3 gives the tab the window's
    // RocketPanel (RunSimulationsRequest::cancelBackgroundRuns).
    const UserPrompts::RunSimulationsRequest request{.window               = window(),
                                                     .document             = m_document,
                                                     .simulations          = std::move(simulations),
                                                     .executor             = executor,
                                                     .cancelBackgroundRuns = {}};
    static_cast<void>(m_services.prompts().runSimulations(this, request));
    return !alive.isNull();
}

void SimulationPanel::maintainSelection()
{
    // fireMaintainSelection(): fireTableDataChanged(), the table's statuses and cells read again
    // (the units of now); the selection follows its simulations (SimulationTableModel).
    m_model->refresh();
}

bool SimulationPanel::needsRun(int row) const
{
    const Status status = m_model->status(row);
    return status == Status::NOT_SIMULATED || status == Status::OUTDATED;
}

bool SimulationPanel::isDataUpToDate(const Simulation& simulation) const
{
    const int row = m_model->rowOf(simulation);
    return row >= 0 && simulation.hasSimulationData() &&
           Simulation::isStatusUpToDate(m_model->status(row));
}

}  // namespace QtRocket::Gui
