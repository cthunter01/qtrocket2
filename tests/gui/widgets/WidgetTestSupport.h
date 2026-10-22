#pragma once

// Helpers of the tests of the editing widgets (tests/gui/widgets): the focus, typing and the
// mouse wheel, as a user gives them.

#include <initializer_list>

#include <QString>

#include "ExpectedQtMessage.h"

class QAbstractSpinBox;
class QLineEdit;
class QWidget;

namespace QtRocket::Gui::Test
{

/// The line edit of @p spinBox (QAbstractSpinBox::lineEdit() is protected): its direct QLineEdit
/// child; null when it has none.
[[nodiscard]] QLineEdit* editorOf(QAbstractSpinBox const& spinBox);

/// Shows @p window and makes it the active window, which the focus needs (offscreen as well):
/// whether it became active.
[[nodiscard]] bool showActive(QWidget& window);

/// Lays @p widgets, children of @p window, out in a column of @p window and shows it active
/// (showActive()). A QDialog gets the OK and Cancel buttons below them too (OK its default
/// button), which accept and reject it.
[[nodiscard]] bool showActiveWith(QWidget& window, std::initializer_list<QWidget*> widgets);

/// Gives @p spinBox the focus, selects its whole text and types @p text over it, as a user
/// replacing the value: the keys go to the spin box, which hands them to its line edit.
void typeInto(QAbstractSpinBox& spinBox, const QString& text);

/// Turns the mouse wheel by @p delta (120: one notch up) over the middle of @p widget: whether
/// @p widget accepted the turn. (Qt passes an ignored turn on to the parent only when it comes
/// from the platform: this one stops at @p widget.)
[[nodiscard]] bool turnWheel(QWidget& widget, int delta);

/// The warnings of the offscreen platform when a popup (a QMenu) opens, expected while it lives:
/// it cannot raise a window or grab the keyboard (and, depending on Qt's version, the mouse), of
/// which Qt warns on every popup. At least one of them must come.
struct OffscreenPopupWarnings
{
    ExpectedQtMessage unsupported{QStringLiteral(
        "^This plugin does not support (raise\\(\\)|grabbing the keyboard|grabbing the mouse)$")};
};

}  // namespace QtRocket::Gui::Test
