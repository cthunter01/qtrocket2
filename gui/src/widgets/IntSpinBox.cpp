#include "QtRocket/gui/widgets/IntSpinBox.h"

#include <algorithm>
#include <memory>
#include <utility>

#include <QAbstractSpinBox>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPointer>
#include <QScopedValueRollback>
#include <QSpinBox>
#include <QString>
#include <QStyle>
#include <QValidator>
#include <QWheelEvent>
#include <QWidget>

#include "QtRocket/gui/binding/IntBinding.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/widgets/SpinBoxEditing.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

namespace
{

/// @p binding, which must not be null.
[[nodiscard]] std::shared_ptr<IntBinding> checkedBinding(std::shared_ptr<IntBinding> binding)
{
    if (binding == nullptr)
    {
        bug("An integer spin box needs a binding");
    }
    return binding;
}

}  // namespace

IntSpinBox::IntSpinBox(std::shared_ptr<IntBinding> binding, QWidget* parent)
  : QSpinBox(parent), m_binding(checkedBinding(std::move(binding)))
{
    SpinBoxEditing::setUp(*this);
    // QSpinBox's own commits (interpretText(), hiding a spin box with an edit) reach the binding
    // here; the values refresh() sets are no commits.
    connect(this, &QSpinBox::valueChanged, this,
            [this](int value) { Guard::run([this, value] { valueTyped(value); }, objectName()); });
    m_connection =
        m_binding->addChangeListener([this] { Guard::run([this] { refresh(); }, objectName()); });
    refresh();
}

IntSpinBox::~IntSpinBox() = default;

void IntSpinBox::setFieldEnabled(bool enabled)
{
    m_fieldEnabled = enabled;
    setEnabled(m_fieldEnabled && m_binding->isEnabled());
}

void IntSpinBox::refresh()
{
    const QScopedValueRollback<bool> refreshing(m_refreshing, true);
    const IntBinding&                binding = *m_binding;
    const int                        value   = binding.value();
    // QSpinBox shows no value outside its range: a value beyond a bound widens it.
    setRange(std::min(binding.minimum(), value), std::max(binding.maximum(), value));
    setValue(value);  // also puts its text back when the value is the same
    // An edit that left the same text has not set the text again.
    lineEdit()->setModified(false);
    setEnabled(m_fieldEnabled && binding.isEnabled());
    update();  // the arrows follow stepEnabled()
}

void IntSpinBox::valueTyped(int value)
{
    if (m_refreshing)
    {
        return;
    }
    const std::shared_ptr<IntBinding> binding = m_binding;
    const QPointer<IntSpinBox>        self(this);
    static_cast<void>(binding->setSpinnerValue(value));
    if (!self.isNull())
    {
        refresh();
    }
}

bool IntSpinBox::commitEdit()
{
    if (m_refreshing || !lineEdit()->isModified())
    {
        return false;
    }
    QString text     = lineEdit()->text();
    int     position = lineEdit()->cursorPosition();
    // QSpinBox's own reading of its text; a text that is no number in the range is reverted.
    const bool                        number  = validate(text, position) == QValidator::Acceptable;
    const std::shared_ptr<IntBinding> binding = m_binding;
    const QPointer<IntSpinBox>        self(this);
    const bool taken = number && binding->setSpinnerValue(valueFromText(text));
    if (!self.isNull())
    {
        refresh();
    }
    return taken;
}

void IntSpinBox::guardedCommit()
{
    const QPointer<IntSpinBox> self(this);
    if (!Guard::run([this] { static_cast<void>(commitEdit()); }, objectName()) && !self.isNull())
    {
        refresh();  // the binding's state again after what the commit threw
    }
}

void IntSpinBox::stepBy(int steps)
{
    // Called from Qt's event handlers (keys, arrows, wheel, auto-repeat): report what the core
    // throws.
    const QPointer<IntSpinBox> self(this);
    Guard::run(
        [this, &self, steps] {
            static_cast<void>(commitEdit());
            const std::shared_ptr<IntBinding> binding = m_binding;
            SpinBoxEditing::repeatSteps(self, steps, [&binding](bool up) {
                return up ? binding->stepUp() : binding->stepDown();
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

QAbstractSpinBox::StepEnabled IntSpinBox::stepEnabled() const
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
    if (m_binding->nextValue().has_value())
    {
        enabled |= StepUpEnabled;
    }
    if (m_binding->previousValue().has_value())
    {
        enabled |= StepDownEnabled;
    }
    return enabled;
}

void IntSpinBox::keyPressEvent(QKeyEvent* event)
{
    switch (SpinBoxEditing::keyAction(*event, lineEdit()->isModified()))
    {
        case SpinBoxEditing::KeyAction::COMMIT:
        {
            event->accept();
            const QPointer<IntSpinBox> self(this);
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
    QSpinBox::keyPressEvent(event);
}

void IntSpinBox::focusInEvent(QFocusEvent* event)
{
    QSpinBox::focusInEvent(event);
    SpinBoxEditing::selectAllOnFocus(*this, *event);
}

void IntSpinBox::focusOutEvent(QFocusEvent* event)
{
    const QPointer<IntSpinBox> self(this);
    guardedCommit();
    if (!self.isNull())
    {
        QSpinBox::focusOutEvent(event);
    }
}

void IntSpinBox::wheelEvent(QWheelEvent* event)
{
    if (!SpinBoxEditing::takesWheel(*this))
    {
        event->ignore();  // on to the widgets around it: a scroll area scrolls
        return;
    }
    QSpinBox::wheelEvent(event);
}

}  // namespace QtRocket::Gui
