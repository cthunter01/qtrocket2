#include "QtRocket/gui/widgets/ColorChooserButton.h"

#include <algorithm>
#include <optional>

#include <QColor>
#include <QIcon>
#include <QIconEngine>
#include <QPainter>
#include <QPen>
#include <QPointer>
#include <QPushButton>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/UserPrompts.h"

namespace QtRocket::Gui
{

namespace
{

/// Java's ColorIcon: the colour, filled; only its outline while disabled.
class ColorIconEngine final : public QIconEngine
{
public:
    explicit ColorIconEngine(const QColor& color) : m_color(color) { }

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode,
               QIcon::State /*state*/) override
    {
        if (mode != QIcon::Disabled)
        {
            painter->fillRect(rect, m_color);
            return;
        }
        // Java: drawRect(x, y, 25, 15), 1 px wide; here inside the icon's rectangle (Java's
        // outline is 1 px wider and taller than the icon), as wide as 1 px of the icon's size.
        const double width = std::max(
            1.0, rect.height() / static_cast<double>(ColorChooserButton::kIconSize.height()));
        QPen pen(m_color, width);
        pen.setJoinStyle(Qt::MiterJoin);
        painter->save();
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(QRectF(rect).adjusted(width / 2, width / 2, -width / 2, -width / 2));
        painter->restore();
    }

    [[nodiscard]] QIconEngine* clone() const override
    {
        return makeQtOwned<ColorIconEngine>(m_color);
    }

    [[nodiscard]] QString key() const override { return QStringLiteral("QtRocketColorIcon"); }

private:
    QColor m_color;
};

}  // namespace

ColorChooserButton::ColorChooserButton(const QColor& initial, UserPrompts& prompts, QWidget* parent)
  : QPushButton(parent), m_prompts(prompts)
{
    setIconSize(kIconSize);
    setSelectedColor(initial);
    connect(this, &QPushButton::clicked, this,
            [this] { Guard::run([this] { chooseColor(); }, objectName()); });
}

ColorChooserButton::~ColorChooserButton() = default;

QIcon ColorChooserButton::colorIcon(const QColor& color)
{
    return QIcon(makeQtOwned<ColorIconEngine>(color));
}

void ColorChooserButton::setSelectedColor(const QColor& color)
{
    // Java: Color.equals(), the RGB values and the alpha.
    if (!color.isValid() || (m_color.isValid() && color.rgba() == m_color.rgba()))
    {
        return;
    }
    m_color = color;
    setIcon(colorIcon(color));
    Q_EMIT selectedColorChanged(color);
}

void ColorChooserButton::chooseColor()
{
    //: Not translated in OpenRocket (ColorChooserButton.java): the title of the colour dialog
    const QString                      title = tr("Select color");
    const QPointer<ColorChooserButton> self(this);
    const std::optional<QColor>        chosen = m_prompts.askColor(this, m_color, title);
    if (!self.isNull() && chosen.has_value())
    {
        setSelectedColor(*chosen);
    }
}

}  // namespace QtRocket::Gui
