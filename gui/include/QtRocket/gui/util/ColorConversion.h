#pragma once

#include <optional>

#include <QColor>
#include <QString>
#include <QStringView>

#include "QtRocket/util/Color.h"

/// The core's colour (util/Color, OpenRocket's ORColor) and Qt's QColor (OpenRocket's
/// ColorConversion between ORColor and java.awt.Color), and the colour texts OpenRocket writes.
/// compositeColor() is Milestone 5's (the calipers' only user), with its test.
namespace QtRocket::Gui::ColorConversion
{

/// @p color as a QColor (toAwtColor()). A channel outside 0-255, which java.awt.Color rejects
/// with an IllegalArgumentException, is a BugError (the core's colours from files are checked).
[[nodiscard]] QColor toQColor(const Color& color);

/// toQColor() of a colour that may be unset (OpenRocket's null): nullopt stays nullopt.
[[nodiscard]] std::optional<QColor> toQColor(const std::optional<Color>& color);

/// @p color as the core's colour, 8 bits per channel (fromAwtColor()). An invalid QColor (none
/// of Java's colours is one) is a BugError.
[[nodiscard]] Color fromQColor(const QColor& color);

/// "<font color=\"#rrggbb\">@p content</font>" (formatHTMLColor(): lower-case hex digits,
/// without the alpha).
[[nodiscard]] QString formatHtmlColor(const QColor& color, const QString& content);

/// The colour as "#RGB" when each channel's two hex digits are equal, else "#RRGGBB", upper case
/// and without the alpha (toHexColor()).
[[nodiscard]] QString toHexColor(const Color& color);

/// The opaque colour with @p amount added to each channel, at most 255 (brightenColor(); the
/// alpha is dropped, as Java's new Color(r, g, b) drops it).
[[nodiscard]] QColor brightenColor(const QColor& color, int amount);

/// The opaque colour of "#RGB", "RGB", "#RRGGBB" or "RRGGBB" (fromHexColor(): the '#' is
/// removed first, then the rest trimmed). nullopt for a blank text (Java returns null) and for
/// any other text (Java throws an IllegalArgumentException, which its only caller treats as the
/// null).
[[nodiscard]] std::optional<Color> fromHexColor(QStringView text);

}  // namespace QtRocket::Gui::ColorConversion
