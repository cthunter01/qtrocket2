#include "QtRocket/gui/widgets/DropdownButton.h"

#include <QAction>
#include <QMenu>
#include <QPoint>
#include <QPushButton>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/util/MakeChild.h"

namespace QtRocket::Gui
{

QString DropdownButton::indicator()
{
    return QStringLiteral("  | \u25BC ");
}

DropdownButton::DropdownButton(const QString& text, QWidget* parent)
  : QPushButton(text + indicator(), parent), m_menu(makeChild<QMenu>(this))
{
    // Show popup menu when button is clicked
    connect(this, &QPushButton::clicked, this, [this] { showPopup(); });
}

QAction* DropdownButton::addMenuItem(const QString& text)
{
    auto* item = makeChild<QAction>(m_menu, text);
    m_menu->addAction(item);
    return item;
}

void DropdownButton::addMenuItem(QAction* action)
{
    m_menu->addAction(action);
}

void DropdownButton::removeAllMenuItems()
{
    // QMenu::clear() deletes the actions the menu owns that no other widget shows.
    m_menu->clear();
}

void DropdownButton::showPopup()
{
    m_menu->popup(mapToGlobal(QPoint(0, height())));
}

}  // namespace QtRocket::Gui
