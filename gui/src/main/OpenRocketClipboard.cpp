#include "QtRocket/gui/main/OpenRocketClipboard.h"

#include <memory>
#include <utility>

#include <QObject>

#include "QtRocket/document/ComponentCopy.h"
#include "QtRocket/document/SimulationCopy.h"
#include "QtRocket/gui/simulation/SimulationClipboard.h"

namespace QtRocket::Gui
{

OpenRocketClipboard::OpenRocketClipboard(QObject* parent)
  : SimulationClipboard(parent), m_simulations(std::make_shared<const SimulationCopy>())
{
}

OpenRocketClipboard::~OpenRocketClipboard() = default;

OpenRocketClipboard& OpenRocketClipboard::instance()
{
    // Made on first use, after the application's QApplication, and destroyed at exit (Java: a
    // class with static members).
    static OpenRocketClipboard s_clipboard;
    return s_clipboard;
}

std::shared_ptr<const SimulationCopy> OpenRocketClipboard::simulations() const
{
    return m_simulations;
}

bool OpenRocketClipboard::isEmpty() const noexcept
{
    return m_components.empty() && m_simulations->empty();
}

void OpenRocketClipboard::setComponents(ComponentCopy components)
{
    m_components  = std::move(components);
    m_simulations = std::make_shared<const SimulationCopy>();
    Q_EMIT changed();
}

void OpenRocketClipboard::setSimulations(SimulationCopy simulations)
{
    m_simulations = std::make_shared<const SimulationCopy>(std::move(simulations));
    m_components  = ComponentCopy();
    Q_EMIT changed();
}

void OpenRocketClipboard::clear()
{
    m_components  = ComponentCopy();
    m_simulations = std::make_shared<const SimulationCopy>();
    Q_EMIT changed();
}

}  // namespace QtRocket::Gui
