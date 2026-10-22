#include "QtRocket/gui/dialogs/PreferencesDialog.h"

#include <memory>
#include <span>
#include <utility>
#include <vector>

#include <QDialog>
#include <QDialogButtonBox>
#include <QObject>
#include <QPointer>
#include <QPushButton>
#include <QString>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/dialogs/DesignPreferencesPanel.h"
#include "QtRocket/gui/dialogs/GeneralPreferencesPanel.h"
#include "QtRocket/gui/dialogs/PreferencesStore.h"
#include "QtRocket/gui/dialogs/UnitsPreferencesPanel.h"
#include "QtRocket/gui/figure/RocketPanel.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/ConfigurationComboBox.h"
// The complete type of the snapshot, which m_snapshot deletes.
#include "QtRocket/preferences/InMemoryPreferences.h"  // IWYU pragma: keep
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/unit/UnitGroup.h"

namespace QtRocket::Gui
{

namespace
{

/// The context of the dialog's reports (Guard).
QString context()
{
    return QStringLiteral("Preferences");
}

}  // namespace

PreferencesDialog::PreferencesDialog(Preferences& preferences, UserPrompts& prompts,
                                     RocketPanels rocketPanels, Tab selectedTab, QWidget* parent)
  : QDialog(parent),
    m_preferences(&preferences),
    m_prompts(&prompts),
    m_rocketPanels(std::move(rocketPanels)),
    // Java keeps a rollback point before it makes the pages.
    m_snapshot(PreferencesStore::snapshot(preferences)),
    m_tabs(makeChild<QTabWidget>(this))
{
    setObjectName(QStringLiteral("preferencesDialog"));
    //: pref.dlg.title.Preferences
    setWindowTitle(tr("Preferences"));
    // Dialog.ModalityType.APPLICATION_MODAL.
    setWindowModality(Qt::ApplicationModal);

    // MigLayout("fill, gap unrel", "[grow]", "[grow][]"): the tabs take the space, the buttons
    // below them at the right.
    auto* layout = makeChild<QVBoxLayout>(this);
    m_tabs->setObjectName(QStringLiteral("tabbedPane"));
    layout->addWidget(m_tabs, 1);
    buildPages(selectedTab);

    // MigLayout's button tags (cancel, apply, ok) order the buttons as the platform does;
    // QDialogButtonBox does so by their roles.
    auto* buttons = makeChild<QDialogButtonBox>(this);
    buttons->setObjectName(QStringLiteral("buttons"));
    //: dlg.but.cancel
    m_cancel = buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    m_cancel->setObjectName(QStringLiteral("cancelButton"));
    //: SimulationConfigDialog.btn.Cancel.ttip
    m_cancel->setToolTip(tr("Discard changes and close the dialog"));
    connect(m_cancel, &QPushButton::clicked, this,
            guarded([this] { static_cast<void>(cancel()); }, context()));
    //: dlg.but.apply
    m_apply = buttons->addButton(tr("Apply"), QDialogButtonBox::ApplyRole);
    m_apply->setObjectName(QStringLiteral("applyButton"));
    //: PreferencesDialog.btn.Apply.ttip
    m_apply->setToolTip(tr("Apply changes without closing the dialog"));
    connect(m_apply, &QPushButton::clicked, this, guarded([this] { applyChanges(); }, context()));
    //: dlg.but.ok
    m_ok = buttons->addButton(tr("OK"), QDialogButtonBox::AcceptRole);
    m_ok->setObjectName(QStringLiteral("okButton"));
    //: SimulationConfigDialog.btn.OK.ttip
    m_ok->setToolTip(tr("Keep changes and close the dialog"));
    // GUIUtil.setDisposableDialogOptions(this, okButton): OK is the default button.
    m_ok->setDefault(true);
    connect(m_ok, &QPushButton::clicked, this, guarded([this] { accept(); }, context()));
    layout->addWidget(buttons);
}

PreferencesDialog::~PreferencesDialog() = default;

PreferencesDialog& PreferencesDialog::showPreferences(Preferences& preferences,
                                                      UserPrompts& prompts,
                                                      RocketPanels rocketPanels, QWidget& parent,
                                                      Tab selectedTab)
{
    auto* dialog = makeChild<PreferencesDialog>(&parent, preferences, prompts,
                                                std::move(rocketPanels), selectedTab);
    connect(dialog, &QDialog::finished, dialog, &QObject::deleteLater);
    dialog->show();
    return *dialog;
}

void PreferencesDialog::storeAndApplyPreferences(Preferences&                  preferences,
                                                 std::span<RocketPanel* const> rocketPanels)
{
    preferences.storeDefaultUnits();
    refreshRocketPanels(rocketPanels);
}

void PreferencesDialog::refreshRocketPanels(std::span<RocketPanel* const> rocketPanels)
{
    for (RocketPanel* panel : rocketPanels)
    {
        if (panel == nullptr)
        {
            continue;
        }
        panel->updateExtras();
        panel->updateFigures();
        panel->updateRulers();
        // The names of the flight configurations, which no event tells of a change of the
        // default name (ConfigurationComboBox).
        panel->configurationSelector().refresh();
    }
}

void PreferencesDialog::applyChanges()
{
    if (m_closed)
    {
        return;
    }
    m_scope->commitAll();
    storeAndApplyPreferences(*m_preferences, rocketPanels());
    // The rollback point of a later Cancel.
    m_snapshot = PreferencesStore::snapshot(*m_preferences);
}

bool PreferencesDialog::cancel()
{
    if (m_closed)
    {
        return false;
    }
    if (!m_preferences->isShowDiscardPreferencesConfirmation())
    {
        closeDialog(false);
        return true;
    }
    // The question runs an event loop, in which the dialog may go.
    const QPointer<PreferencesDialog> self(this);
    const UserPrompts::DiscardAnswer  answer =
        m_prompts->askDiscardChanges(this, UserPrompts::DiscardQuestion::PREFERENCE_CHANGES);
    if (self.isNull() || m_closed)
    {
        return false;
    }
    // Java turns the confirmation off as soon as "Don't ask me again" is ticked.
    if (answer.doNotAskAgain)
    {
        m_preferences->setShowDiscardPreferencesConfirmation(false);
    }
    if (!answer.discard)
    {
        return false;
    }
    closeDialog(false);
    return true;
}

void PreferencesDialog::accept()
{
    if (m_closed)
    {
        return;
    }
    m_scope->commitAll();
    closeDialog(true);
}

void PreferencesDialog::reject()
{
    // Java: the Escape key and the close button dispose the dialog, which stores (storePreferences
    // is still true).
    accept();
}

void PreferencesDialog::reopen()
{
    if (m_closed)
    {
        return;
    }
    // Java: showPreferences() disposes this dialog (its windowClosed listener stores) and makes a
    // new one, which takes its snapshot first.
    finishSession(true);
    while (m_tabs->count() > 0)
    {
        QWidget* const page = m_tabs->widget(0);
        m_tabs->removeTab(0);
        // Not at once: the General page's button is still in its clicked().
        page->deleteLater();
    }
    m_snapshot = PreferencesStore::snapshot(*m_preferences);
    buildPages(Tab::GENERAL);
}

void PreferencesDialog::buildPages(Tab selectedTab)
{
    m_scope = std::make_unique<BindingScope>(*this, nullptr);

    m_general = makeChild<GeneralPreferencesPanel>(m_tabs, *m_preferences, *m_prompts);
    //: pref.dlg.tab.General
    m_tabs->addTab(m_general, tr("General"));
    //: pref.dlg.tab.General.ttip
    m_tabs->setTabToolTip(m_tabs->indexOf(m_general), tr("Miscellaneous options"));
    connect(m_general, &GeneralPreferencesPanel::preferencesCleared, this,
            guarded([this] { reopen(); }, context()));

    m_design = makeChild<DesignPreferencesPanel>(m_tabs, *m_preferences, *m_scope);
    //: pref.dlg.tab.Design
    m_tabs->addTab(m_design, tr("Design"));
    // Java's tooltip of this tab is its title again.
    //: pref.dlg.tab.Design
    m_tabs->setTabToolTip(m_tabs->indexOf(m_design), tr("Design"));
    connect(m_design, &DesignPreferencesPanel::showMarkersChanged, this,
            guarded([this] { updateFigures(); }, context()));

    m_units = makeChild<UnitsPreferencesPanel>(m_tabs, *m_preferences);
    //: pref.dlg.tab.Units
    m_tabs->addTab(m_units, tr("Units"));
    //: pref.dlg.tab.Defaultunits
    m_tabs->setTabToolTip(m_tabs->indexOf(m_units), tr("Default units"));

    m_tabs->setCurrentIndex(static_cast<int>(selectedTab));
}

void PreferencesDialog::finishSession(bool store)
{
    // A late commit of a field (the focus leaving it as the dialog hides) stores nothing now.
    m_scope->release();
    // "We don't want to lose the preference for the confirmation dialog".
    const bool showDiscardConfirmation = m_preferences->isShowDiscardPreferencesConfirmation();
    if (store)
    {
        storeAndApplyPreferences(*m_preferences, rocketPanels());
    }
    else
    {
        PreferencesStore::restore(*m_preferences, *m_snapshot);
        // U4: the units of the snapshot, also where it holds none (Java only loads them).
        UnitGroup::resetDefaultUnits();
        m_preferences->loadDefaultUnits();
        refreshRocketPanels(rocketPanels());
    }
    m_preferences->setShowDiscardPreferencesConfirmation(showDiscardConfirmation);
}

void PreferencesDialog::closeDialog(bool store)
{
    if (m_closed)
    {
        return;
    }
    // First: hiding the dialog calls reject() again.
    m_closed = true;
    finishSession(store);
    QDialog::done(store ? QDialog::Accepted : QDialog::Rejected);
}

std::vector<RocketPanel*> PreferencesDialog::rocketPanels() const
{
    return m_rocketPanels ? m_rocketPanels() : std::vector<RocketPanel*>{};
}

void PreferencesDialog::updateFigures() const
{
    // Java: updateFigures() of every BasicFrame's rocket panel.
    for (RocketPanel* panel : rocketPanels())
    {
        if (panel != nullptr)
        {
            panel->updateFigures();
        }
    }
}

}  // namespace QtRocket::Gui
