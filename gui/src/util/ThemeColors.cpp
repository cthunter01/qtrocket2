#include "QtRocket/gui/util/ThemeColors.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <utility>

#include <QColor>
#include <QGuiApplication>
#include <QPalette>
#include <QRgb>

#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using Key = ThemeColors::Key;

/// A key's name and its value in each theme (0xAARRGGBB; FlatLaf writes #RRGGBBAA).
struct Entry
{
    Key              key;
    std::string_view name;
    QRgb             light;
    QRgb             dark;
};

constexpr Entry colors(Key key, std::string_view name, QRgb light, QRgb dark)
{
    return {.key = key, .name = name, .light = light, .dark = dark};
}

// The values of themes/FlatLightLaf.properties and themes/FlatDarculaLaf.properties, with every
// reference ($OR.colors.blue, ...) resolved. VISIBILITY_HIDDEN_FOREGROUND is lighten(#000000, 25%)
// and darken(#bbbbbb, 25%) of FlatLaf's Tree.textForeground in each theme.
constexpr std::array<Entry, ThemeColors::kAllKeys.size()> kEntries{{
    colors(Key::BLUE, "OR.colors.blue", 0xff2d7ff9, 0xff6ea8fe),
    colors(Key::PURPLE, "OR.colors.purple", 0xff9f5cf5, 0xffc592ff),
    colors(Key::YELLOW, "OR.colors.yellow", 0xffa68002, 0xffffd64a),
    colors(Key::ORANGE, "OR.colors.orange", 0xffd99800, 0xffffb066),
    colors(Key::RED, "OR.colors.red", 0xffff0000, 0xfff68f8f),
    colors(Key::DARK_RED, "OR.colors.darkRed", 0xffc80000, 0xffe56767),
    colors(Key::GREEN, "OR.colors.green", 0xff20c968, 0xff66de93),
    colors(Key::BACKGROUND, "OR.colors.background", 0xffffffff, 0xff494c4f),
    colors(Key::BORDER, "OR.colors.border", 0xff000000, 0xff616365),
    colors(Key::TEXT, "OR.colors.text", 0xff000000, 0xffadadad),
    colors(Key::TEXT_DIM, "OR.colors.text.dim", 0xff808080, 0xffb6b6b6),
    colors(Key::TEXT_DISABLED, "OR.colors.text.disabled", 0xff808080, 0xffa1a1a1),
    colors(Key::INFO, "OR.colors.info", 0xff2d7ff9, 0xff6ea8fe),
    colors(Key::WARNING, "OR.colors.warning", 0xffd99800, 0xffffb066),
    colors(Key::ERROR, "OR.colors.error", 0xffff0000, 0xfff68f8f),
    colors(Key::DARK_ERROR, "OR.colors.darkError", 0xffc80000, 0xffe56767),
    colors(Key::MULTI_COMP_EDIT, "OR.colors.multiCompEdit", 0xffaa0064, 0xffde92b0),
    colors(Key::MOTOR_BORDER, "OR.colors.motor.border", 0xc8000000, 0x64000000),
    colors(Key::MOTOR_FILL, "OR.colors.motor.fill", 0x64000000, 0x32000000),
    colors(Key::CG, "OR.colors.cg", 0xff0000ff, 0xff5585fd),
    colors(Key::CP, "OR.colors.cp", 0xffff0000, 0xffff486a),
    colors(Key::URL, "OR.colors.url", 0xff0000ff, 0xff96a7ff),
    colors(Key::VISIBILITY_HIDDEN_FOREGROUND, "OR.colors.visibility.hidden.foreground", 0xff404040,
           0xff7b7b7b),
    colors(Key::FIN_GRID_MAJOR, "OR.colors.fin.gridMajor", 0x500000ff, 0xc58787c7),
    colors(Key::FIN_GRID_MINOR, "OR.colors.fin.gridMinor", 0x1e0000ff, 0x457979bd),
    colors(Key::FIN_POINT, "OR.colors.fin.point", 0xffc80000, 0xffd96c6c),
    colors(Key::FIN_SELECTED_POINT, "OR.colors.fin.selectedPoint", 0xffc80000, 0xffe84e4e),
    colors(Key::FIN_BODY_LINE, "OR.colors.fin.bodyLine", 0xff000000, 0xffffffff),
    colors(Key::FIN_SNAP_HIGHLIGHT, "OR.colors.fin.snapHighlight", 0xffff0000, 0xffff3a3a),
    colors(Key::RULER, "OR.colors.ruler", 0xff735a09, 0xffffd64a),
    colors(Key::ICONS_DEFAULT, "OR.icons.default", 0xff000000, 0xffffffff),
    colors(Key::ICONS_TINT_COLOR, "OR.icons.tintColor", 0xff2d7ff9, 0xff6ea8fe),
    colors(Key::ICONS_HELP, "OR.icons.help", 0xff2d7ff9, 0xff6ea8fe),
    colors(Key::ICONS_OVERRIDE, "OR.icons.override", 0xff2d7ff9, 0xff6ea8fe),
    colors(Key::ICONS_OVERRIDE_SUBCOMPONENT, "OR.icons.override.subcomponent", 0xff808080,
           0xffb6b6b6),
    colors(Key::ICONS_LOCKED, "OR.icons.locked", 0xfff1c066, 0xfff1c066),
    colors(Key::ICONS_DELETE, "OR.icons.delete", 0xffc80000, 0xfff68f8f),
    colors(Key::ICONS_PLUS, "OR.icons.plus", 0xff20c968, 0xff66de93),
    colors(Key::ICONS_DUPLICATE, "OR.icons.duplicate", 0xff2d7ff9, 0xff6ea8fe),
    colors(Key::ICONS_PENCIL, "OR.icons.pencil", 0xffa68002, 0xffffd64a),
    colors(Key::ICONS_ZOOM_IN, "OR.icons.zoomIn", 0xff20c968, 0xff66de93),
    colors(Key::ICONS_ZOOM_OUT, "OR.icons.zoomOut", 0xffc80000, 0xffe56767),
    colors(Key::ICONS_ZOOM_FIT, "OR.icons.zoomFit", 0xff2d7ff9, 0xff6ea8fe),
    colors(Key::ICONS_IMPORT, "OR.icons.import", 0xffc80000, 0xffe56767),
    colors(Key::ICONS_MOVE_UP, "OR.icons.moveUp", 0xff2d7ff9, 0xff6ea8fe),
    colors(Key::ICONS_MOVE_DOWN, "OR.icons.moveDown", 0xff9f5cf5, 0xffc592ff),
    colors(Key::ICONS_UNDO, "OR.icons.undo", 0xffc80000, 0xffe56767),
    colors(Key::ICONS_REDO, "OR.icons.redo", 0xff2d7ff9, 0xff6ea8fe),
    colors(Key::ICONS_MAGNET_RED, "OR.icons.magnet.red", 0xfffc5e42, 0xffe86b55),
    colors(Key::ICONS_MAGNET_WHITE, "OR.icons.magnet.white", 0xffffffff, 0xffe1e3e2),
}};

// OR.defaults.component.* of each theme, keyed by the class names of SwingPreferences'
// DEFAULT_COLORS (Preferences::defaultComponentColors()).
constexpr std::array<std::pair<std::string_view, std::string_view>, 10> kLightComponentColors{{
    {"BodyComponent", "0,0,240"},
    {"TubeFinSet", "0,0,200"},
    {"FinSet", "0,0,200"},
    {"LaunchLug", "0,0,180"},
    {"RailButton", "0,0,180"},
    {"InternalComponent", "170,0,100"},
    {"MassObject", "0,0,0"},
    {"RecoveryDevice", "255,0,0"},
    {"PodSet", "160,160,215"},
    {"ParallelStage", "198,163,184"},
}};

constexpr std::array<std::pair<std::string_view, std::string_view>, 10> kDarkComponentColors{{
    {"BodyComponent", "150,162,255"},
    {"TubeFinSet", "150,178,255"},
    {"FinSet", "150,178,255"},
    {"LaunchLug", "142,153,238"},
    {"RailButton", "142,153,238"},
    {"InternalComponent", "181,128,151"},
    {"MassObject", "210,210,210"},
    {"RecoveryDevice", "220,90,90"},
    {"PodSet", "190,190,235"},
    {"ParallelStage", "210,180,195"},
}};

/// The entry of @p key (the table is in the order of the keys).
const Entry& entry(Key key)
{
    const auto index = static_cast<std::size_t>(key);
    QTROCKET_ASSERT(index < kEntries.size() && kEntries.at(index).key == key);
    return kEntries.at(index);
}

/// FlatLaf's lighten(color, amount) for a positive @p amount and darken() for a negative one:
/// the HSL lightness plus @p amount, kept within 0 to 1.
QColor shiftLightness(const QColor& color, float amount)
{
    const QColor hsl       = color.toHsl();
    const float  lightness = std::clamp(hsl.lightnessF() + amount, 0.0F, 1.0F);
    return QColor::fromHslF(hsl.hslHueF(), hsl.hslSaturationF(), lightness, hsl.alphaF());
}

}  // namespace

ThemeColors::Theme ThemeColors::theme(const QPalette& palette)
{
    const QColor text   = palette.color(QPalette::Active, QPalette::WindowText);
    const QColor window = palette.color(QPalette::Active, QPalette::Window);
    return text.lightnessF() > window.lightnessF() ? Theme::DARK : Theme::LIGHT;
}

ThemeColors::Theme ThemeColors::currentTheme()
{
    return theme(QGuiApplication::palette());
}

QColor ThemeColors::color(Key key, const QPalette& palette)
{
    constexpr float kHiddenTextShift = 0.25F;
    switch (key)
    {
        case Key::ICONS_DEFAULT:
            return palette.color(QPalette::Active, QPalette::WindowText);
        case Key::VISIBILITY_HIDDEN_FOREGROUND:
            return shiftLightness(
                palette.color(QPalette::Active, QPalette::Text),
                theme(palette) == Theme::LIGHT ? kHiddenTextShift : -kHiddenTextShift);
        default:
            return themeColor(key, theme(palette));
    }
}

QColor ThemeColors::color(Key key)
{
    return color(key, QGuiApplication::palette());
}

QColor ThemeColors::themeColor(Key key, Theme theme)
{
    const Entry& value = entry(key);
    return QColor::fromRgba(theme == Theme::LIGHT ? value.light : value.dark);
}

std::string_view ThemeColors::keyName(Key key)
{
    return entry(key).name;
}

ComponentDefaults ThemeColors::defaultComponentColors(Theme theme) noexcept
{
    if (theme == Theme::LIGHT)
    {
        return kLightComponentColors;
    }
    return kDarkComponentColors;
}

}  // namespace QtRocket::Gui
