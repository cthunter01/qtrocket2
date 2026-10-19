#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"

#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include <QDialog>
#include <QGuiApplication>
#include <QLatin1Char>
#include <QList>
#include <QMoveEvent>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/configdialog/ConfigPanelFactory.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The application's one dialog and what Java keeps in the class's statics.
struct DialogState
{
    /// The open dialog (Java: the static dialog).
    QPointer<ComponentConfigDialog> dialog;
    /// Whether a dialog has been open (Java: dialog != null), and the kind of its main component
    /// and its selected tab when it closed.
    bool          hadDialog = false;
    ComponentKind lastKind  = ComponentKind::ROCKET;
    QString       lastTabName;
    /// The tab the next dialog selects (Java: previousSelectedTab); a null string selects the
    /// first.
    QString previousSelectedTab;
};

[[nodiscard]] DialogState& dialogState()
{
    static DialogState s_state;
    return s_state;
}

}  // namespace

void ComponentConfigDialog::showDialog(QWidget& window, const GuiServices& services,
                                       OpenRocketDocument& document, RocketComponent& main,
                                       std::span<RocketComponent* const> others,
                                       bool isNewComponent, bool rememberPreviousTab)
{
    // The open dialog closes first (Java), committing its pending edits: a stage's new name is a
    // TREE change, which would end a selection made before it.
    if (ComponentConfigDialog* open = dialog())
    {
        open->ok();
    }
    DialogState& state     = dialogState();
    auto         selection = std::make_shared<ComponentSelection>(main, others);
    if (&selection->getRocket() != &document.getRocket())
    {
        bug("The configuration dialog edits components of its document's rocket");
    }
    if (state.hadDialog)
    {
        // Not for rockets or stages, which would leave the next component in the Override tab.
        if (state.lastKind == ComponentKind::ROCKET ||
            (isStage(state.lastKind) && !isStage(main.kind())))
        {
            state.previousSelectedTab = QString();
        }
        else
        {
            state.previousSelectedTab = state.lastTabName;
        }
    }
    if (services.preferences().isAlwaysOpenLeftmostTab() || !rememberPreviousTab)
    {
        state.previousSelectedTab = QString();
    }

    const bool multiEdit = selection->isMultiEdit();
    auto*      dialog    = makeChild<ComponentConfigDialog>(&window, Key(), services, document,
                                                            std::move(selection), isNewComponent);
    state.dialog         = dialog;
    state.hadDialog      = true;
    dialog->show();

    // One undo position per session, none for a new component (its add button made one).
    if (!isNewComponent)
    {
        if (multiEdit)
        {
            //: ComponentCfgDlg.ModifyComponents
            document.addUndoPosition(QtStrings::toUtf8(tr("Modify components")));
        }
        else
        {
            //: ComponentCfgDlg.Modify ("Modify" and the component's type name)
            document.addUndoPosition(QtStrings::toUtf8(tr("Modify %1").arg(enumText(main.kind()))));
        }
    }
    // The user's decision U4: what happened before the position does not count.
    dialog->startCountingModifications();
}

void ComponentConfigDialog::disposeDialog()
{
    if (ComponentConfigDialog* open = dialog())
    {
        open->ok();
    }
}

bool ComponentConfigDialog::isDialogVisible()
{
    const ComponentConfigDialog* open = dialog();
    return open != nullptr && open->isVisible();
}

ComponentConfigDialog* ComponentConfigDialog::dialog()
{
    ComponentConfigDialog* open = dialogState().dialog.data();
    return open != nullptr && !open->m_disposed ? open : nullptr;
}

void ComponentConfigDialog::forgetPreviousDialogs()
{
    DialogState& state        = dialogState();
    state.hadDialog           = false;
    state.lastKind            = ComponentKind::ROCKET;
    state.lastTabName         = QString();
    state.previousSelectedTab = QString();
}

bool ComponentConfigDialog::commitPendingEdits()
{
    ComponentConfigDialog* open = dialog();
    return open != nullptr && open->m_configurator->commitAll();
}

ComponentConfigDialog::ComponentConfigDialog(Key /*key*/, const GuiServices& services,
                                             OpenRocketDocument&                 document,
                                             std::shared_ptr<ComponentSelection> selection,
                                             bool isNewComponent, QWidget* window)
  : QDialog(window),
    m_services(services),
    m_document(&document),
    m_mainKind(selection->getMain().kind()),
    m_newComponent(isNewComponent)
{
    setObjectName(QString::fromLatin1(kGeometryName));
    setModal(false);
    // The rocket's listener before the panel, as Java: the panel's own changes reach it (and are
    // not counted, see startCountingModifications()).
    m_rocketConnection = document.getRocket().addComponentChangeListener(
        [this](const ComponentChangeEvent& event) { componentChanged(event); });

    const bool allSameType = selection->isAllOfOneKind();
    const bool multiEdit   = selection->isMultiEdit();
    m_configurator = ConfigPanelFactory::create(document, m_services, std::move(selection), this);
    m_configurator->setNewComponent(isNewComponent);
    auto* layout = makeChild<QVBoxLayout>(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_configurator);
    m_configurator->updateFields();

    if (allSameType)
    {
        m_configurator->setSelectedTab(dialogState().previousSelectedTab);
    }
    else
    {
        // Java selects tab 1, the Appearance tab (Milestone 5); here the first.
        m_configurator->setSelectedTabIndex(0);
    }

    const QString name = enumText(m_mainKind);
    if (!allSameType)
    {
        //: ComponentCfgDlg.MultiComponentConfig
        setWindowTitle(tr("Multi-component configuration"));
    }
    else if (multiEdit)
    {
        //: ComponentCfgDlg.MultiComponent
        setWindowTitle(QLatin1Char('(') + tr("Multi-component") + QStringLiteral(") ") +
                       //: ComponentCfgDlg.configuration (after the component's type name)
                       tr("%1 configuration").arg(name));
    }
    else
    {
        //: ComponentCfgDlg.configuration (after the component's type name)
        setWindowTitle(tr("%1 configuration").arg(name));
    }

    connect(&m_configurator->okButton(), &QPushButton::clicked, this,
            guarded([this] { ok(); }, QStringLiteral("okButton")));
    connect(&m_configurator->cancelButton(), &QPushButton::clicked, this,
            guarded([this] { cancel(); }, QStringLiteral("cancelButton")));
    m_configurator->orderFocus();

    // The size is the contents' (Java: pack()), the position the one remembered.
    adjustSize();
    restorePosition();
}

ComponentConfigDialog::~ComponentConfigDialog()
{
    // Closed with its window: as dispose(), without hiding (Java: windowClosed()).
    if (!m_disposed)
    {
        DialogState& state = dialogState();
        state.lastKind     = m_mainKind;
        state.lastTabName  = selectedTabName();
        m_rocketConnection.disconnect();
        m_configurator->release();
    }
}

std::string ComponentConfigDialog::positionKey()
{
    return std::string(kPositionKey);
}

void ComponentConfigDialog::restorePosition()
{
    const Preferences* node = m_services.preferences().findNode(GuiPreferenceKeys::kNodeWindows);
    if (node == nullptr)
    {
        return;
    }
    const std::optional<std::string> text = node->get(positionKey());
    if (!text.has_value())
    {
        return;
    }
    // "x,y" (SwingPreferences.getWindowPosition()); a place on no screen is not used.
    const QList<QString> parts = QtStrings::fromUtf8(*text).split(QLatin1Char(','));
    if (parts.size() != 2)
    {
        return;
    }
    bool         xValid = false;
    bool         yValid = false;
    const QPoint position(parts.at(0).trimmed().toInt(&xValid),
                          parts.at(1).trimmed().toInt(&yValid));
    if (!xValid || !yValid || QGuiApplication::screenAt(position) == nullptr)
    {
        return;
    }
    move(position);
}

void ComponentConfigDialog::moveEvent(QMoveEvent* event)
{
    QDialog::moveEvent(event);
    // Every move of the shown dialog (GUIUtil.rememberWindowPosition()).
    if (isVisible() && !m_disposed)
    {
        m_services.preferences()
            .getNode(GuiPreferenceKeys::kNodeWindows)
            .put(positionKey(), std::format("{},{}", pos().x(), pos().y()));
    }
}

QString ComponentConfigDialog::selectedTabName() const
{
    return m_configurator->selectedTabName();
}

void ComponentConfigDialog::componentChanged(const ComponentChangeEvent& event)
{
    if (m_disposed)
    {
        return;
    }
    // A TREE or UNDO change: the components may go once the event is delivered.
    if (event.isTreeChange() || event.isUndoChange())
    {
        dispose();
        return;
    }
    if (m_configurator != nullptr)
    {
        m_configurator->updateFields();
    }
    if (m_counting && !m_modified)
    {
        setWindowTitle(QLatin1Char('*') + windowTitle());
        m_modified = true;
    }
}

void ComponentConfigDialog::ok()
{
    if (m_disposed)
    {
        return;
    }
    // A commit may close the dialog (a stage's new name is a TREE change).
    m_configurator->commitAll();
    dispose();
}

void ComponentConfigDialog::reject()
{
    Guard::run([this] { ok(); }, QStringLiteral("Close"));
}

void ComponentConfigDialog::cancel()
{
    if (m_disposed)
    {
        return;
    }
    // An existing component that was not modified: just close.
    if (!m_newComponent && !m_modified)
    {
        dispose();
        return;
    }
    Preferences& preferences = m_services.preferences();
    if (!preferences.isShowDiscardConfirmation())
    {
        discardChanges();
        return;
    }
    const UserPrompts::DiscardAnswer answer = m_services.prompts().askDiscardChanges(
        this, m_newComponent ? UserPrompts::DiscardQuestion::COMPONENT_ADDITION
                             : UserPrompts::DiscardQuestion::COMPONENT_CHANGES);
    // "Don't ask me again" turns the question off whatever the answer (Java).
    if (answer.doNotAskAgain)
    {
        preferences.setShowDiscardConfirmation(false);
    }
    // A dialog closed meanwhile (an undo while the question was open) undoes nothing more.
    if (answer.discard && !m_disposed)
    {
        discardChanges();
    }
}

void ComponentConfigDialog::dispose()
{
    if (m_disposed)
    {
        return;
    }
    m_disposed         = true;
    DialogState& state = dialogState();
    state.lastKind     = m_mainKind;
    state.lastTabName  = selectedTabName();
    m_rocketConnection.disconnect();
    // The bindings end before the dialog hides: the focus loss of an edited field then reaches no
    // component.
    m_configurator->release();
    hide();
    deleteLater();
}

void ComponentConfigDialog::discardChanges()
{
    OpenRocketDocument& document = *m_document;
    // Closed first, then the undo (Java): the undo replaces every component.
    dispose();
    document.undo();
}

}  // namespace QtRocket::Gui
