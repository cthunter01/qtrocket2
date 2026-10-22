#pragma once

#include <functional>

#include <QPointer>

class QAbstractSpinBox;
class QFocusEvent;
class QKeyEvent;

/// The editing rules OpenRocket's spinners follow (its SpinnerEditor, the JFormattedTextField of
/// the spinner and Swing's BasicSpinnerUI), shared by UnitSpinBox and IntSpinBox, which call them
/// from their own event handlers. They are not an event filter: a widget's destructor still sends
/// it a focus-out (QWidget::clearFocus()), which a filter would see after the spin box's own part
/// is gone, and a commit then would reach a spin box that no longer exists; a virtual event
/// handler is no longer called by then.
///
/// - Enter and Return commit the edited text, select all and keep the focus, and are accepted:
///   otherwise a QDialog presses its default button and closes (QAbstractSpinBox lets them pass
///   on). Java commits and clears the focus owner.
/// - Esc reverts an edited text and is accepted (Java: JFormattedTextField's reset-field-edit,
///   enabled only while the field is edited); an unedited spin box lets it pass on to the dialog,
///   which closes, as in Java.
/// - Focus loss commits an edited text only, so that tabbing through a dialog loses no precision
///   (Java: COMMIT_OR_REVERT on an edited field only). No commit, whatever ends the edit, sets the
///   value the spin box shows (the same text, or the same count): Java's text field fires no value
///   change for a value equal to its own (JFormattedTextField.commitEdit() -> setValue()), so its
///   model's setter never runs; each spin box's commitEdit() checks it.
/// - The arrow buttons and the Up and Down keys commit the edited text first, then step from the
///   committed value (Java: BasicSpinnerUI's ArrowButtonHandler); each spin box's stepBy() does
///   it. PageUp and PageDown step ten times (Qt's spin boxes; Java binds no key to them).
/// - The wheel steps only a spin box that has the focus, and a turn over one that has none goes
///   to the widgets around it (a scroll area scrolls); the spin box takes the focus from a click
///   or Tab, never from the wheel (Java's spinners take no wheel at all).
/// - The whole text is selected when the focus comes in, also by a click: after the click has put
///   the cursor, as Java's invokeLater(selectAll) does it (a second click then places the cursor);
///   not when the focus comes back from a popup menu, which in Swing takes no focus.
/// - Left and Right collapse a selection to its edges (SelectionKeyFilter).
namespace QtRocket::Gui::SpinBoxEditing
{

/// What a key press does in a spin box (keyAction()).
enum class KeyAction
{
    /// Commit the edited text, select all, accept the key (Enter, Return).
    COMMIT,
    /// Show the binding's value again and accept the key (Esc on an edited text).
    REVERT,
    /// Ignore the key without handing it to the spin box, so that it goes on to the parent: the
    /// dialog (Esc on an unedited text).
    PASS_ON,
    /// The spin box's own handling: editing, the arrows' steps.
    EDIT,
};

/// The settings of the rules: strong focus (a click or Tab, never the wheel), no keyboard
/// tracking (the text is read when it is committed, never while it is typed), no acceleration of
/// held arrows, and the SelectionKeyFilter.
void setUp(QAbstractSpinBox& spinBox);

/// What @p event does in a spin box whose text has (@p edited) or has not been edited since the
/// spin box last showed its binding's value.
[[nodiscard]] KeyAction keyAction(const QKeyEvent& event, bool edited) noexcept;

/// Whether a turn of the wheel may step @p spinBox: only while it has the focus.
[[nodiscard]] bool takesWheel(const QAbstractSpinBox& spinBox);

/// Selects the whole text of @p spinBox once the focus came in with @p event, deferred until the
/// events in hand (the click that gave the focus) are done; nothing for a popup's return.
void selectAllOnFocus(QAbstractSpinBox& spinBox, const QFocusEvent& event);

/// Steps @p steps times, up when it is positive and down when it is negative, through
/// @p stepOnce (its argument: up), until it returns false (a bound) or @p spinBox is deleted by
/// what a step set off.
void repeatSteps(const QPointer<QAbstractSpinBox>& spinBox, int steps,
                 const std::function<bool(bool up)>& stepOnce);

}  // namespace QtRocket::Gui::SpinBoxEditing
