#pragma once

#include <QObject>
#include <QPushButton>
#include <QString>

class QAction;
class QMenu;
class QWidget;

namespace QtRocket::Gui
{

/// A button that shows a menu when clicked (OpenRocket's DropdownButton, swing
/// gui/widgets/DropdownButton.java): its text followed by the indicator "  | \u25BC ", the menu
/// popping up below the button's left edge. The fin editor's "Point Actions..." button.
///
/// Deviations from OpenRocket: the margins are the platform style's (Java narrows the right
/// margin to 4 pixels); the menu's items are QActions (addMenuItem() returns the action to
/// connect to; Java's take an ActionListener or a JMenuItem).
class DropdownButton final : public QPushButton
{
    Q_OBJECT

public:
    /// The indicator after the text (DROPDOWN_INDICATOR).
    [[nodiscard]] static QString indicator();

    /// A button showing @p text and the indicator, with an empty menu.
    DropdownButton(const QString& text, QWidget* parent);

    /// Adds an item @p text to the menu: the action, whose triggered() the caller connects
    /// (addMenuItem(String, ActionListener)).
    QAction* addMenuItem(const QString& text);
    /// Adds @p action, which the menu does not take over, to the menu (addMenuItem(JMenuItem)).
    void addMenuItem(QAction* action);
    /// Removes every item (removeAllMenuItems()), deleting those addMenuItem(text) made
    /// (QMenu::clear()).
    void removeAllMenuItems();

    /// The menu (getPopupMenu()).
    [[nodiscard]] QMenu& popupMenu() const noexcept { return *m_menu; }

    /// Shows the menu below the button (what a click does).
    void showPopup();

private:
    QMenu* m_menu;
};

}  // namespace QtRocket::Gui
