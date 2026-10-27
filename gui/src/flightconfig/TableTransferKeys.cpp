#include "QtRocket/gui/flightconfig/TableTransferKeys.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include <QClipboard>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLatin1Char>
#include <QMimeData>
#include <QObject>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// Whether @p event is one of the keys of Cut, Copy and Paste.
[[nodiscard]] bool isTransferKey(const QKeyEvent& event)
{
    return event.matches(QKeySequence::Cut) || event.matches(QKeySequence::Copy) ||
           event.matches(QKeySequence::Paste);
}

}  // namespace

TableTransferKeys::TableTransferKeys(CellSource cells, QWidget* table)
  : QObject(table), m_cells(std::move(cells))
{
    QTROCKET_ASSERT(table != nullptr);
    table->installEventFilter(this);
}

bool TableTransferKeys::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != parent() ||
        (event->type() != QEvent::ShortcutOverride && event->type() != QEvent::KeyPress))
    {
        return QObject::eventFilter(watched, event);
    }
    const auto* key = dynamic_cast<const QKeyEvent*>(event);
    if (key == nullptr || !isTransferKey(*key))
    {
        return QObject::eventFilter(watched, event);
    }
    if (event->type() == QEvent::ShortcutOverride)
    {
        // The table takes the key: the window's shortcut of it does not act.
        event->accept();
        return true;
    }
    if (key->matches(QKeySequence::Copy))
    {
        Guard::run([this] { copy(); }, QStringLiteral("Copy table cells"));
    }
    return true;
}

QString TableTransferKeys::plainText(const Cells& cells)
{
    QString text;
    for (std::size_t row = 0; row < cells.size(); ++row)
    {
        if (row > 0)
        {
            text += QLatin1Char('\n');
        }
        const std::vector<QString>& values = cells[row];
        for (std::size_t column = 0; column < values.size(); ++column)
        {
            if (column > 0)
            {
                text += QLatin1Char('\t');
            }
            text += values[column];
        }
    }
    return text;
}

QString TableTransferKeys::html(const Cells& cells)
{
    QString text = QStringLiteral("<html>\n<body>\n<table>\n");
    for (const std::vector<QString>& values : cells)
    {
        text += QStringLiteral("<tr>\n");
        for (const QString& value : values)
        {
            text += QStringLiteral("  <td>") + value + QStringLiteral("</td>\n");
        }
        text += QStringLiteral("</tr>\n");
    }
    return text + QStringLiteral("</table>\n</body>\n</html>");
}

void TableTransferKeys::copy() const
{
    const Cells cells = m_cells ? m_cells() : Cells{};
    if (cells.empty())
    {
        return;
    }
    auto data = std::make_unique<QMimeData>();
    data->setText(plainText(cells));
    data->setHtml(html(cells));
    // The clipboard takes the data over.
    QGuiApplication::clipboard()->setMimeData(data.release());
}

}  // namespace QtRocket::Gui
