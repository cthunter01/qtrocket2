#include "QtRocket/gui/widgets/SelectionKeyFilter.h"

#include <QAbstractSpinBox>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QObject>
#include <QPlainTextEdit>
#include <QString>
#include <QTextCursor>
#include <QTextEdit>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// Where the cursor goes before the key @p event acts on a selection from @p start to @p end:
/// one character after the start for Left, one before the end for Right (Java:
/// setCaretPosition(start + 1) and setCaretPosition(end - 1)); -1 when the rule does not apply
/// (Shift held, another key, no selection).
[[nodiscard]] int preparedPosition(const QKeyEvent& event, int start, int end)
{
    if (event.modifiers().testFlag(Qt::ShiftModifier) || end <= start)
    {
        return -1;
    }
    switch (event.key())
    {
        case Qt::Key_Left:
            return start + 1;
        case Qt::Key_Right:
            return end - 1;
        default:
            return -1;
    }
}

/// The rule on a multi-line editor (QPlainTextEdit, QTextEdit).
template <class TextEditor>
void prepareTextKey(TextEditor& edit, const QKeyEvent& event)
{
    QTextCursor cursor = edit.textCursor();
    if (!cursor.hasSelection())
    {
        return;
    }
    const int position = preparedPosition(event, cursor.selectionStart(), cursor.selectionEnd());
    if (position >= 0)
    {
        cursor.setPosition(position);
        edit.setTextCursor(cursor);
    }
}

/// Whether the filter can act on @p widget.
[[nodiscard]] bool isSupported(const QWidget& widget)
{
    return qobject_cast<const QLineEdit*>(&widget) != nullptr ||
           qobject_cast<const QPlainTextEdit*>(&widget) != nullptr ||
           qobject_cast<const QTextEdit*>(&widget) != nullptr ||
           qobject_cast<const QAbstractSpinBox*>(&widget) != nullptr;
}

}  // namespace

SelectionKeyFilter::SelectionKeyFilter(QWidget* parent) : QObject(parent) { }

void SelectionKeyFilter::install(QWidget& widget)
{
    if (!isSupported(widget))
    {
        bug("SelectionKeyFilter: a widget that is no text field, text area or spin box");
    }
    widget.installEventFilter(makeChild<SelectionKeyFilter>(&widget));
}

void SelectionKeyFilter::prepareKey(QLineEdit& edit, const QKeyEvent& event)
{
    if (!edit.hasSelectedText())
    {
        return;
    }
    const int position = preparedPosition(event, edit.selectionStart(), edit.selectionEnd());
    if (position >= 0)
    {
        edit.setCursorPosition(position);  // also clears the selection
    }
}

void SelectionKeyFilter::prepareKey(QPlainTextEdit& edit, const QKeyEvent& event)
{
    prepareTextKey(edit, event);
}

void SelectionKeyFilter::prepareKey(QTextEdit& edit, const QKeyEvent& event)
{
    prepareTextKey(edit, event);
}

bool SelectionKeyFilter::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::KeyPress)
    {
        return QObject::eventFilter(watched, event);
    }
    const auto* const key = dynamic_cast<const QKeyEvent*>(event);
    if (key == nullptr)
    {
        return QObject::eventFilter(watched, event);
    }
    if (auto* const line = qobject_cast<QLineEdit*>(watched))
    {
        prepareKey(*line, *key);
    }
    else if (auto* const spinBox = qobject_cast<QAbstractSpinBox*>(watched))
    {
        // The spin box hands its keys to its line edit, its direct child.
        if (auto* const edit =
                spinBox->findChild<QLineEdit*>(QString(), Qt::FindDirectChildrenOnly))
        {
            prepareKey(*edit, *key);
        }
    }
    else if (auto* const plain = qobject_cast<QPlainTextEdit*>(watched))
    {
        prepareKey(*plain, *key);
    }
    else if (auto* const rich = qobject_cast<QTextEdit*>(watched))
    {
        prepareKey(*rich, *key);
    }
    return QObject::eventFilter(watched, event);  // the key goes on
}

}  // namespace QtRocket::Gui
