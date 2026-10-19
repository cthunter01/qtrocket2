#include "QtRocket/gui/widgets/ConfigurationComboBox.h"

#include <cstddef>
#include <vector>

#include <QComboBox>
#include <QString>
#include <QWheelEvent>
#include <QWidget>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

ConfigurationComboBox::ConfigurationComboBox(Rocket& rocket, const Preferences& preferences,
                                             QWidget* parent)
  : ConfigurationComboBox(rocket, preferences, true, parent)
{
}

ConfigurationComboBox::ConfigurationComboBox(Rocket& rocket, const Preferences& preferences,
                                             bool updateRocketConfig, QWidget* parent)
  : QComboBox(parent),
    m_rocket(&rocket),
    m_preferences(&preferences),
    m_updateRocketConfig(updateRocketConfig),
    m_selectedId(rocket.getSelectedConfiguration().getId())
{
    // Java's preferred width is that of the widest name, whatever the names are now.
    setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(this, &QComboBox::activated, this,
            guarded([this](int index) { itemActivated(index); },
                    QStringLiteral("Flight configuration")));
    m_rocketConnection = rocket.addComponentChangeListener(
        guarded([this] { refresh(); }, QStringLiteral("Flight configuration")));
    refresh();
}

FlightConfigurationId ConfigurationComboBox::selectedConfigurationId() const
{
    if (m_updateRocketConfig)
    {
        return m_rocket->getSelectedConfiguration().getId();
    }
    return m_selectedId;
}

const FlightConfigurationId& ConfigurationComboBox::configurationIdAt(int index) const
{
    QTROCKET_ASSERT(index >= 0 && static_cast<std::size_t>(index) < m_itemIds.size());
    return m_itemIds[static_cast<std::size_t>(index)];
}

void ConfigurationComboBox::setSelectedConfiguration(const FlightConfigurationId& id)
{
    if (m_updateRocketConfig)
    {
        m_rocket->setSelectedConfiguration(id);
    }
    else
    {
        // Java keeps rkt.getFlightConfiguration(id): the default when there is no such one.
        m_selectedId = m_rocket->getFlightConfiguration(id).getId();
    }
    updateCurrentIndex();
}

void ConfigurationComboBox::refresh()
{
    // The model of Java's ConfigurationModel: the default configuration and every other one.
    const int itemCount = m_rocket->getConfigurationCount() + 1;
    m_itemIds.clear();
    m_itemIds.reserve(static_cast<std::size_t>(itemCount));
    while (count() > itemCount)
    {
        removeItem(count() - 1);
    }
    for (int i = 0; i < itemCount; ++i)
    {
        const FlightConfiguration& configuration = m_rocket->getFlightConfigurationByIndex(i, true);
        const QString text = QtStrings::fromUtf8(configuration.getName(*m_preferences));
        m_itemIds.push_back(configuration.getId());
        if (i < count())
        {
            if (itemText(i) != text)
            {
                setItemText(i, text);
            }
        }
        else
        {
            addItem(text);
        }
    }
    if (!m_updateRocketConfig)
    {
        // An own selection the rocket no longer has is its default configuration.
        m_selectedId = m_rocket->getFlightConfiguration(m_selectedId).getId();
    }
    updateCurrentIndex();
}

void ConfigurationComboBox::showPopup()
{
    // Java builds its model again each time the popup opens.
    refresh();
    QComboBox::showPopup();
}

void ConfigurationComboBox::wheelEvent(QWheelEvent* event)
{
    event->ignore();
}

void ConfigurationComboBox::itemActivated(int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_itemIds.size())
    {
        return;
    }
    const FlightConfigurationId id = m_itemIds[static_cast<std::size_t>(index)];
    setSelectedConfiguration(id);
    Q_EMIT configurationActivated(selectedConfigurationId());
}

void ConfigurationComboBox::updateCurrentIndex()
{
    const FlightConfigurationId selected = selectedConfigurationId();
    int                         current  = 0;
    for (std::size_t i = 0; i < m_itemIds.size(); ++i)
    {
        if (m_itemIds[i] == selected)
        {
            current = static_cast<int>(i);
            break;
        }
    }
    if (currentIndex() != current)
    {
        setCurrentIndex(current);
    }
}

}  // namespace QtRocket::Gui
