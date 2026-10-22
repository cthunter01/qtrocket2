#include "QtRocket/gui/configdialog/ClusterSelectionPanel.h"

#include <cstddef>
#include <memory>
#include <span>

#include <QGridLayout>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/configdialog/ClusterButton.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/rocket/ClusterConfiguration.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The number of buttons in a row (Java: a wrap after every fourth button).
constexpr int kColumns = 4;

}  // namespace

ClusterSelectionPanel::ClusterSelectionPanel(BindingScope& scope, QWidget* parent)
  : QWidget(parent), m_selection(scope.selection())
{
    if (m_selection == nullptr || !m_selection->isValid())
    {
        bug("The cluster selection needs a valid selection");
    }
    const auto* tube = dynamic_cast<const InnerTube*>(&m_selection->getMain());
    if (tube == nullptr)
    {
        bug("The cluster selection needs an inner tube");
    }
    setObjectName(QStringLiteral("ClusterConfiguration"));
    m_selected = &tube->getClusterConfiguration();

    auto* grid = makeChild<QGridLayout>(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(0);
    const std::span<const ClusterConfiguration> configurations =
        ClusterConfiguration::configurations();
    for (std::size_t i = 0; i < configurations.size(); ++i)
    {
        const ClusterConfiguration& configuration = configurations[i];
        auto*                       button        = makeChild<ClusterButton>(this, configuration);
        const int                   index         = static_cast<int>(i);
        grid->addWidget(button, index / kColumns, index % kColumns);
        connect(button, &ClusterButton::clicked, this,
                guarded([this, &configuration] { select(configuration); },
                        QStringLiteral("ClusterConfiguration ") +
                            QtStrings::fromUtf8(configuration.getXmlName())));
        m_buttons.append(button);
    }
    showSelected();

    m_connection = m_selection->addListener(
        [this](ComponentSelection::Phase phase) { selectionChanged(phase); });
}

int ClusterSelectionPanel::buttonCount() const noexcept
{
    return static_cast<int>(m_buttons.size());
}

ClusterButton& ClusterSelectionPanel::button(int index) const
{
    if (index < 0 || index >= m_buttons.size())
    {
        bug("No cluster button at this index");
    }
    return *m_buttons.at(index);
}

void ClusterSelectionPanel::select(const ClusterConfiguration& configuration)
{
    if (!m_selection->isValid())
    {
        return;
    }
    m_selection->apply<InnerTube>(
        [&configuration](InnerTube& tube) { tube.setClusterConfiguration(configuration); });
}

void ClusterSelectionPanel::selectionChanged(ComponentSelection::Phase phase)
{
    switch (phase)
    {
        case ComponentSelection::Phase::READ:
            if (const auto* tube = dynamic_cast<const InnerTube*>(&m_selection->getMain()))
            {
                m_selected = &tube->getClusterConfiguration();
            }
            return;
        case ComponentSelection::Phase::NOTIFY:
            showSelected();
            return;
        case ComponentSelection::Phase::INVALIDATED:
            m_selected = nullptr;
            showSelected();
            return;
    }
}

void ClusterSelectionPanel::showSelected()
{
    for (ClusterButton* button : m_buttons)
    {
        button->setSelected(&button->configuration() == m_selected);
    }
}

}  // namespace QtRocket::Gui
