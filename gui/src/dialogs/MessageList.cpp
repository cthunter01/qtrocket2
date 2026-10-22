#include "QtRocket/gui/dialogs/MessageList.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMouseEvent>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QtContainerFwd>

namespace QtRocket::Gui
{

MessageList::MessageList(const QStringList& messages, QWidget* parent)
  : MessageList(messages, QColor(), parent)
{
}

MessageList::MessageList(const QStringList& messages, const QColor& textColor, QWidget* parent)
  : QListWidget(parent)
{
    setAlternatingRowColors(true);
    // JList's default: any rows, with Ctrl and Shift.
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    addItems(messages);
    if (textColor.isValid())
    {
        for (int row = 0; row < count(); ++row)
        {
            item(row)->setForeground(QBrush(textColor));
        }
    }
}

QStringList MessageList::messages() const
{
    QStringList texts;
    for (int row = 0; row < count(); ++row)
    {
        texts.append(item(row)->text());
    }
    return texts;
}

void MessageList::mousePressEvent(QMouseEvent* event)
{
    // ErrorWarningDialog: "Deselect items if clicked on blank region".
    if (!indexAt(event->position().toPoint()).isValid())
    {
        clearSelection();
        event->accept();
        return;
    }
    QListWidget::mousePressEvent(event);
}

}  // namespace QtRocket::Gui
