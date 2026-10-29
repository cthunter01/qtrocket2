#include "QtRocket/gui/dialogs/DetailDialog.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include <QChar>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QLayoutItem>
#include <QList>
#include <QPlainTextEdit>
#include <QRect>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include "QtRocket/gui/dialogs/ModalDialog.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// JOptionPane's icon of a message type.
[[nodiscard]] QStyle::StandardPixmap severityPixmap(UserPrompts::Severity severity)
{
    switch (severity)
    {
        case UserPrompts::Severity::INFORMATION:
            return QStyle::SP_MessageBoxInformation;
        case UserPrompts::Severity::WARNING:
            return QStyle::SP_MessageBoxWarning;
        case UserPrompts::Severity::ERROR:
            return QStyle::SP_MessageBoxCritical;
    }
    QTROCKET_UNREACHABLE();
}

/// Swing's rule (BasicHTML.isHTMLString()): a label's text is HTML when it starts with "<html>",
/// in any case.
[[nodiscard]] Qt::TextFormat lineFormat(const QString& line)
{
    return line.startsWith(QStringLiteral("<html>"), Qt::CaseInsensitive) ? Qt::RichText
                                                                          : Qt::PlainText;
}

/// @p font, kDetailFontDecrease points smaller (or as many pixels, for a font sized in pixels),
/// but at least 1.
[[nodiscard]] QFont smallerFont(QFont font)
{
    if (font.pointSizeF() > 0)
    {
        font.setPointSizeF(std::max(1.0, font.pointSizeF() - DetailDialog::kDetailFontDecrease));
    }
    else if (font.pixelSize() > 0)
    {
        font.setPixelSize(
            std::max(1, font.pixelSize() - static_cast<int>(DetailDialog::kDetailFontDecrease)));
    }
    return font;
}

}  // namespace

DetailDialog::DetailDialog(const QStringList& message, const std::optional<QString>& details,
                           const QString& title, UserPrompts::Severity severity, QWidget* parent)
  : QDialog(parent),
    m_layout(makeChild<QGridLayout>(this)),
    m_message(makeChild<QWidget>(this)),
    m_icon(makeChild<QLabel>(this))
{
    setWindowTitle(title);

    const int   size = style()->pixelMetric(QStyle::PM_MessageBoxIconSize, nullptr, this);
    const QIcon icon = style()->standardIcon(severityPixmap(severity), nullptr, this);
    m_icon->setPixmap(icon.pixmap(QSize(size, size), devicePixelRatio()));
    m_icon->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_layout->addWidget(m_icon, 0, 0);

    auto* columnLayout = makeChild<QVBoxLayout>(m_message);
    columnLayout->setContentsMargins(0, 0, 0, 0);
    for (const QString& line : message)
    {
        auto* label = makeChild<QLabel>(m_message);
        label->setTextFormat(lineFormat(line));
        label->setText(line);
        columnLayout->addWidget(label);
        m_messageLabels.append(label);
    }
    if (details.has_value())
    {
        m_details = makeChild<QPlainTextEdit>(m_message);
        m_details->setObjectName(QStringLiteral("details"));
        m_details->setReadOnly(true);
        m_details->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                           Qt::TextSelectableByKeyboard);
        m_details->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_details->setFont(smallerFont(m_details->font()));
        m_details->setPlainText(*details);
        m_details->moveCursor(QTextCursor::Start);
        // JTextArea(5, 40): rows of the line height, columns of the width of 'm'.
        const QFontMetrics metrics(m_details->font());
        const int margins = (2 * m_details->frameWidth()) +
                            (2 * static_cast<int>(m_details->document()->documentMargin()));
        m_details->setMinimumSize(
            (kDetailColumns * metrics.horizontalAdvance(QChar(u'm'))) + margins,
            (kDetailRows * metrics.lineSpacing()) + margins);
        columnLayout->addWidget(m_details);
    }
    m_layout->addWidget(m_message, 0, 1);

    m_buttons = makeChild<QDialogButtonBox>(this, QDialogButtonBox::Ok);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    m_layout->addWidget(m_buttons, 1, 0, 1, 2);
}

DetailDialog::~DetailDialog() = default;

void DetailDialog::wrapMessage(int width)
{
    for (QLabel* const label : std::as_const(m_messageLabels))
    {
        label->setWordWrap(true);
        label->setFixedWidth(width);
    }
    if (m_messageArea == nullptr)
    {
        m_messageArea = makeChild<QScrollArea>(this);
        m_messageArea->setObjectName(QStringLiteral("messageArea"));
        m_messageArea->setFrameShape(QFrame::NoFrame);
        m_messageArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_messageArea->setWidgetResizable(true);
        // The layout gives up the column's item, and the area takes the column.
        const std::unique_ptr<QLayoutItem> columnItem(
            m_layout->replaceWidget(m_message, m_messageArea));
        m_messageArea->setWidget(m_message);
    }
    // As tall as the wrapped lines, up to the share of the screen, and as wide as they are, with
    // the scroll bar beside them when they scroll.
    const int columnWidth = std::max(width, m_message->minimumSizeHint().width());
    const int wanted      = m_message->hasHeightForWidth() ? m_message->heightForWidth(columnWidth)
                                                           : m_message->sizeHint().height();
    const QScreen* const screen = this->screen();
    const int            available =
        screen != nullptr
            ? static_cast<int>(screen->availableGeometry().height() * kMaximumScreenShare)
            : wanted;
    const int scrollBar =
        wanted > available ? m_messageArea->verticalScrollBar()->sizeHint().width() : 0;
    m_messageArea->setFixedSize(columnWidth + scrollBar, std::min(wanted, available));
}

void DetailDialog::showDetailedMessageDialog(QWidget* parent, const QStringList& message,
                                             const std::optional<QString>& details,
                                             const QString& title, UserPrompts::Severity severity)
{
    // Made for the message, so that it survives its parent being destroyed while it is open.
    ModalDialog<DetailDialog> dialog(parent, message, details, title, severity);
    dialog.exec();
}

QStringList DetailDialog::messageLines() const
{
    QStringList lines;
    for (const QLabel* label : m_messageLabels)
    {
        lines.append(label->text());
    }
    return lines;
}

}  // namespace QtRocket::Gui
