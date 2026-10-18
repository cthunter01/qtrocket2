#include "QtRocket/gui/widgets/HtmlLabel.h"

#include <QLabel>
#include <QSize>
#include <QString>
#include <QWidget>
#include <Qt>

namespace QtRocket::Gui
{

HtmlLabel::HtmlLabel(QWidget* parent) : QLabel(parent)
{
    limitSize();
}

HtmlLabel::HtmlLabel(const QString& text, QWidget* parent) : QLabel(text, parent)
{
    limitSize();
}

HtmlLabel::HtmlLabel(const QString& text, Qt::Alignment alignment, QWidget* parent)
  : QLabel(text, parent)
{
    setAlignment(alignment);
    limitSize();
}

QSize HtmlLabel::minimumSizeHint() const
{
    return {0, m_height};
}

void HtmlLabel::limitSize()
{
    // QLabel's, named: limitSize() runs in the constructors.
    m_height = QLabel::sizeHint().height();
    setMinimumHeight(m_height);
    setMaximumHeight(m_height);
}

}  // namespace QtRocket::Gui
