#include "QtRocket/gui/widgets/IconButton.h"

#include <algorithm>
#include <cmath>

#include <QAbstractButton>
#include <QAction>
#include <QEvent>
#include <QFontMetrics>
#include <QIcon>
#include <QObject>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStyle>
#include <QStyleOptionButton>
#include <QStyleOptionFocusRect>
#include <QStylePainter>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/util/MenuText.h"

namespace QtRocket::Gui
{

IconButton::IconButton(QWidget* parent) : QPushButton(parent)
{
    scaleIcon();
    connect(this, &QAbstractButton::clicked, this, [this] {
        if (!m_action.isNull())
        {
            m_action->trigger();
        }
    });
}

void IconButton::setAction(QAction* action)
{
    if (action == m_action)
    {
        return;
    }
    disconnect(m_actionChanged);
    m_action = action;
    if (action != nullptr)
    {
        m_actionChanged = connect(action, &QAction::changed, this, [this] { showAction(); });
        showAction();
    }
}

void IconButton::showAction()
{
    if (m_action.isNull())
    {
        return;
    }
    // iconText() is the text without its mnemonic marks (a doubled '&' becomes one), which the
    // button shows as it is (escaped: no mnemonic of the button's own either).
    setText(MenuText::escaped(m_action->iconText()));
    setIcon(m_action->icon());
    setToolTip(m_action->toolTip());
    setEnabled(m_action->isEnabled());
}

void IconButton::scaleIcon()
{
    const int extent = static_cast<int>(
        std::lround(style()->pixelMetric(QStyle::PM_ButtonIconSize, nullptr, this) * kIconScale));
    setIconSize(QSize(extent, extent));
}

QSize IconButton::sizeHint() const
{
    // QPushButton::sizeHint() with kIconGap where it puts 4 pixels between the icon and the text.
    ensurePolished();
    QStyleOptionButton option;
    initStyleOption(&option);
    int width  = 0;
    int height = 0;
    if (!icon().isNull())
    {
        width  = option.iconSize.width();
        height = option.iconSize.height();
    }
    if (!text().isEmpty())
    {
        const QSize textSize = fontMetrics().size(Qt::TextShowMnemonic, text());
        width += textSize.width() + (icon().isNull() ? 0 : kIconGap);
        height = std::max(height, textSize.height());
    }
    if (width == 0)
    {
        // An empty button is as large as QPushButton's ("XXXX").
        const QSize empty = fontMetrics().size(Qt::TextShowMnemonic, QStringLiteral("XXXX"));
        width             = empty.width();
        height            = empty.height();
    }
    option.rect.setSize(QSize(width, height));
    return style()->sizeFromContents(QStyle::CT_PushButton, &option, QSize(width, height), this);
}

void IconButton::paintEvent(QPaintEvent* /*event*/)
{
    QStylePainter      painter(this);
    QStyleOptionButton option;
    initStyleOption(&option);
    // QCommonStyle's CE_PushButton: the bevel, the label within the contents, the focus frame;
    // the label is drawn here, from the left.
    painter.drawControl(QStyle::CE_PushButtonBevel, option);

    QRect contents = style()->subElementRect(QStyle::SE_PushButtonContents, &option, this);
    if (option.state.testAnyFlags(QStyle::State_On | QStyle::State_Sunken))
    {
        contents.translate(style()->pixelMetric(QStyle::PM_ButtonShiftHorizontal, &option, this),
                           style()->pixelMetric(QStyle::PM_ButtonShiftVertical, &option, this));
    }
    QRect textRect = contents;
    if (!option.icon.isNull())
    {
        QIcon::Mode mode = QIcon::Normal;
        if (!isEnabled())
        {
            mode = QIcon::Disabled;
        }
        else if (hasFocus())
        {
            mode = QIcon::Active;
        }
        const QIcon::State state = isChecked() ? QIcon::On : QIcon::Off;
        const QPixmap pixmap = option.icon.pixmap(option.iconSize, devicePixelRatio(), mode, state);
        const QRect iconRect(contents.left(),
                             contents.top() + ((contents.height() - option.iconSize.height()) / 2),
                             option.iconSize.width(), option.iconSize.height());
        style()->drawItemPixmap(&painter, QStyle::visualRect(layoutDirection(), contents, iconRect),
                                Qt::AlignCenter, pixmap);
        textRect.setLeft(iconRect.right() + 1 + kIconGap);
    }
    if (!option.text.isEmpty())
    {
        const bool mnemonics = style()->styleHint(QStyle::SH_UnderlineShortcut, &option, this) != 0;
        const Qt::Alignment alignment =
            QStyle::visualAlignment(layoutDirection(), Qt::AlignLeft | Qt::AlignVCenter);
        const int flags = static_cast<int>(alignment.toInt()) |
                          (mnemonics ? Qt::TextShowMnemonic : Qt::TextHideMnemonic);
        style()->drawItemText(&painter, QStyle::visualRect(layoutDirection(), contents, textRect),
                              flags, option.palette, isEnabled(), option.text,
                              QPalette::ButtonText);
    }

    if (option.state.testFlag(QStyle::State_HasFocus))
    {
        QStyleOptionFocusRect focus;
        focus.initFrom(this);
        focus.rect = style()->subElementRect(QStyle::SE_PushButtonFocusRect, &option, this);
        painter.drawPrimitive(QStyle::PE_FrameFocusRect, focus);
    }
}

void IconButton::changeEvent(QEvent* event)
{
    QPushButton::changeEvent(event);
    if (event->type() == QEvent::StyleChange)
    {
        scaleIcon();
    }
}

}  // namespace QtRocket::Gui
