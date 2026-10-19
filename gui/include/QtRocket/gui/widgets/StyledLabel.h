#pragma once

#include <QLabel>
#include <QObject>
#include <QString>
#include <Qt>

class QColor;
class QWidget;

namespace QtRocket::Gui
{

/// A label whose font is the default font resized by a number of points and given a style
/// (OpenRocket's StyledLabel): new StyledLabel(text, -2) is OpenRocket's small text. Java's
/// constructors without a text are these with an empty one. The size is relative to the label's
/// font when the change is made.
///
/// Deviations from OpenRocket: the constructor with an icon (an icon scaled by the size) is
/// not ported, as nothing in the GUI uses it; nor is checkPreferredSize(), a Swing work-around
/// that has no effect.
class StyledLabel : public QLabel
{
    Q_OBJECT

public:
    /// The font style (Font.PLAIN, BOLD, ITALIC, BOLD | ITALIC).
    enum class Style
    {
        PLAIN,
        BOLD,
        ITALIC,
        BOLD_ITALIC,
    };

    explicit StyledLabel(QWidget* parent);
    StyledLabel(const QString& text, QWidget* parent);
    StyledLabel(const QString& text, double size, QWidget* parent);
    StyledLabel(const QString& text, Style style, QWidget* parent);
    StyledLabel(const QString& text, double size, Style style, QWidget* parent);
    /// With the horizontal alignment @p alignment (Java's horizontalAlignment; the default is
    /// leading).
    StyledLabel(const QString& text, Qt::Alignment alignment, double size, Style style,
                QWidget* parent);

    /// Changes the font size by @p size points (positive or negative), relative to the current
    /// size (resizeFont()).
    void resizeFont(double size);

    /// Sets the font's style (setFontStyle()): PLAIN clears bold and italic.
    void setFontStyle(Style style);

    /// The text colour (setFontColor()).
    void setFontColor(const QColor& color);
};

}  // namespace QtRocket::Gui
