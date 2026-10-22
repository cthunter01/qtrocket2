#include "QtRocket/gui/dialogs/OptionDialog.h"

#include <algorithm>

#include <QAbstractButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QShowEvent>
#include <QSize>
#include <QSizePolicy>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// Whether JOptionPane shows an icon for the message type @p type.
[[nodiscard]] bool hasIcon(OptionDialog::MessageType type) noexcept
{
    return type != OptionDialog::MessageType::PLAIN;
}

/// The icon of the message type @p type, which has one.
[[nodiscard]] QStyle::StandardPixmap iconPixmap(OptionDialog::MessageType type)
{
    switch (type)
    {
        case OptionDialog::MessageType::INFORMATION:
            return QStyle::SP_MessageBoxInformation;
        case OptionDialog::MessageType::WARNING:
            return QStyle::SP_MessageBoxWarning;
        case OptionDialog::MessageType::ERROR:
            return QStyle::SP_MessageBoxCritical;
        case OptionDialog::MessageType::QUESTION:
            return QStyle::SP_MessageBoxQuestion;
        case OptionDialog::MessageType::PLAIN:
            break;
    }
    QTROCKET_UNREACHABLE();
}

}  // namespace

OptionDialog::MessageType OptionDialog::messageType(UserPrompts::Severity severity)
{
    switch (severity)
    {
        case UserPrompts::Severity::INFORMATION:
            return MessageType::INFORMATION;
        case UserPrompts::Severity::WARNING:
            return MessageType::WARNING;
        case UserPrompts::Severity::ERROR:
            return MessageType::ERROR;
    }
    QTROCKET_UNREACHABLE();
}

bool OptionDialog::isHtml(const QString& line)
{
    return line.startsWith(QStringLiteral("<html>"), Qt::CaseInsensitive);
}

OptionDialog::OptionDialog(MessageType type, const QString& title, QWidget* parent)
  : QDialog(parent),
    m_icon(makeChild<QLabel>(this)),
    m_message(makeChild<QWidget>(this)),
    m_messageLayout(makeChild<QVBoxLayout>(m_message)),
    m_buttons(makeChild<QDialogButtonBox>(this))
{
    setWindowTitle(title);
    auto* layout = makeChild<QGridLayout>(this);

    m_icon->setObjectName(QStringLiteral("icon"));
    if (hasIcon(type))
    {
        const int   size = style()->pixelMetric(QStyle::PM_MessageBoxIconSize, nullptr, this);
        const QIcon icon = style()->standardIcon(iconPixmap(type), nullptr, this);
        m_icon->setPixmap(icon.pixmap(QSize(size, size), devicePixelRatio()));
    }
    else
    {
        m_icon->hide();
    }
    m_icon->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    layout->addWidget(m_icon, 0, 0);

    m_message->setObjectName(QStringLiteral("message"));
    m_messageLayout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_message, 0, 1);

    m_buttons->setObjectName(QStringLiteral("options"));
    connect(m_buttons, &QDialogButtonBox::clicked, this,
            [this](QAbstractButton* option) { choose(option); });
    layout->addWidget(m_buttons, 1, 0, 1, 2);
}

OptionDialog::~OptionDialog() = default;

QLabel* OptionDialog::addMessageLine(const QString& line)
{
    auto* label = makeChild<QLabel>(m_message);
    label->setTextFormat(isHtml(line) ? Qt::RichText : Qt::PlainText);
    label->setText(line);
    m_messageLayout->addWidget(label);
    m_messageLabels.append(label);
    return label;
}

void OptionDialog::addMessageLines(const QStringList& lines)
{
    for (const QString& line : lines)
    {
        static_cast<void>(addMessageLine(line));
    }
}

void OptionDialog::addMessageWidget(QWidget* widget)
{
    QTROCKET_ASSERT(widget != nullptr);
    m_messageLayout->addWidget(widget);
}

void OptionDialog::addMessageGap()
{
    m_messageLayout->addSpacing(gapHeight());
}

int OptionDialog::gapHeight() const
{
    int spacing = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing, nullptr, this);
    if (spacing < 0)
    {
        // A style that spaces by the kinds of the widgets.
        spacing = style()->layoutSpacing(QSizePolicy::DefaultType, QSizePolicy::DefaultType,
                                         Qt::Vertical, nullptr, this);
    }
    return std::max(spacing, 0);
}

QPushButton* OptionDialog::addOption(const QString& text, QDialogButtonBox::ButtonRole role)
{
    return m_buttons->addButton(text, role);
}

QPushButton* OptionDialog::addOption(QDialogButtonBox::StandardButton button)
{
    return m_buttons->addButton(button);
}

void OptionDialog::setInitialOption(QPushButton* option)
{
    QTROCKET_ASSERT(option != nullptr && m_buttons->buttons().contains(option));
    m_initial = option;
    option->setDefault(true);
}

QStringList OptionDialog::messageLines() const
{
    QStringList lines;
    for (const QLabel* label : m_messageLabels)
    {
        lines.append(label->text());
    }
    return lines;
}

void OptionDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (m_initial != nullptr)
    {
        m_initial->setFocus(Qt::OtherFocusReason);
    }
}

void OptionDialog::choose(QAbstractButton* option)
{
    m_chosen = option;
    accept();
}

}  // namespace QtRocket::Gui
