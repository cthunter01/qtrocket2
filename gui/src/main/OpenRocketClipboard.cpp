#include "QtRocket/gui/main/OpenRocketClipboard.h"

#include <utility>

#include <QObject>

#include "QtRocket/document/ComponentCopy.h"

namespace QtRocket::Gui
{

OpenRocketClipboard::OpenRocketClipboard(QObject* parent) : QObject(parent) { }

OpenRocketClipboard::~OpenRocketClipboard() = default;

OpenRocketClipboard& OpenRocketClipboard::instance()
{
    // Made on first use, after the application's QApplication, and destroyed at exit (Java: a
    // class with static members).
    static OpenRocketClipboard s_clipboard;
    return s_clipboard;
}

void OpenRocketClipboard::setComponents(ComponentCopy components)
{
    m_components = std::move(components);
    Q_EMIT changed();
}

void OpenRocketClipboard::clear()
{
    m_components = ComponentCopy();
    Q_EMIT changed();
}

}  // namespace QtRocket::Gui
