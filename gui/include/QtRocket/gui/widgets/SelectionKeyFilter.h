#pragma once

#include <QObject>

class QEvent;
class QKeyEvent;
class QLineEdit;
class QPlainTextEdit;
class QTextEdit;
class QWidget;

namespace QtRocket::Gui
{

/// Left and Right with a selection put the cursor at the start and the end of the selection
/// (OpenRocket's TextComponentSelectionKeyListener, on its spinners' editors and on the name and
/// comment fields of the component dialogs). Without it the cursor moves one character from where
/// it is, as Swing's text components do, and as Qt's do under the Windows keyboard scheme (on
/// Windows, and on the offscreen platform the tests run on); Qt's other schemes collapse the
/// selection themselves, and the filter gives the same result there.
///
/// The rule is Java's, step for step: unless Shift is held, a Left (also on the keypad) with a
/// selection first puts the cursor one character after the selection's start, a Right one
/// character before its end, without a selection; the key then does what it always does, so the
/// plain key ends at the selection's edge and Ctrl+Left goes on to the previous word from there.
/// The event is never consumed.
///
/// A filter is installed on one widget, which owns it: a QLineEdit, a QPlainTextEdit, a QTextEdit
/// or a spin box (QAbstractSpinBox), whose keys arrive at the spin box and act on its line edit.
class SelectionKeyFilter : public QObject
{
    Q_OBJECT

public:
    /// Installs a filter on @p widget, which owns it.
    /// @throws BugError when @p widget is none of the kinds above
    static void install(QWidget& widget);

    /// Java's rule for the key press @p event on @p edit: moves the cursor as described above when
    /// the key is Left or Right without Shift and @p edit has a selection.
    static void prepareKey(QLineEdit& edit, const QKeyEvent& event);
    static void prepareKey(QPlainTextEdit& edit, const QKeyEvent& event);
    static void prepareKey(QTextEdit& edit, const QKeyEvent& event);

    /// A filter owned by @p parent; install() installs it as well.
    explicit SelectionKeyFilter(QWidget* parent);

    /// Applies the rule to a key press on @p watched; the event always goes on (false).
    bool eventFilter(QObject* watched, QEvent* event) override;
};

}  // namespace QtRocket::Gui
