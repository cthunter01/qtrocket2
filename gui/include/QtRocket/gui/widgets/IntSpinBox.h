#pragma once

#include <memory>

#include <QObject>
#include <QSpinBox>

#include "QtRocket/util/Signal.h"

class QFocusEvent;
class QKeyEvent;
class QWheelEvent;
class QWidget;

namespace QtRocket::Gui
{

class IntBinding;

/// The spin box of a count field (OpenRocket: a JSpinner on IntegerModel.getSpinnerModel() with
/// its SpinnerEditor): the fin count, an instance count, a parachute's line count. A QSpinBox,
/// which takes plain integers only, as Java's Integer formatter does ("2*3" and "4.0" are no
/// count there either); it shows its IntBinding's value and commits through it, with the editing
/// rules of UnitSpinBox (SpinBoxEditing: Enter accepted, Esc reverts an edit or reaches the
/// dialog, the wheel only with the focus, everything selected on focus, Left and Right collapse a
/// selection).
///
/// A committed number goes to IntBinding::setSpinnerValue(), which clamps it to the binding's
/// bounds; the arrows and the Up and Down keys commit an edited text first, then step through
/// IntBinding::stepUp() and stepDown() (Java: the next value is the value plus one, none at the
/// maximum), so that a value beyond a bound (a file's fin count of 10, the bounds 1 to 8) is
/// shown as it is and steps back in. The spin box's range is the binding's bounds widened to the
/// value it shows. After every commit and step it shows the binding's value again, and whenever
/// the binding tells of a change; once the binding's selection is invalid it keeps the last value
/// and is disabled.
///
/// Enabled: the binding's isEnabled() and setFieldEnabled() together, as UnitSpinBox.
///
/// Deviations from OpenRocket: a QSpinBox refuses a keystroke that cannot make a number in its
/// range ("x", a number above the maximum), where Java's formatter takes any text and reverts it
/// or clamps it on the commit (the bindings scout's decision D13); and those of UnitSpinBox for
/// Enter, PageUp and PageDown, the wheel and the focus loss. As Qt's spin boxes, a focus loss
/// commits the typed number even while a popup menu takes the focus.
class IntSpinBox : public QSpinBox
{
    Q_OBJECT

public:
    /// A spin box of @p binding.
    /// @throws BugError when @p binding is null
    IntSpinBox(std::shared_ptr<IntBinding> binding, QWidget* parent);
    ~IntSpinBox() override;
    IntSpinBox(const IntSpinBox&)            = delete;
    IntSpinBox& operator=(const IntSpinBox&) = delete;
    IntSpinBox(IntSpinBox&&)                 = delete;
    IntSpinBox& operator=(IntSpinBox&&)      = delete;

    [[nodiscard]] const std::shared_ptr<IntBinding>& binding() const noexcept { return m_binding; }

    /// Commits the text when it has been edited since the binding's value was shown, then shows
    /// the binding's value again: true when there was an edit to commit.
    bool commitEdit();

    /// Whether the field may be edited as far as the dialog is concerned (see UnitSpinBox).
    [[nodiscard]] bool isFieldEnabled() const noexcept { return m_fieldEnabled; }
    void               setFieldEnabled(bool enabled);

    /// Commits an edited text, then steps @p steps times up (positive) or down from the
    /// binding's value, stopping at the bound.
    void stepBy(int steps) override;

protected:
    [[nodiscard]] StepEnabled stepEnabled() const override;
    void                      keyPressEvent(QKeyEvent* event) override;
    void                      focusInEvent(QFocusEvent* event) override;
    void                      focusOutEvent(QFocusEvent* event) override;
    void                      wheelEvent(QWheelEvent* event) override;

private:
    /// Shows the binding's state: its value in a range wide enough for it, enabled.
    void refresh();
    /// A number the spin box took (QSpinBox::valueChanged): to the binding.
    void valueTyped(int value);
    /// commitEdit() with what it throws reported (an event handler's call into the core).
    void guardedCommit();

    std::shared_ptr<IntBinding> m_binding;
    Signal<>::ScopedConnection  m_connection;
    bool                        m_fieldEnabled{true};
    /// Set while refresh() shows the binding's state: the spin box's own valueChanged then is
    /// no edit, and a focus loss it causes commits nothing.
    bool m_refreshing{false};
};

}  // namespace QtRocket::Gui
