#include "QtRocket/gui/widgets/LineStyleComboBox.h"

#include <cstddef>
#include <string_view>

#include <QColor>
#include <QComboBox>
#include <QGuiApplication>
#include <QIcon>
#include <QIconEngine>
#include <QLineF>
#include <QList>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QRect>
#include <QString>
#include <QWheelEvent>
#include <QWidget>
#include <Qt>
#include <QtTypes>

#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/LineStyle.h"

namespace QtRocket::Gui
{

namespace
{

/// The English text of @p style's key (messages.properties: LineStyle.Solid, .Dashed, .Dotted,
/// .Dash-dotted). The core's LineStyle has a display key but no displayName(), which
/// util/EnumText's enumText() needs; this gives the text enumText() would, with the same key and
/// translation context.
[[nodiscard]] std::string_view englishLineStyleName(LineStyle style)
{
    switch (style)
    {
        case LineStyle::SOLID:
            return "Solid";
        case LineStyle::DASHED:
            return "Dashed";
        case LineStyle::DOTTED:
            return "Dotted";
        case LineStyle::DASHDOT:
            return "Dash-dotted";
    }
    QTROCKET_UNREACHABLE();
}

/// The index of @p style in kAllLineStyles.
[[nodiscard]] int styleIndex(LineStyle style)
{
    for (std::size_t index = 0; index < kAllLineStyles.size(); ++index)
    {
        if (kAllLineStyles.at(index) == style)
        {
            return static_cast<int>(index);
        }
    }
    QTROCKET_UNREACHABLE();
}

/// Java's LineStyleIcon: the style's stroke across the middle of the icon, in the palette's text
/// colour of the icon's mode.
class StrokeIconEngine final : public QIconEngine
{
public:
    explicit StrokeIconEngine(LineStyle style) : m_style(style) { }

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode,
               QIcon::State /*state*/) override
    {
        const QPalette palette = QGuiApplication::palette();
        QColor         color;
        switch (mode)
        {
            case QIcon::Selected:
                color = palette.color(QPalette::Active, QPalette::HighlightedText);
                break;
            case QIcon::Disabled:
                color = palette.color(QPalette::Disabled, QPalette::Text);
                break;
            case QIcon::Normal:
            case QIcon::Active:
                color = palette.color(QPalette::Active, QPalette::Text);
                break;
        }
        // Java draws at 48 x 12; an icon drawn larger (a high-density screen) scales the stroke.
        const double scale =
            rect.width() / static_cast<double>(LineStyleComboBox::kIconSize.width());
        QPen pen = LineStyleComboBox::strokePen(m_style, color);
        pen.setWidthF(pen.widthF() * scale);
        // Java: drawLine(x, y + HEIGHT / 2, x + WIDTH, y + HEIGHT / 2).
        const double middle = rect.top() + (rect.height() / 2.0);
        painter->save();
        painter->setPen(pen);
        painter->drawLine(QLineF(rect.left(), middle, rect.left() + rect.width(), middle));
        painter->restore();
    }

    [[nodiscard]] QIconEngine* clone() const override
    {
        return makeQtOwned<StrokeIconEngine>(m_style);
    }

    [[nodiscard]] QString key() const override { return QStringLiteral("QtRocketLineStyleIcon"); }

private:
    LineStyle m_style;
};

}  // namespace

LineStyleComboBox::LineStyleComboBox(LineStyle initial, QWidget* parent) : QComboBox(parent)
{
    // Not Qt::WheelFocus (QComboBox's): the wheel must not take the focus.
    setFocusPolicy(Qt::StrongFocus);
    setIconSize(kIconSize);
    for (const LineStyle style : kAllLineStyles)
    {
        addItem(strokeIcon(style), text(style));
    }
    setCurrentIndex(styleIndex(initial));
    connect(this, &QComboBox::activated, this,
            [this] { Guard::run([this] { Q_EMIT lineStyleSelected(); }, objectName()); });
}

LineStyleComboBox::~LineStyleComboBox() = default;

LineStyle LineStyleComboBox::selectedLineStyle() const
{
    const int index = currentIndex();
    QTROCKET_ASSERT(index >= 0 && static_cast<std::size_t>(index) < kAllLineStyles.size());
    return kAllLineStyles.at(static_cast<std::size_t>(index));
}

void LineStyleComboBox::setSelectedLineStyle(LineStyle style)
{
    setCurrentIndex(styleIndex(style));
    Q_EMIT lineStyleSelected();
}

QString LineStyleComboBox::text(LineStyle style)
{
    return translatedEnumText(displayKey(style), englishLineStyleName(style));
}

QPen LineStyleComboBox::strokePen(LineStyle style, const QColor& color)
{
    QPen pen(color, kStrokeWidth, Qt::SolidLine, Qt::FlatCap, Qt::BevelJoin);
    if (style != LineStyle::SOLID)
    {
        QList<qreal> pattern;
        for (const double length : dashes(style))
        {
            pattern.append(length / kStrokeWidth);
        }
        pen.setDashPattern(pattern);
    }
    return pen;
}

QIcon LineStyleComboBox::strokeIcon(LineStyle style)
{
    return QIcon(makeQtOwned<StrokeIconEngine>(style));
}

void LineStyleComboBox::wheelEvent(QWheelEvent* event)
{
    if (!hasFocus())
    {
        event->ignore();  // on to the widgets around it: a scroll area scrolls
        return;
    }
    QComboBox::wheelEvent(event);
}

}  // namespace QtRocket::Gui
