#pragma once

#include <memory>
#include <optional>

#include <QAbstractSpinBox>
#include <QObject>
#include <QSize>
#include <QString>
#include <QValidator>

#include "QtRocket/util/Signal.h"

class QFocusEvent;
class QKeyEvent;
class QWheelEvent;
class QWidget;

namespace QtRocket::Gui
{

class DoubleBinding;

/// The spin box of a number field (OpenRocket: a JSpinner on DoubleModel.getSpinnerModel() with
/// its SpinnerEditor): it shows its DoubleBinding's text in the binding's current unit, reads
/// what is typed as an expression in that unit ("2*3", "1 3/4"; DoubleBinding::commitText()),
/// and steps by the unit's own step or a fixed one (setFixedStep(); Java's getSpinnerModel(step),
/// the shape parameters and the fins' fillet radius).
///
/// It is a QAbstractSpinBox of no value type of its own, so that every keystroke is accepted
/// (validate() is always Acceptable, as Java's String formatter) and every commit is the
/// binding's: an edited text is committed by Enter, by the focus leaving, by the arrows (before
/// they step) and by commitEdit(), which the dialog calls before OK, Close, a save or an undo
/// (macOS buttons take no focus, so a click there leaves the edit pending). The editing rules
/// are SpinBoxEditing's: Enter is accepted (a dialog's default button stays unpressed), Esc
/// reverts an edited text and otherwise reaches the dialog, the wheel steps only with the focus,
/// the text is selected when the focus comes in, Left and Right collapse a selection.
///
/// A commit of the very text the spin box shows (typed again after the select-all of the focus)
/// sets nothing, as in Java, where the spinner's text field holds the model's value as a String
/// and fires no value change for an equal one (JFormattedTextField.commitEdit() -> setValue() ->
/// firePropertyChange(), which drops an equal value), so that the setter is never called: an
/// automatic value stays automatic, the stored value keeps its precision, and in a multi-edit the
/// other components keep their own values.
///
/// After every commit and step, whatever came of it, the spin box shows the binding's text again
/// (a commit that changes nothing, or invalid text, fires nothing). It also shows it whenever the
/// binding tells of a change, also over an edit in progress (Java's editor takes the model's
/// value too); a commit cannot happen while it does so. A problem text (getProblem(): the
/// override CD without a force analysis) is shown read-only, without steps; once the binding's
/// selection is invalid the spin box is empty and disabled, and a pending edit can reach no
/// component (the binding drops it).
///
/// Enabled: the binding's isEnabled() and setFieldEnabled() together (a check box's enable
/// follower: Java's BooleanModel.addEnableComponent() sets the spinner's enabled state); call
/// setFieldEnabled(), not setEnabled(), which the next change of the binding would override.
///
/// The binding is shared with the field's unit selector and slider; the spin box holds it until
/// it goes or setBinding() replaces it (Java's InnerTubeConfig switches its separation field
/// between two models).
///
/// Deviations from OpenRocket:
/// - Enter keeps the focus and selects all (Java clears the focus owner); PageUp and PageDown step
///   ten times; the wheel steps a focused spin box (see SpinBoxEditing).
/// - A focus loss of any kind commits an edited text, as Qt's spin boxes do; Java leaves the edit
///   pending on a temporary one (another window, a popup).
/// - Invalid text and a NaN or infinite result set nothing (DoubleBinding).
class UnitSpinBox : public QAbstractSpinBox
{
    Q_OBJECT

public:
    /// A spin box of @p binding.
    /// @throws BugError when @p binding is null
    UnitSpinBox(std::shared_ptr<DoubleBinding> binding, QWidget* parent);
    ~UnitSpinBox() override;
    UnitSpinBox(const UnitSpinBox&)            = delete;
    UnitSpinBox& operator=(const UnitSpinBox&) = delete;
    UnitSpinBox(UnitSpinBox&&)                 = delete;
    UnitSpinBox& operator=(UnitSpinBox&&)      = delete;

    [[nodiscard]] const std::shared_ptr<DoubleBinding>& binding() const noexcept
    {
        return m_binding;
    }

    /// Shows and edits @p binding from now on; an edit in progress is dropped.
    /// @throws BugError when @p binding is null
    void setBinding(std::shared_ptr<DoubleBinding> binding);

    /// The fixed step in the current unit (Java's getSpinnerModel(stepSize)), or nullopt for the
    /// unit's own step.
    [[nodiscard]] std::optional<double> fixedStep() const noexcept { return m_fixedStep; }

    /// @throws BugError when @p step is not finite and positive (Java: IllegalArgumentException)
    void setFixedStep(std::optional<double> step);

    /// Commits the text when it has been edited since the binding's text was shown and differs
    /// from that text, then shows the binding's text again: true when the binding took a value
    /// (DoubleBinding::commitText()), false when nothing was edited, the text is the one shown
    /// (see the class comment), the text was invalid or the binding dropped it.
    bool commitEdit();

    /// Whether the field may be edited as far as the dialog is concerned (see the class comment).
    [[nodiscard]] bool isFieldEnabled() const noexcept { return m_fieldEnabled; }
    void               setFieldEnabled(bool enabled);

    /// Commits an edited text, then steps @p steps times up (positive) or down from the
    /// binding's value, stopping at the bound.
    void stepBy(int steps) override;

    /// Wide enough for a typical value ("-0000.000"): the base class would measure the texts of
    /// its value bounds, which a spin box without a value type has none of.
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

    /// Acceptable, whatever the text: the binding reads it when it is committed (Java's
    /// DefaultFormatter of a String value takes every keystroke).
    QValidator::State validate(QString& input, int& pos) const override;

protected:
    [[nodiscard]] StepEnabled stepEnabled() const override;
    void                      keyPressEvent(QKeyEvent* event) override;
    void                      focusInEvent(QFocusEvent* event) override;
    void                      focusOutEvent(QFocusEvent* event) override;
    void                      wheelEvent(QWheelEvent* event) override;

private:
    void connectBinding();
    /// Shows the binding's state: its text, enabled, read-only and the arrows.
    void refresh();
    /// commitEdit() with what it throws reported (an event handler's call into the core).
    void                guardedCommit();
    [[nodiscard]] QSize sizeFor(const QString& sample, const QSize& editSize) const;

    std::shared_ptr<DoubleBinding> m_binding;
    Signal<>::ScopedConnection     m_connection;
    std::optional<double>          m_fixedStep;
    bool                           m_fieldEnabled{true};
    /// The text refresh() showed last (Java: the value of the spinner's text field), which a
    /// commit of the same text leaves alone.
    QString m_shownText;
    /// Set while refresh() shows the binding's state: a focus loss it causes commits nothing.
    bool m_refreshing{false};
};

}  // namespace QtRocket::Gui
