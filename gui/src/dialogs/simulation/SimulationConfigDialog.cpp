#include "QtRocket/gui/dialogs/simulation/SimulationConfigDialog.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <QDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLatin1Char>
#include <QLineEdit>
#include <QMargins>
#include <QPointer>
#include <QPushButton>
#include <QRect>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSize>
#include <QSizePolicy>
#include <QString>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/gui/dialogs/ModalDialog.h"
#include "QtRocket/gui/dialogs/plot/SimulationPlotPanel.h"
#include "QtRocket/gui/dialogs/simulation/SimulationConditionsPanel.h"
#include "QtRocket/gui/dialogs/simulation/SimulationExportPanel.h"
#include "QtRocket/gui/dialogs/simulation/SimulationOptionsPanel.h"
#include "QtRocket/gui/dialogs/simulation/SimulationWarningsPanel.h"
#include "QtRocket/gui/settings/WindowGeometry.h"
#include "QtRocket/gui/util/ColorConversion.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/SimulationStatusColors.h"
#include "QtRocket/gui/util/SimulationStatusText.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/ConfigurationComboBox.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

namespace
{

/// MigLayout's paragraph gap ("gapright para", "gapleft para").
constexpr int kParagraphGap = 12;
/// The size of the multi-simulation edit label, in points above the default font's (Java:
/// StyledLabel("", -1, BOLD)).
constexpr double kMultiSimEditSize = -1.0;

/// A tab's scroll area (Java: createTabScrollPane()): as JScrollPane's preferred size, its size
/// hint is its content's (QScrollArea's own is limited to 36 x 24 lines), so that the packed
/// dialog shows the tab without scroll bars.
class TabContentScrollArea final : public QScrollArea
{
public:
    explicit TabContentScrollArea(QWidget* parent) : QScrollArea(parent) { }

    [[nodiscard]] QSize sizeHint() const override
    {
        const int   frame   = 2 * frameWidth();
        const QSize content = widget() != nullptr
                                  ? widget()->sizeHint().expandedTo(widget()->minimumSizeHint())
                                  : QSize(0, 0);
        return content + QSize(frame, frame);
    }
};

/// The scroll area of a tab of @p tabs named @p name over @p content, made by @p makeContent
/// (Java: createTabScrollPane(): no border, the scroll bars as needed, no minimum size, a scroll
/// step of 16 pixels).
template <class MakeContent>
[[nodiscard]] QScrollArea* makeTabScrollArea(QTabWidget& tabs, const QString& name,
                                             const MakeContent& makeContent)
{
    auto* scrollArea = makeChild<TabContentScrollArea>(&tabs);
    scrollArea->setObjectName(name);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setMinimumSize(0, 0);
    scrollArea->verticalScrollBar()->setSingleStep(SimulationConfigDialog::kScrollStep);
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(makeContent(*scrollArea));
    return scrollArea;
}

/// Undoes @p document back to the state at its undo position @p position, the one the dialog's
/// own undo position made (Java's discardChanges(): if (document.isUndoAvailable())
/// document.undo()). OpenRocket bug 29, the dialog's half: Java's single undo() of a clean
/// document goes back one step beyond the dialog's position, undoing the change the user made
/// before the dialog opened (after an edit the document's equality ignores, or one undone by
/// hand); here a clean document at the dialog's position is left as it is.
void undoToPosition(OpenRocketDocument& document, std::size_t position)
{
    for (;;)
    {
        const OpenRocketDocument::UndoDetail before = document.getUndoDetail();
        if ((before.clean && before.position <= position) || !document.isUndoAvailable())
        {
            return;
        }
        document.undo();
        // An undo goes back a step, or from the changes made since the last step to that step;
        // one that did neither (a slot of its events changed the document again) ends the loop.
        const OpenRocketDocument::UndoDetail after = document.getUndoDetail();
        if (after.position >= before.position && !after.clean)
        {
            return;
        }
    }
}

/// Undoes @p document until @p simulation, which the Simulations tab has just added, is no longer
/// in it. OpenRocket bug 30: Java's single undo() after an edit of a new simulation only undoes
/// the edit (back to the dialog's undo position, made after the addition), so the simulation it
/// asked to "undo adding" stays; here the addition is undone too.
void undoAddition(OpenRocketDocument& document, const Simulation& simulation)
{
    while (document.getSimulationIndex(simulation).has_value() && document.isUndoAvailable())
    {
        const OpenRocketDocument::UndoDetail before = document.getUndoDetail();
        document.undo();
        const OpenRocketDocument::UndoDetail after = document.getUndoDetail();
        if (after.position >= before.position && !after.clean)
        {
            return;
        }
    }
}

}  // namespace

SimulationConfigDialog::SimulationConfigDialog(const GuiServices&                    services,
                                               OpenRocketDocument&                   document,
                                               const SimulationDialogs::EditRequest& request,
                                               QWidget*                              parent)
  : QDialog(parent),
    m_services(&services),
    m_document(&document),
    m_simulations(request.simulations),
    m_newSimulation(request.newSimulation),
    m_initialIsSaved(document.isSaved()),
    m_cancelBackgroundRuns(request.cancelBackgroundRuns),
    m_pauseBackgroundRuns(request.pauseBackgroundRuns),
    m_scope(*this, nullptr)
{
    if (m_simulations.empty() ||
        std::ranges::any_of(m_simulations, [](const std::shared_ptr<Simulation>& simulation) {
            return simulation == nullptr;
        }))
    {
        bug("The simulation settings dialog edits at least one simulation, none null");
    }
    setObjectName(QString::fromLatin1(kGeometryName));
    setWindowTitle(titleFor(m_simulations.size()));
    // DOCUMENT_MODAL: modal to the design window.
    setWindowModality(Qt::WindowModal);

    // The window's background estimate neither starts nor is adopted while the dialog is open
    // (O-M4-16): its adoption would be one of the dialog's changes, and Cancel's undo would undo
    // it.
    if (m_pauseBackgroundRuns)
    {
        m_pauseBackgroundRuns(true);
    }

    Simulation& first = *m_simulations.front();
    if (m_simulations.size() == 1)
    {
        //: Not translated in OpenRocket (SimulationConfigDialog.java): the undo step of the
        //: settings dialog of one simulation, "Edit " and its name
        document.addUndoPosition(
            QtStrings::toUtf8(tr("Edit %1").arg(QtStrings::fromUtf8(first.getName()))));
    }
    else
    {
        //: Not translated in OpenRocket (SimulationConfigDialog.java): the undo step of the
        //: settings dialog of several simulations
        document.addUndoPosition(QtStrings::toUtf8(tr("Edit simulations")));
    }
    m_undoPosition = document.getUndoDetail().position;

    // The first change marks the dialog modified, once (Java's listener removes itself).
    m_firstChange =
        Signal<>::ScopedConnection(first.changed().connect([this] { firstSimulationChanged(); }));

    auto* layout = makeChild<QVBoxLayout>(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* content = makeChild<QWidget>(this);
    content->setObjectName(QStringLiteral("contentPanel"));
    auto* contentLayout = makeChild<QVBoxLayout>(content);
    buildTopPanel(*content, *contentLayout);
    buildTabs(*content, *contentLayout);
    layout->addWidget(content, 1);

    buildBottomPanel();
    layout->addWidget(m_bottomPanel);

    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) { updateButtons(index); });
    switchToTab(request.tab);
    updateButtons(m_tabs->currentIndex());

    sizeAndPlace();
}

SimulationConfigDialog::~SimulationConfigDialog()
{
    // The bindings end before the panels go (QWidget's destructor deletes them after this).
    m_firstChange.disconnect();
    m_scope.release();
}

void SimulationConfigDialog::edit(QWidget* parent, const GuiServices& services,
                                  OpenRocketDocument&                   document,
                                  const SimulationDialogs::EditRequest& request)
{
    ModalDialog<SimulationConfigDialog> dialog(parent != nullptr ? parent->window() : nullptr,
                                               services, document, request);
    static_cast<void>(dialog.exec());
}

int SimulationConfigDialog::tabIndex(SimulationDialogs::Tab tab) noexcept
{
    switch (tab)
    {
        case SimulationDialogs::Tab::LAUNCH_CONDITIONS:
            return kLaunchConditionsTab;
        case SimulationDialogs::Tab::WARNINGS:
            return kWarningsTab;
        case SimulationDialogs::Tab::PLOT_DATA:
            return kPlotTab;
        case SimulationDialogs::Tab::EXPORT_DATA:
            return kExportTab;
    }
    return kLaunchConditionsTab;
}

QString SimulationConfigDialog::titleFor(std::size_t count)
{
    if (count == 1)
    {
        //: simedtdlg.title.Editsim
        return tr("Edit simulation");
    }
    //: simedtdlg.title.MultiSimEdit
    return tr("Multi-simulation edit");
}

// ------------------------------------------------------------------------------ the parts

void SimulationConfigDialog::buildTopPanel(QWidget& content, QVBoxLayout& layout)
{
    Simulation& first = *m_simulations.front();
    m_topPanel        = makeChild<QWidget>(&content);
    m_topPanel->setObjectName(QStringLiteral("topPanel"));
    // "height pref": the rows keep their height.
    m_topPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* grid = makeChild<QGridLayout>(m_topPanel);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(kParagraphGap);

    //// Name:
    //: simedtdlg.lbl.Simname
    m_nameLabel = makeChild<QLabel>(m_topPanel, tr("Name:") + QLatin1Char(' '));
    m_nameLabel->setObjectName(QStringLiteral("nameLabel"));
    grid->addWidget(m_nameLabel, 0, 0);
    m_nameField = makeChild<QLineEdit>(m_topPanel, QtStrings::fromUtf8(first.getName()));
    m_nameField->setObjectName(QStringLiteral("nameField"));
    connect(m_nameField, &QLineEdit::textChanged, this,
            guarded([this](const QString& text) { nameEdited(text); }, QStringLiteral("Name")));
    grid->addWidget(m_nameField, 0, 1);

    //// Flight selector
    //// Flight configuration:
    //: simedtdlg.lbl.Flightcfg
    m_configurationLabel = makeChild<QLabel>(m_topPanel, tr("Flight configuration:"));
    m_configurationLabel->setObjectName(QStringLiteral("configurationLabel"));
    //// Select the motor configuration to use.
    //: simedtdlg.lbl.ttip.Flightcfg
    m_configurationLabel->setToolTip(tr("Select the flight configuration to use."));
    // "gapleft para".
    m_configurationLabel->setContentsMargins(kParagraphGap, 0, 0, 0);
    grid->addWidget(m_configurationLabel, 0, 2);

    m_configurationComboBox = makeChild<ConfigurationComboBox>(m_topPanel, m_document->getRocket(),
                                                               m_services->preferences(), false);
    m_configurationComboBox->setObjectName(QStringLiteral("configComboBox"));
    m_configurationComboBox->setSelectedConfiguration(first.getFlightConfigurationId());
    //// Select the motor configuration to use.
    //: simedtdlg.combo.ttip.Flightcfg
    m_configurationComboBox->setToolTip(tr("Select the flight configuration to use."));
    connect(m_configurationComboBox, &ConfigurationComboBox::configurationActivated, this,
            guarded(
                [this](const FlightConfigurationId& id) {
                    if (!m_closed)
                    {
                        m_simulations.front()->setFlightConfigurationId(id);
                    }
                },
                QStringLiteral("Flight configuration")));
    grid->addWidget(m_configurationComboBox, 0, 3);

    //// Display current simulation status
    //: simpanel.col.Status
    m_statusLabel = makeChild<QLabel>(m_topPanel, tr("Status") + QLatin1Char(':'));
    m_statusLabel->setObjectName(QStringLiteral("statusLabel"));
    grid->addWidget(m_statusLabel, 1, 0);
    // The description first, then the status (Java's order of getStatusDescription() and
    // getStatus(); both work the status out).
    const QString statusText = SimulationStatusText::description(first);
    const QColor  color      = SimulationStatusColors::color(first.getStatus());
    m_statusText             = makeChild<QLabel>(
        m_topPanel, QStringLiteral("<html>") + ColorConversion::formatHtmlColor(color, statusText) +
                        QStringLiteral("</html>"));
    m_statusText->setObjectName(QStringLiteral("simStatus"));
    m_statusText->setTextFormat(Qt::RichText);
    grid->addWidget(m_statusText, 1, 1, 1, 3);

    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);
    layout.addWidget(m_topPanel);
}

void SimulationConfigDialog::buildTabs(QWidget& content, QVBoxLayout& layout)
{
    const std::shared_ptr<Simulation>& first = m_simulations.front();
    m_tabs                                   = makeChild<QTabWidget>(&content);
    m_tabs->setObjectName(QStringLiteral("tabbedPane"));
    // Java: setMinimumSize(new Dimension(0, 0)) and "wmin 0, hmin 0": the tabs never hold the
    // dialog larger than the other parts need (a tab without a scroll area of its own, the plot
    // tab's, is cut off instead). Qt reads a minimum of 0 as none, so the least explicit one.
    m_tabs->setMinimumSize(1, 1);

    //// Launch conditions
    m_conditionsScroll = makeTabScrollArea(
        *m_tabs, QStringLiteral("launchConditionsScrollPane"), [&](QScrollArea& scrollArea) {
            m_conditionsPanel = makeChild<SimulationConditionsPanel>(
                &scrollArea, first->getOptions(), SimulationConditionsPanel::Variant::SIMULATION,
                *m_services, m_scope);
            return m_conditionsPanel;
        });
    //: SimulationConfigDialog.tab.Launchcond
    m_tabs->addTab(m_conditionsScroll, tr("Launch conditions"));

    //// Simulation options
    m_optionsScroll = makeTabScrollArea(
        *m_tabs, QStringLiteral("simulationOptionsScrollPane"), [&](QScrollArea& scrollArea) {
            m_optionsPanel =
                makeChild<SimulationOptionsPanel>(&scrollArea, first, *m_services, m_scope);
            return m_optionsPanel;
        });
    //: SimulationConfigDialog.tab.Simopt
    m_tabs->addTab(m_optionsScroll, tr("Simulation options"));

    //// Simulation Warnings
    m_warningsPanel = makeChild<SimulationWarningsPanel>(m_tabs, first, !isMultiEdit(),
                                                         m_services->preferences(), m_scope);
    //: SimulationConfigDialog.tab.Warnings
    m_tabs->addTab(m_warningsPanel, tr("Warnings"));

    //// Plot data
    const bool hasData = first->hasSimulationData();
    QWidget*   plotTab = nullptr;
    if (hasData)
    {
        m_plotPanel = makeChild<SimulationPlotPanel>(m_tabs, *m_services, first);
        plotTab     = m_plotPanel;
    }
    else
    {
        // Java adds the tab without a component.
        plotTab = makeChild<QWidget>(m_tabs);
        plotTab->setObjectName(QStringLiteral("plotPlaceholder"));
    }
    //: SimulationConfigDialog.tab.Plotdata
    m_tabs->addTab(plotTab, tr("Plot data"));
    if (isMultiEdit() || !hasData)
    {
        m_tabs->setTabEnabled(kPlotTab, false);
        //: SimulationConfigDialog.tab.plotDis.ttip
        const QString disabled = tr("Plotting not supported for multi-simulation editing");
        //: SimulationConfigDialog.tab.plotNoData.ttip
        const QString noData = tr("Simulation has no data to plot");
        m_tabs->setTabToolTip(kPlotTab, hasData ? disabled : noData);
    }

    //// Export data
    QWidget* exportTab = nullptr;
    if (hasData)
    {
        m_exportPanel = makeChild<SimulationExportPanel>(m_tabs, *m_services, first);
        exportTab     = m_exportPanel;
    }
    else
    {
        // Java adds the tab without a component.
        exportTab = makeChild<QWidget>(m_tabs);
        exportTab->setObjectName(QStringLiteral("exportPlaceholder"));
    }
    //: SimulationConfigDialog.tab.Exportdata
    m_tabs->addTab(exportTab, tr("Export data"));
    if (isMultiEdit() || !hasData)
    {
        m_tabs->setTabEnabled(kExportTab, false);
        //: SimulationConfigDialog.tab.expDis.ttip
        const QString disabled = tr("Exporting not supported for multi-simulation editing");
        //: SimulationConfigDialog.tab.expNoData.ttip
        const QString noData = tr("Simulation has no data to export");
        m_tabs->setTabToolTip(kExportTab, hasData ? disabled : noData);
    }

    layout.addWidget(m_tabs, 1);
}

void SimulationConfigDialog::buildBottomPanel()
{
    m_bottomPanel = makeChild<QWidget>(this);
    m_bottomPanel->setObjectName(QStringLiteral("bottomPanel"));
    auto* row = makeChild<QHBoxLayout>(m_bottomPanel);
    // "ins 0 n n n": no inset above.
    const QMargins margins = row->contentsMargins();
    row->setContentsMargins(margins.left(), 0, margins.right(), margins.bottom());

    //// Multi-simulation edit
    if (isMultiEdit())
    {
        m_multiSimEditLabel = makeChild<StyledLabel>(m_bottomPanel, QString(), kMultiSimEditSize,
                                                     StyledLabel::Style::BOLD);
        m_multiSimEditLabel->setObjectName(QStringLiteral("multiSimEditLabel"));
        m_multiSimEditLabel->setFontColor(ThemeColors::color(ThemeColors::Key::MULTI_COMP_EDIT));
        //: simedtdlg.title.MultiSimEdit
        m_multiSimEditLabel->setText(tr("Multi-simulation edit"));
        //: simedtdlg.title.MultiSimEdit.ttip (the names of the simulations follow)
        QString components = tr("<html>You are editing the following simulations: <br>");
        for (std::size_t i = 0; i < m_simulations.size(); ++i)
        {
            components += QtStrings::fromUtf8(m_simulations.at(i)->getName());
            if (i < m_simulations.size() - 1)
            {
                components += QStringLiteral(", ");
            }
        }
        m_multiSimEditLabel->setToolTip(components);
        row->addWidget(m_multiSimEditLabel);
    }
    row->addStretch(1);

    //// Cancel button
    //: dlg.but.cancel
    m_cancelButton = makeChild<QPushButton>(m_bottomPanel, tr("Cancel"));
    m_cancelButton->setObjectName(QStringLiteral("cancelButton"));
    //: SimulationConfigDialog.btn.Cancel.ttip
    m_cancelButton->setToolTip(tr("Discard changes and close the dialog"));
    m_cancelButton->setAutoDefault(false);
    connect(m_cancelButton, &QPushButton::clicked, this,
            guarded([this] { cancel(); }, QStringLiteral("cancelButton")));
    row->addWidget(m_cancelButton);

    //// Ok button
    //: dlg.but.ok
    m_okButton = makeChild<QPushButton>(m_bottomPanel, tr("OK"));
    m_okButton->setObjectName(QStringLiteral("okButton"));
    //: SimulationConfigDialog.btn.OK.ttip
    m_okButton->setToolTip(tr("Keep changes and close the dialog"));
    m_okButton->setAutoDefault(false);
    connect(m_okButton, &QPushButton::clicked, this,
            guarded([this] { ok(); }, QStringLiteral("okButton")));
    row->addWidget(m_okButton);
}

void SimulationConfigDialog::updateButtons(int index)
{
    switch (index)
    {
        case kPlotTab:
            //: SimulationConfigDialog.btn.plot
            m_okButton->setText(tr("Plot"));
            //: dlg.but.close
            m_cancelButton->setText(tr("Close"));
            break;
        case kExportTab:
            //: SimulationConfigDialog.btn.export
            m_okButton->setText(tr("Export"));
            //: dlg.but.close
            m_cancelButton->setText(tr("Close"));
            break;
        default:
            //: dlg.but.ok
            m_okButton->setText(tr("OK"));
            //: dlg.but.cancel
            m_cancelButton->setText(tr("Cancel"));
            break;
    }
}

// ----------------------------------------------------------------------------- the events

void SimulationConfigDialog::firstSimulationChanged()
{
    if (m_modified || m_closed)
    {
        return;
    }
    m_modified = true;
    // Add component changed indicator to the title.
    setWindowTitle(QStringLiteral("* ") + windowTitle());
    // Only do this once.
    m_firstChange.disconnect();
}

void SimulationConfigDialog::nameEdited(const QString& text)
{
    // Java's document listener: an empty field leaves the name.
    if (m_closed || text.isEmpty())
    {
        return;
    }
    m_simulations.front()->setName(QtStrings::toUtf8(text));
}

void SimulationConfigDialog::showEvent(QShowEvent* event)
{
    // Java's dialog has no default button (setDisposableDialogOptions(this, null)): a button of a
    // panel would otherwise become the default one (QDialog picks the first auto-default button
    // when it is shown) and Return in a field would click it.
    for (QPushButton* const button : findChildren<QPushButton*>())
    {
        if (button->window() != this)
        {
            continue;  // a button of a dialog of its own (the plot dialog)
        }
        button->setAutoDefault(false);
        if (button->isDefault())
        {
            button->setDefault(false);
        }
    }
    QDialog::showEvent(event);
}

// ---------------------------------------------------------------------------- the actions

void SimulationConfigDialog::switchToTab(SimulationDialogs::Tab tab)
{
    m_tabs->setCurrentIndex(tabIndex(tab));
}

void SimulationConfigDialog::ok()
{
    if (m_closed)
    {
        return;
    }
    // A button that takes no focus leaves a field's edit pending, where Java's focus loss commits
    // it before the action.
    m_scope.commitAll();
    m_optionsPanel->prepareForSimulation();
    copyChangesToAllSimulations();

    // Run outdated simulations
    if (std::vector<std::shared_ptr<Simulation>> outdated = outdatedSimulations();
        !outdated.empty() && !runSimulations(std::move(outdated)))
    {
        return;  // destroyed with its window during the run
    }
    if (m_closed)
    {
        return;
    }

    const int tabIndex = m_tabs->currentIndex();
    if (tabIndex == kPlotTab)
    {
        if (m_plotPanel == nullptr)
        {
            dispose(QDialog::Accepted);
            resumeBackgroundRuns();
            return;
        }
        // The dialog stays open. A run that left no data has nothing to plot (Java's plot
        // dialog would fail on it).
        if (m_simulations.front()->hasSimulationData())
        {
            static_cast<void>(m_plotPanel->doPlot(this));
        }
        return;
    }
    if (tabIndex == kExportTab)
    {
        if (m_exportPanel == nullptr)
        {
            dispose(QDialog::Accepted);
            resumeBackgroundRuns();
            return;
        }
        // The dialog stays open. The export writes the flight data as the run left it (OpenRocket
        // bug 49, SimulationExportPanel), and nothing when the run left none.
        static_cast<void>(m_exportPanel->doExport());
        return;
    }

    dispose(QDialog::Accepted);
    resumeBackgroundRuns();
}

void SimulationConfigDialog::cancel()
{
    if (m_closed)
    {
        return;
    }
    const int tabIndex = m_tabs->currentIndex();
    if (tabIndex == kLaunchConditionsTab || tabIndex == kSimulationOptionsTab ||
        tabIndex == kWarningsTab)
    {
        cancelSimulationEdit();
        return;
    }
    // Normal close action: the edits stay.
    dispose(QDialog::Rejected);
    resumeBackgroundRuns();
}

void SimulationConfigDialog::reject()
{
    Guard::run([this] { cancel(); }, QStringLiteral("Cancel"));
}

void SimulationConfigDialog::cancelSimulationEdit()
{
    // Don't do anything on cancel if you are editing an existing simulation, and it is not
    // modified.
    if (!m_newSimulation && !m_modified)
    {
        dispose(QDialog::Rejected);
        resumeBackgroundRuns();
        return;
    }

    // Apply the cancel operation if set to auto discard in preferences.
    Preferences& preferences = m_services->preferences();
    if (!preferences.isShowDiscardSimulationConfirmation())
    {
        discardChanges();
        return;
    }

    // Yes/No dialog: Are you sure you want to discard your changes? The question may run an
    // event loop in which the dialog goes with its window: nothing of it is read afterwards
    // unless it is still there.
    const QPointer<SimulationConfigDialog> self(this);
    const UserPrompts::DiscardAnswer       answer = m_services->prompts().askDiscardChanges(
        this, m_newSimulation ? UserPrompts::DiscardQuestion::SIMULATION_ADDITION
                              : UserPrompts::DiscardQuestion::SIMULATION_CHANGES);
    // "Don't ask me again" turns the question off whatever the answer (Java).
    if (answer.doNotAskAgain)
    {
        preferences.setShowDiscardSimulationConfirmation(false);
    }
    if (self.isNull() || m_closed)
    {
        return;
    }
    if (answer.discard)
    {
        discardChanges();
    }
}

void SimulationConfigDialog::discardChanges()
{
    OpenRocketDocument&                    document       = *m_document;
    const std::shared_ptr<Simulation>      first          = m_simulations.front();
    const bool                             newSimulation  = m_newSimulation;
    const bool                             initialIsSaved = m_initialIsSaved;
    const std::size_t                      position       = m_undoPosition;
    const QPointer<SimulationConfigDialog> self(this);
    // Closed first, then the undo (the undo changes the simulation the bindings edit).
    dispose(QDialog::Rejected);
    undoToPosition(document, position);
    if (newSimulation)
    {
        undoAddition(document, *first);
    }
    // Restore the saved state of the document.
    document.setSaved(initialIsSaved);
    document.fireDocumentChangeEvent(
        {.kind = DocumentChangeEvent::Kind::DOCUMENT, .source = std::monostate{}});
    if (!self.isNull())
    {
        resumeBackgroundRuns();
    }
}

void SimulationConfigDialog::copyChangesToAllSimulations()
{
    if (!isMultiEdit())
    {
        return;
    }
    const Simulation& first = *m_simulations.front();
    for (std::size_t i = 1; i < m_simulations.size(); ++i)
    {
        Simulation& other = *m_simulations.at(i);
        if (&other == &first)
        {
            continue;  // listed twice: its own options and extensions already
        }
        other.copySimulationOptionsFrom(first.getOptions());
        std::vector<std::shared_ptr<SimulationExtension>>& extensions =
            other.getSimulationExtensions();
        extensions.clear();
        for (const std::shared_ptr<SimulationExtension>& extension :
             first.getSimulationExtensions())
        {
            extensions.push_back(extension->clone());
        }
    }
}

std::vector<std::shared_ptr<Simulation>> SimulationConfigDialog::outdatedSimulations() const
{
    std::vector<std::shared_ptr<Simulation>> outdated;
    for (const std::shared_ptr<Simulation>& simulation : m_simulations)
    {
        if (!Simulation::isStatusUpToDate(simulation->getStatus()))
        {
            outdated.push_back(simulation);
        }
    }
    return outdated;
}

bool SimulationConfigDialog::runSimulations(std::vector<std::shared_ptr<Simulation>> simulations)
{
    SimulationExecutor* const executor = m_services->simulationExecutor();
    if (executor == nullptr)
    {
        return true;  // nothing runs without an executor (GuiServices)
    }
    const UserPrompts::RunSimulationsRequest request{
        .window               = parentWidget(),
        .document             = m_document,
        .simulations          = std::move(simulations),
        .executor             = executor,
        .cancelBackgroundRuns = m_cancelBackgroundRuns};
    const QPointer<SimulationConfigDialog> self(this);
    static_cast<void>(m_services->prompts().runSimulations(this, request));
    return !self.isNull();
}

void SimulationConfigDialog::dispose(int result)
{
    if (m_closed)
    {
        return;
    }
    m_closed = true;
    m_firstChange.disconnect();
    // The bindings end before the dialog hides: the focus loss of an edited field then reaches no
    // simulation.
    m_scope.release();
    QDialog::done(result);
}

void SimulationConfigDialog::resumeBackgroundRuns()
{
    if (m_resumed)
    {
        return;
    }
    m_resumed = true;
    if (m_pauseBackgroundRuns)
    {
        m_pauseBackgroundRuns(false);
    }
}

// ---------------------------------------------------------------------------- the geometry

void SimulationConfigDialog::sizeAndPlace()
{
    // pack(): the contents' size (adjustSize() would limit it to two thirds of the screen).
    resize(sizeHint());
    capDialogToScreenBounds();
    // GUIUtil.rememberWindowPosition() and rememberWindowSize().
    m_geometry = makeChild<WindowGeometry>(this, m_services->preferences(),
                                           QString::fromLatin1(kGeometryName));
    capDialogToScreenBounds();
}

void SimulationConfigDialog::capDialogToScreenBounds()
{
    const QScreen* const screen = this->screen();
    if (screen == nullptr)
    {
        return;
    }
    // Prevent the packed or restored dialog size from exceeding the usable screen area.
    const QRect screenBounds = screen->availableGeometry();
    const int   maxWidth     = std::max(0, screenBounds.width() - kScreenMargin);
    const int   maxHeight    = std::max(0, screenBounds.height() - kScreenMargin);
    if (maxWidth == 0 || maxHeight == 0)
    {
        return;
    }
    resize(std::min(width(), maxWidth), std::min(height(), maxHeight));
}

}  // namespace QtRocket::Gui
