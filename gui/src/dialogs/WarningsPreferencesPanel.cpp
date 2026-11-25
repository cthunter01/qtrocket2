#include "QtRocket/gui/dialogs/WarningsPreferencesPanel.h"

#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "QtRocket/gui/dialogs/simulation/RecoveryWarningsPanel.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/preferences/Preferences.h"

namespace QtRocket::Gui
{

WarningsPreferencesPanel::WarningsPreferencesPanel(Preferences& preferences, BindingScope& scope,
                                                   QWidget* parent)
  : QWidget(parent), m_recovery(makeChild<RecoveryWarningsPanel>(this, preferences, scope))
{
    setObjectName(QStringLiteral("warningsPreferencesPanel"));
    // Java: MigLayout("fillx", "[grow]"), the panel "growx, wmin 0"; it keeps its height.
    auto* layout = makeChild<QVBoxLayout>(this);
    layout->addWidget(m_recovery);
    layout->addStretch(1);
}

WarningsPreferencesPanel::~WarningsPreferencesPanel() = default;

}  // namespace QtRocket::Gui
