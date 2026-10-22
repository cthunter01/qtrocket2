#pragma once

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QPushButton>
#include <QSize>

class QAction;
class QEvent;
class QPaintEvent;
class QWidget;

namespace QtRocket::Gui
{

/// A push button that shows an action's icon and text at its left edge (OpenRocket's IconButton
/// as DesignPanel uses it: a JButton tied to an action with RocketActions.tieActionToButton(),
/// its contents aligned left with setHorizontalAlignment(LEFT)). Buttons of one column made the
/// same width line their icons and texts up on the left.
///
/// - setAction() ties the button to an action (tieActionToButton(), JButton.setAction()): the
///   button shows the action's text (without its mnemonic: DesignPanel's setMnemonic(0), and
///   OpenRocket's Move up and Move down have none), its icon and its tooltip, is enabled as the
///   action is, follows every change of these, and a click triggers the action.
/// - The icon is 0.9 times the size the style gives a button's icon (ICON_SCALE: Java draws the
///   action's icon scaled by 0.9), kIconGap pixels before the text (ICON_GAP, JButton's
///   iconTextGap).
///
/// Deviations from OpenRocket: the icon's size is the platform style's button icon size scaled
/// (Java scales the action's icon, 1.25 times the font size); the pressed look is the style's
/// (Java gives the pressed button the scaled icon too, a work-around for a Swing bug).
class IconButton final : public QPushButton
{
    Q_OBJECT

public:
    /// The space between the icon and the text (ICON_GAP).
    static constexpr int kIconGap = 10;
    /// The icon's size as a part of the style's button icon size (ICON_SCALE).
    static constexpr double kIconScale = 0.9;

    /// A button tied to no action.
    explicit IconButton(QWidget* parent);

    /// Ties the button to @p action (null: to none, the button keeping what it shows), as
    /// described in the class comment. The action may be destroyed first: the button then keeps
    /// what it showed last and does nothing.
    void setAction(QAction* action);

    /// The action the button is tied to; null when none (or when it was destroyed).
    [[nodiscard]] QAction* action() const noexcept { return m_action.data(); }

    /// QPushButton's size for the icon and the text, with kIconGap between them.
    [[nodiscard]] QSize sizeHint() const override;

protected:
    /// The style's button with the icon and the text at its left edge.
    void paintEvent(QPaintEvent* event) override;
    /// Sizes the icon again when the style changes.
    void changeEvent(QEvent* event) override;

private:
    /// Shows what the action has now (text, icon, tooltip, enabled state).
    void showAction();
    /// Gives the icon kIconScale of the style's button icon size.
    void scaleIcon();

    QPointer<QAction>       m_action;
    QMetaObject::Connection m_actionChanged;
};

}  // namespace QtRocket::Gui
