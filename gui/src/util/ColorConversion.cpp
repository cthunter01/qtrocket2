#include "QtRocket/gui/util/ColorConversion.h"

#include <algorithm>
#include <optional>

#include <QChar>
#include <QColor>
#include <QLatin1Char>
#include <QString>
#include <QStringView>

#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"

namespace QtRocket::Gui::ColorConversion
{

namespace
{

constexpr int kMaxChannel = 255;

[[nodiscard]] bool isChannel(int value) noexcept
{
    return value >= 0 && value <= kMaxChannel;
}

/// Whether every character of @p text is a hexadecimal digit.
[[nodiscard]] bool isHex(QStringView text)
{
    return std::ranges::all_of(text, [](QChar character) {
        return (character >= QLatin1Char('0') && character <= QLatin1Char('9')) ||
               (character >= QLatin1Char('a') && character <= QLatin1Char('f')) ||
               (character >= QLatin1Char('A') && character <= QLatin1Char('F'));
    });
}

/// The value of the hexadecimal digits @p digits (all checked by isHex()).
[[nodiscard]] int hexValue(QStringView digits)
{
    bool      ok    = false;
    const int value = digits.toInt(&ok, 16);
    QTROCKET_ASSERT(ok);
    return value;
}

/// Two hex digits of @p value, in upper case.
[[nodiscard]] QString twoHexDigits(int value)
{
    return QStringLiteral("%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
}

}  // namespace

QColor toQColor(const Color& color)
{
    QTROCKET_ASSERT(isChannel(color.red()) && isChannel(color.green()) && isChannel(color.blue()) &&
                    isChannel(color.alpha()));
    return QColor::fromRgb(color.red(), color.green(), color.blue(), color.alpha());
}

std::optional<QColor> toQColor(const std::optional<Color>& color)
{
    if (!color)
    {
        return std::nullopt;
    }
    return toQColor(*color);
}

Color fromQColor(const QColor& color)
{
    QTROCKET_ASSERT(color.isValid());
    const QColor rgb = color.toRgb();
    return {rgb.red(), rgb.green(), rgb.blue(), rgb.alpha()};
}

QString formatHtmlColor(const QColor& color, const QString& content)
{
    return QStringLiteral("<font color=\"%1\">%2</font>").arg(color.name(QColor::HexRgb), content);
}

QString toHexColor(const Color& color)
{
    const int red   = color.red();
    const int green = color.green();
    const int blue  = color.blue();
    // Java: (c >> 4) == (c & 15) for each channel, then %01X of c & 15.
    constexpr int kNibble = 4;
    constexpr int kLow    = 15;
    if ((red >> kNibble) == (red & kLow) && (green >> kNibble) == (green & kLow) &&
        (blue >> kNibble) == (blue & kLow))
    {
        return QStringLiteral("#%1%2%3")
            .arg(red & kLow, 1, 16)
            .arg(green & kLow, 1, 16)
            .arg(blue & kLow, 1, 16)
            .toUpper();
    }
    return QStringLiteral("#") + twoHexDigits(red) + twoHexDigits(green) + twoHexDigits(blue);
}

QColor brightenColor(const QColor& color, int amount)
{
    const QColor rgb = color.toRgb();
    return QColor::fromRgb(std::min(kMaxChannel, rgb.red() + amount),
                           std::min(kMaxChannel, rgb.green() + amount),
                           std::min(kMaxChannel, rgb.blue() + amount));
}

std::optional<Color> fromHexColor(QStringView text)
{
    if (text.trimmed().isEmpty())
    {
        return std::nullopt;
    }
    QStringView hex = text;
    if (hex.startsWith(QLatin1Char('#')))
    {
        hex = hex.sliced(1);
    }
    hex = hex.trimmed();
    if (!isHex(hex))
    {
        return std::nullopt;
    }
    if (hex.size() == 3)
    {
        const int     red    = hexValue(hex.sliced(0, 1));
        const int     green  = hexValue(hex.sliced(1, 1));
        const int     blue   = hexValue(hex.sliced(2, 1));
        constexpr int kShift = 4;
        return Color((red << kShift) + red, (green << kShift) + green, (blue << kShift) + blue);
    }
    if (hex.size() == 6)
    {
        return Color(hexValue(hex.sliced(0, 2)), hexValue(hex.sliced(2, 2)),
                     hexValue(hex.sliced(4, 2)));
    }
    return std::nullopt;
}

}  // namespace QtRocket::Gui::ColorConversion
