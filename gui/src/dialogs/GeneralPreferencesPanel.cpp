#include "QtRocket/gui/dialogs/GeneralPreferencesPanel.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QMargins>
#include <QPointer>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "QtRocket/gui/dialogs/PreferencesStore.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"

namespace QtRocket::Gui
{

namespace
{

/// The page's top inset (MigLayout "ins 30lp n n n").
constexpr int kTopInset = 30;
/// The gap above the reset button ("gaptop 20lp").
constexpr int kResetGap = 20;

/// The context of the page's reports (Guard).
QString context()
{
    return QStringLiteral("General preferences");
}

}  // namespace

GeneralPreferencesPanel::GeneralPreferencesPanel(Preferences& preferences, UserPrompts& prompts,
                                                 QWidget* parent)
  : QWidget(parent), m_preferences(&preferences), m_prompts(&prompts)
{
    setObjectName(QStringLiteral("generalPreferencesPanel"));
    // MigLayout("fillx, ins 30lp n n n"): one item a row, the buttons at the bottom ("pushy,
    // bottom").
    auto*    layout  = makeChild<QVBoxLayout>(this);
    QMargins margins = layout->contentsMargins();
    margins.setTop(kTopInset);
    layout->setContentsMargins(margins);

    // Java's ActionListener: the user's clicks only.
    //: pref.dlg.but.openlast
    m_openLast = makeChild<QCheckBox>(this, tr("Open last design file on startup"));
    m_openLast->setObjectName(QtStrings::fromUtf8(PreferenceKeys::kAutoOpenLastDesign));
    m_openLast->setChecked(preferences.isAutoOpenLastDesignOnStartupEnabled());
    connect(
        m_openLast, &QCheckBox::clicked, this,
        guarded([this](bool checked) { m_preferences->setAutoOpenLastDesignOnStartup(checked); },
                context()));
    layout->addWidget(m_openLast);

    // Java's ItemListener: every change of the state.
    //: pref.dlg.checkbox.ShowDiscardPreferencesConfirmation
    m_discard =
        makeChild<QCheckBox>(this, tr("Show confirmation dialog when discarding preferences"));
    m_discard->setObjectName(
        QtStrings::fromUtf8(PreferenceKeys::kShowDiscardPreferencesConfirmation));
    m_discard->setChecked(preferences.isShowDiscardPreferencesConfirmation());
    connect(
        m_discard, &QCheckBox::toggled, this,
        guarded(
            [this](bool checked) { m_preferences->setShowDiscardPreferencesConfirmation(checked); },
            context()));
    layout->addWidget(m_discard);
    layout->addStretch(1);

    // The buttons' panel (JPanel(new MigLayout("fillx, ins 0"))): the reset button at its right
    // ("pushx, right, gaptop 20lp").
    auto* buttonPanel = makeChild<QWidget>(this);
    buttonPanel->setObjectName(QStringLiteral("buttonPanel"));
    auto* buttons = makeChild<QHBoxLayout>(buttonPanel);
    buttons->setContentsMargins(0, kResetGap, 0, 0);
    buttons->addStretch(1);
    //: pref.dlg.but.resetAllPreferences
    m_reset = makeChild<QPushButton>(buttonPanel, tr("Reset all preferences"));
    m_reset->setObjectName(QStringLiteral("resetAllPreferences"));
    //: pref.dlg.but.resetAllPreferences.ttip
    m_reset->setToolTip(
        tr("Reset all the preferences, including cached preferences (UI "
           "settings, recent files, etc.)"));
    connect(m_reset, &QPushButton::clicked, this,
            guarded([this] { static_cast<void>(resetAllPreferences()); }, context()));
    buttons->addWidget(m_reset);
    layout->addWidget(buttonPanel);
}

GeneralPreferencesPanel::~GeneralPreferencesPanel() = default;

bool GeneralPreferencesPanel::resetAllPreferences()
{
    // The question and the message run event loops, in which the page may go with its dialog's
    // window: nothing of it is used once it is gone.
    const QPointer<GeneralPreferencesPanel> self(this);
    // JOptionPane.showConfirmDialog(parent, ..., YES_NO_OPTION, WARNING_MESSAGE), the parent the
    // dialog.
    const bool reset = m_prompts->askYesNo(
        window(), {.severity = UserPrompts::Severity::WARNING,
                   //: pref.dlg.clearCachedPreferences.title
                   .title = tr("Reset preferences?"),
                   //: pref.dlg.clearCachedPreferences.message
                   .text = tr("Are you sure you want to reset all your preferences?")});
    if (!reset || self.isNull())
    {
        return false;
    }
    PreferencesStore::clearPreferences(*m_preferences);
    // OpenRocket's texts with QtRocket's name.
    m_prompts->showMessage(
        window(),
        {.severity = UserPrompts::Severity::WARNING,
         //: generalprefs.ImportWarning.title
         .title = tr("Reload QtRocket"),
         //: generalprefs.ImportWarning.msg
         .lines = {
             tr("You may need to restart QtRocket for some of the changes to take effect.")}});
    if (self.isNull())
    {
        // The preferences are reset; there is no dialog left to show them anew.
        return true;
    }
    Q_EMIT preferencesCleared();
    return true;
}

}  // namespace QtRocket::Gui
