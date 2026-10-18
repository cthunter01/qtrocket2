#pragma once

#include <QLabel>
#include <QObject>
#include <QSize>
#include <QString>
#include <Qt>

class QWidget;

namespace QtRocket::Gui
{

/// A label for rich text whose height is held at the height it wants for the text it was made
/// with (OpenRocket's HtmlLabel), so that an HTML text does not make its panel grow taller: the
/// minimum height and the maximum height are both that height, and its width may shrink to 0.
/// As in OpenRocket, the height is fixed at construction; a later setText() keeps it.
class HtmlLabel : public QLabel
{
    Q_OBJECT

public:
    explicit HtmlLabel(QWidget* parent);
    HtmlLabel(const QString& text, QWidget* parent);
    /// With the horizontal alignment @p alignment.
    HtmlLabel(const QString& text, Qt::Alignment alignment, QWidget* parent);

    /// Width 0, the height fixed at construction.
    [[nodiscard]] QSize minimumSizeHint() const override;

private:
    /// Holds the height at the size hint's (limitSize()).
    void limitSize();

    int m_height = 0;
};

}  // namespace QtRocket::Gui
