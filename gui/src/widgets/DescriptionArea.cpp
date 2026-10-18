#include "QtRocket/gui/widgets/DescriptionArea.h"

#include <algorithm>
#include <cmath>

#include <QColor>
#include <QDesktopServices>
#include <QFont>
#include <QPalette>
#include <QScrollBar>
#include <QSize>
#include <QString>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QUrl>
#include <QWidget>
#include <Qt>

namespace QtRocket::Gui
{

DescriptionArea::DescriptionArea(int rows, QWidget* parent)
  : DescriptionArea(QString(), rows, kDefaultSize, true, parent)
{
}

DescriptionArea::DescriptionArea(int rows, double size, QWidget* parent)
  : DescriptionArea(QString(), rows, size, true, parent)
{
}

DescriptionArea::DescriptionArea(const QString& text, int rows, QWidget* parent)
  : DescriptionArea(text, rows, kDefaultSize, true, parent)
{
}

DescriptionArea::DescriptionArea(const QString& text, int rows, double size, QWidget* parent)
  : DescriptionArea(text, rows, size, true, parent)
{
}

DescriptionArea::DescriptionArea(const QString& text, int rows, double size, bool opaque,
                                 QWidget* parent)
  : QTextBrowser(parent), m_size(size), m_rows(rows)
{
    setReadOnly(true);
    setOpenLinks(false);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    connect(this, &QTextBrowser::anchorClicked, this, &DescriptionArea::openLink);
    if (!opaque)
    {
        // Java: the background of a default JPanel.
        QPalette window = palette();
        window.setColor(QPalette::Base, window.color(QPalette::Window));
        setPalette(window);
    }
    setDescription(text);
}

void DescriptionArea::setDescription(const QString& text)
{
    m_text = text;
    document()->setDefaultFont(textFont());
    setHtml(text);
    scrollToTop();
}

void DescriptionArea::setTextFont(const QFont& font)
{
    setFont(font);
    setDescription(m_text);
    updateGeometry();
}

void DescriptionArea::setForegroundColor(const QColor& color)
{
    QPalette colored = palette();
    colored.setColor(QPalette::Text, color);
    setPalette(colored);
}

void DescriptionArea::setBackgroundColor(const QColor& color)
{
    QPalette colored = palette();
    colored.setColor(QPalette::Base, color);
    setPalette(colored);
}

void DescriptionArea::scrollToTop()
{
    moveCursor(QTextCursor::Start);
    verticalScrollBar()->setValue(verticalScrollBar()->minimum());
}

QSize DescriptionArea::sizeHint() const
{
    return {QTextBrowser::sizeHint().width(), heightOfRows(m_rows)};
}

int DescriptionArea::heightOfRows(int rows) const
{
    // Java measures its pane with "abc" and with "abc<br>def".
    QTextDocument measure;
    measure.setDefaultFont(textFont());
    measure.setDocumentMargin(document()->documentMargin());
    measure.setHtml(QStringLiteral("abc"));
    const double oneLine = measure.size().height();
    measure.setHtml(QStringLiteral("abc<br>def"));
    const double twoLines   = measure.size().height();
    const double lineHeight = twoLines - oneLine;
    const double extra      = oneLine - lineHeight;
    return static_cast<int>(std::lround((lineHeight * rows) + extra + 2.0)) + (2 * frameWidth());
}

QFont DescriptionArea::textFont() const
{
    QFont resized = font();
    if (resized.pointSizeF() > 0.0)
    {
        resized.setPointSizeF(std::max(1.0, resized.pointSizeF() + m_size));
    }
    else
    {
        resized.setPixelSize(
            std::max(1, resized.pixelSize() + static_cast<int>(std::lround(m_size))));
    }
    return resized;
}

void DescriptionArea::openLink(const QUrl& url)
{
    Q_EMIT linkActivated(url);
    if (m_opensLinks)
    {
        static_cast<void>(QDesktopServices::openUrl(url));
    }
}

}  // namespace QtRocket::Gui
