#include "QtRocket/gui/widgets/StyledLabel.h"

#include <algorithm>
#include <cmath>

#include <QColor>
#include <QFont>
#include <QLabel>
#include <QPalette>
#include <QString>
#include <QWidget>
#include <Qt>

namespace QtRocket::Gui
{

StyledLabel::StyledLabel(QWidget* parent) : QLabel(parent) { }

StyledLabel::StyledLabel(const QString& text, QWidget* parent) : QLabel(text, parent) { }

StyledLabel::StyledLabel(const QString& text, double size, QWidget* parent) : QLabel(text, parent)
{
    resizeFont(size);
}

StyledLabel::StyledLabel(const QString& text, Style style, QWidget* parent)
  : StyledLabel(text, 0.0, style, parent)
{
}

StyledLabel::StyledLabel(const QString& text, double size, Style style, QWidget* parent)
  : QLabel(text, parent)
{
    resizeFont(size);
    setFontStyle(style);
}

StyledLabel::StyledLabel(const QString& text, Qt::Alignment alignment, double size, Style style,
                         QWidget* parent)
  : StyledLabel(text, size, style, parent)
{
    setAlignment(alignment);
}

void StyledLabel::resizeFont(double size)
{
    if (size == 0.0)
    {
        // Java sets the same font again; leaving it alone keeps the label following its
        // parent's font.
        return;
    }
    QFont resized = font();
    if (resized.pointSizeF() > 0.0)
    {
        resized.setPointSizeF(std::max(1.0, resized.pointSizeF() + size));
    }
    else
    {
        // A font sized in pixels: as many pixels as points.
        resized.setPixelSize(
            std::max(1, resized.pixelSize() + static_cast<int>(std::lround(size))));
    }
    setFont(resized);
}

void StyledLabel::setFontStyle(Style style)
{
    QFont styled = font();
    styled.setBold(style == Style::BOLD || style == Style::BOLD_ITALIC);
    styled.setItalic(style == Style::ITALIC || style == Style::BOLD_ITALIC);
    setFont(styled);
}

void StyledLabel::setFontColor(const QColor& color)
{
    QPalette colored = palette();
    colored.setColor(QPalette::WindowText, color);
    setPalette(colored);
}

}  // namespace QtRocket::Gui
