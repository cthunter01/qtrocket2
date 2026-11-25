#include "QtRocket/gui/dialogs/LaunchPreferencesPanel.h"

#include <memory>
#include <utility>

#include <QMargins>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/dialogs/simulation/SimulationConditionsPanel.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/simulation/LaunchPreferences.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// MigLayout's paragraph gap ("gapbottom para").
constexpr int kParagraphGap = 12;

/// @p launch, which must not be null.
[[nodiscard]] std::shared_ptr<LaunchPreferences> checkedLaunch(
    std::shared_ptr<LaunchPreferences> launch)
{
    if (launch == nullptr)
    {
        bug("The Launch preference page needs the launch preferences");
    }
    return launch;
}

}  // namespace

LaunchPreferencesPanel::LaunchPreferencesPanel(std::shared_ptr<LaunchPreferences> launch,
                                               UserPrompts& prompts, BindingScope& scope,
                                               QWidget* parent)
  : QWidget(parent),
    m_launch(checkedLaunch(std::move(launch))),
    m_launchWarning(makeChild<StyledLabel>(
        this,
        QStringLiteral("<html>") +
            //: pref.dlg.lbl.launchWarning
            tr("NOTE: the settings below apply only to newly created simulations. <br>To change "
               "the settings for an existing simulation, select that simulation and edit it.") +
            QStringLiteral("</html>"),
        kWarningSize, StyledLabel::Style::BOLD)),
    // Simulation conditions
    m_conditions(makeChild<SimulationConditionsPanel>(this, *m_launch, prompts, scope))
{
    setObjectName(QStringLiteral("launchPreferencesPanel"));
    // MigLayout("fillx, ins 30lp n n n").
    auto*    layout  = makeChild<QVBoxLayout>(this);
    QMargins margins = layout->contentsMargins();
    margins.setTop(kTopInset);
    layout->setContentsMargins(margins);
    layout->setSpacing(0);

    // Warning message
    m_launchWarning->setObjectName(QStringLiteral("launchWarning"));
    m_launchWarning->setFontColor(ThemeColors::color(ThemeColors::Key::DARK_ERROR, palette()));
    //: pref.dlg.lbl.launchWarning.ttip
    m_launchWarning->setToolTip(tr("You would not be the first one to be confused by this :)"));
    // "spanx, growx 0, gapbottom para, wrap".
    layout->addWidget(m_launchWarning, 0, Qt::AlignLeft);
    layout->addSpacing(kParagraphGap);

    layout->addWidget(m_conditions);
    layout->addStretch(1);
}

LaunchPreferencesPanel::~LaunchPreferencesPanel() = default;

}  // namespace QtRocket::Gui
