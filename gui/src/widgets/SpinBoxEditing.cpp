#include "QtRocket/gui/widgets/SpinBoxEditing.h"

#include <functional>

#include <QAbstractSpinBox>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QPointer>
#include <QTimer>
#include <Qt>

#include "QtRocket/gui/widgets/SelectionKeyFilter.h"

namespace QtRocket::Gui::SpinBoxEditing
{

void setUp(QAbstractSpinBox& spinBox)
{
    // Not Qt::WheelFocus (QAbstractSpinBox's): the wheel must not take the focus.
    spinBox.setFocusPolicy(Qt::StrongFocus);
    spinBox.setKeyboardTracking(false);
    spinBox.setAccelerated(false);
    SelectionKeyFilter::install(spinBox);
}

KeyAction keyAction(const QKeyEvent& event, bool edited) noexcept
{
    switch (event.key())
    {
        case Qt::Key_Enter:
        case Qt::Key_Return:
            return KeyAction::COMMIT;
        case Qt::Key_Escape:
            return edited ? KeyAction::REVERT : KeyAction::PASS_ON;
        default:
            return KeyAction::EDIT;
    }
}

bool takesWheel(const QAbstractSpinBox& spinBox)
{
    return spinBox.hasFocus();
}

void selectAllOnFocus(QAbstractSpinBox& spinBox, const QFocusEvent& event)
{
    if (event.reason() == Qt::PopupFocusReason)
    {
        return;
    }
    // The spin box is the timer's context: a spin box deleted meanwhile is not called.
    QTimer::singleShot(0, &spinBox, [&spinBox] { spinBox.selectAll(); });
}

void repeatSteps(const QPointer<QAbstractSpinBox>& spinBox, int steps,
                 const std::function<bool(bool up)>& stepOnce)
{
    const bool up = steps > 0;
    // In a long long: -steps of the smallest int would overflow.
    const long long count = up ? static_cast<long long>(steps) : -static_cast<long long>(steps);
    for (long long left = count; left > 0 && !spinBox.isNull(); --left)
    {
        if (!stepOnce(up))
        {
            break;
        }
    }
}

}  // namespace QtRocket::Gui::SpinBoxEditing
