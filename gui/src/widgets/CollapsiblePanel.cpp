#include "QtRocket/gui/widgets/CollapsiblePanel.h"

#include <algorithm>

#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QSizePolicy>
#include <QString>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The header font's size below the label font's, pt (HEADER_FONT_SIZE_REDUCTION).
constexpr double kHeaderFontSizeReduction = 1.0;
/// The smallest header font, pt (Java: Math.max(9.0f, ...)).
constexpr double kMinimumHeaderFontSize = 9.0;
/// The width of the frame and of the line above the content, px.
constexpr int kLineWidth = 1;
/// The content's empty border, px (Java: createEmptyBorder(4, 8, 6, 8)).
constexpr int kContentTop    = 4;
constexpr int kContentSide   = 8;
constexpr int kContentBottom = 6;

/// @p content, which must not be null.
[[nodiscard]] QWidget* checkedContent(QWidget* content)
{
    if (content == nullptr)
    {
        bug("A collapsible panel needs a content");
    }
    return content;
}

}  // namespace

CollapsiblePanel::CollapsiblePanel(const QString& title, QWidget* content, bool expanded,
                                   QWidget* parent)
  : QWidget(parent),
    m_header(makeChild<QToolButton>(this)),
    m_container(makeChild<QWidget>(this)),
    m_content(checkedContent(content))
{
    m_header->setObjectName(QStringLiteral("headerButton"));
    m_header->setText(title);
    m_header->setAccessibleName(title);
    m_header->setCheckable(true);
    m_header->setAutoRaise(true);
    m_header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // Java: setExpanded(headerButton.isSelected()) on every action.
    connect(m_header, &QToolButton::clicked, this, [this](bool checked) { setExpanded(checked); });

    auto* layout = makeChild<QVBoxLayout>(this);
    layout->setContentsMargins(kLineWidth, kLineWidth, kLineWidth, kLineWidth);
    layout->setSpacing(0);
    layout->addWidget(m_header);
    layout->addWidget(m_container);

    auto* containerLayout = makeChild<QVBoxLayout>(m_container);
    containerLayout->setContentsMargins(kContentSide, kLineWidth + kContentTop, kContentSide,
                                        kContentBottom);
    containerLayout->addWidget(m_content);

    updateAppearance();
    setExpanded(expanded);
}

void CollapsiblePanel::setExpanded(bool expanded)
{
    m_expanded = expanded;
    m_header->setChecked(expanded);
    m_header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    m_container->setVisible(expanded);
    update();
}

void CollapsiblePanel::updateAppearance()
{
    QFont font = QApplication::font("QLabel");
    font.setBold(true);
    if (font.pointSizeF() > 0)
    {
        font.setPointSizeF(
            std::max(kMinimumHeaderFontSize, font.pointSizeF() - kHeaderFontSizeReduction));
    }
    m_header->setFont(font);
}

void CollapsiblePanel::changeEvent(QEvent* event)
{
    // Java's updateUI(): a new look and feel brings its fonts.
    if (event->type() == QEvent::StyleChange)
    {
        updateAppearance();
    }
    QWidget::changeEvent(event);
}

void CollapsiblePanel::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setPen(palette().color(QPalette::Mid));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0, 0, -kLineWidth, -kLineWidth));
    if (m_expanded)
    {
        // The content's matte border: a line along its top.
        const int y = m_container->y();
        painter.drawLine(kLineWidth, y, width() - (2 * kLineWidth), y);
    }
}

}  // namespace QtRocket::Gui
