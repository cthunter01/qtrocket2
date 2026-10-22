#include "QtRocket/gui/widgets/UnitSpinBox.h"

#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <QAbstractSpinBox>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPointer>
#include <QScopedValueRollback>
#include <QSize>
#include <QString>
#include <QStyle>
#include <QStyleOptionSpinBox>
#include <QValidator>
#include <QWheelEvent>
#include <QWidget>

#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/SpinBoxEditing.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

namespace
{

/// @p binding, which must not be null.
[[nodiscard]] std::shared_ptr<DoubleBinding> checkedBinding(std::shared_ptr<DoubleBinding> binding)
{
    if (binding == nullptr)
    {
        bug("A unit spin box needs a binding");
    }
    return binding;
}

/// Java's check of getSpinnerModel(stepSize).
void checkStep(std::optional<double> step)
{
    const bool usable = !step.has_value() || (std::isfinite(*step) && *step > 0);
    if (!usable)
    {
        bug("A spinner step size must be finite and positive");
    }
}

}  // namespace

UnitSpinBox::UnitSpinBox(std::shared_ptr<DoubleBinding> binding, QWidget* parent)
  : QAbstractSpinBox(parent), m_binding(checkedBinding(std::move(binding)))
{
    SpinBoxEditing::setUp(*this);
    connectBinding();
    refresh();
}

UnitSpinBox::~UnitSpinBox() = default;

void UnitSpinBox::connectBinding()
{
    m_connection =
        m_binding->addChangeListener([this] { Guard::run([this] { refresh(); }, objectName()); });
}

void UnitSpinBox::setBinding(std::shared_ptr<DoubleBinding> binding)
{
    std::shared_ptr<DoubleBinding> checked = checkedBinding(std::move(binding));
    m_connection.disconnect();
    m_binding = std::move(checked);
    connectBinding();
    refresh();
}

void UnitSpinBox::setFixedStep(std::optional<double> step)
{
    checkStep(step);
    m_fixedStep = step;
    update();  // the arrows
}

void UnitSpinBox::setFieldEnabled(bool enabled)
{
    m_fieldEnabled = enabled;
    // Only the enabled state: an edit in progress stays, as Java's setEnabled() leaves the text.
    setEnabled(m_fieldEnabled && m_binding->isEnabled());
}

void UnitSpinBox::refresh()
{
    const QScopedValueRollback<bool> refreshing(m_refreshing, true);
    const DoubleBinding&             binding = *m_binding;
    // The text first: setText() clears isModified(), so the focus loss that disabling a focused
    // spin box causes finds nothing to commit.
    m_shownText = QtStrings::fromUtf8(binding.text());
    lineEdit()->setText(m_shownText);
    setReadOnly(binding.getProblem().has_value() || !binding.isWritable());
    setEnabled(m_fieldEnabled && binding.isEnabled());
    update();  // the arrows follow stepEnabled()
}

bool UnitSpinBox::commitEdit()
{
    if (m_refreshing || !lineEdit()->isModified())
    {
        return false;
    }
    if (lineEdit()->text() == m_shownText)
    {
        // The text shown, typed again: Java's text field fires no value change for it, so no
        // setter runs (see the class comment).
        refresh();
        return false;
    }
    const std::string text = QtStrings::toUtf8(lineEdit()->text());
    // The binding's listeners may replace or drop it, or delete a dialog with deleteLater().
    const std::shared_ptr<DoubleBinding> binding = m_binding;
    const QPointer<UnitSpinBox>          self(this);
    const bool                           taken = binding->commitText(text);
    if (!self.isNull())
    {
        refresh();  // whatever came of it: invalid text and an unchanged value fire nothing
    }
    return taken;
}

void UnitSpinBox::guardedCommit()
{
    const QPointer<UnitSpinBox> self(this);
    if (!Guard::run([this] { static_cast<void>(commitEdit()); }, objectName()) && !self.isNull())
    {
        refresh();  // the binding's state again after what the commit threw
    }
}

void UnitSpinBox::stepBy(int steps)
{
    // Called from Qt's event handlers (keys, arrows, wheel, auto-repeat): report what the core
    // throws.
    const QPointer<UnitSpinBox> self(this);
    Guard::run(
        [this, &self, steps] {
            static_cast<void>(commitEdit());
            const std::shared_ptr<DoubleBinding> binding = m_binding;
            const std::optional<double>          step    = m_fixedStep;
            SpinBoxEditing::repeatSteps(self, steps, [&binding, step](bool up) {
                return up ? binding->stepUp(step) : binding->stepDown(step);
            });
        },
        objectName());
    if (self.isNull())
    {
        return;
    }
    refresh();
    if (style()->styleHint(QStyle::SH_SpinBox_SelectOnStep, nullptr, this) != 0)
    {
        selectAll();
    }
}

QValidator::State UnitSpinBox::validate(QString& /*input*/, int& /*pos*/) const
{
    // Every keystroke is accepted (Java's DefaultFormatter of a String value); the binding reads
    // the text when it is committed.
    return QValidator::Acceptable;
}

QAbstractSpinBox::StepEnabled UnitSpinBox::stepEnabled() const
{
    if (isReadOnly())
    {
        return StepNone;
    }
    if (lineEdit()->isModified())
    {
        // The arrows commit the edit first, which may move the value off a bound.
        return StepUpEnabled | StepDownEnabled;
    }
    StepEnabled enabled = StepNone;
    if (m_binding->nextValue(m_fixedStep).has_value())
    {
        enabled |= StepUpEnabled;
    }
    if (m_binding->previousValue(m_fixedStep).has_value())
    {
        enabled |= StepDownEnabled;
    }
    return enabled;
}

void UnitSpinBox::keyPressEvent(QKeyEvent* event)
{
    switch (SpinBoxEditing::keyAction(*event, lineEdit()->isModified()))
    {
        case SpinBoxEditing::KeyAction::COMMIT:
        {
            event->accept();
            const QPointer<UnitSpinBox> self(this);
            guardedCommit();
            if (!self.isNull())
            {
                selectAll();
                Q_EMIT editingFinished();
            }
            return;
        }
        case SpinBoxEditing::KeyAction::REVERT:
            event->accept();
            refresh();
            return;
        case SpinBoxEditing::KeyAction::PASS_ON:
            event->ignore();
            return;
        case SpinBoxEditing::KeyAction::EDIT:
            break;
    }
    QAbstractSpinBox::keyPressEvent(event);
}

void UnitSpinBox::focusInEvent(QFocusEvent* event)
{
    QAbstractSpinBox::focusInEvent(event);
    SpinBoxEditing::selectAllOnFocus(*this, *event);
}

void UnitSpinBox::focusOutEvent(QFocusEvent* event)
{
    const QPointer<UnitSpinBox> self(this);
    guardedCommit();
    if (!self.isNull())
    {
        QAbstractSpinBox::focusOutEvent(event);
    }
}

void UnitSpinBox::wheelEvent(QWheelEvent* event)
{
    if (!SpinBoxEditing::takesWheel(*this))
    {
        event->ignore();  // on to the widgets around it: a scroll area scrolls
        return;
    }
    QAbstractSpinBox::wheelEvent(event);
}

QSize UnitSpinBox::sizeHint() const
{
    return sizeFor(QStringLiteral("-0000.000 "), lineEdit()->sizeHint());
}

QSize UnitSpinBox::minimumSizeHint() const
{
    return sizeFor(QStringLiteral("000 "), lineEdit()->minimumSizeHint());
}

QSize UnitSpinBox::sizeFor(const QString& sample, const QSize& editSize) const
{
    // As QAbstractSpinBox measures its value texts: the text, and 2 pixels for the cursor.
    ensurePolished();
    const QFontMetrics  metrics = fontMetrics();
    const QSize         contents(metrics.horizontalAdvance(sample) + 2, editSize.height());
    QStyleOptionSpinBox option;
    initStyleOption(&option);
    return style()->sizeFromContents(QStyle::CT_SpinBox, &option, contents, this);
}

}  // namespace QtRocket::Gui
