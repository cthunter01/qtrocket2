#pragma once

#include <array>
#include <string_view>

#include <QColor>

#include "QtRocket/preferences/Preferences.h"

class QPalette;

namespace QtRocket::Gui
{

/// OpenRocket's own colours (the OR.* keys of its UITheme) that the GUI paints with: the figure,
/// the carets, the fin editor, the tree's muted text, the icons' colours. OpenRocket takes them
/// from its FlatLaf theme (themes/FlatLightLaf.properties, FlatDarculaLaf.properties); QtRocket
/// follows the platform's style and its light or dark palette instead (the theme selector is
/// Milestone 5), so it takes the light theme's values on a light palette and the dark theme's on
/// a dark one, as theme() decides.
///
/// Deviations from OpenRocket, where its value refers to FlatLaf's own colours, which QtRocket
/// does not have: ICONS_DEFAULT (OR.icons.default: OR.colors.text in the light theme, white in
/// the dark one) is the palette's window text, so that the icons look like the text around them
/// in any style; VISIBILITY_HIDDEN_FOREGROUND is the palette's text lightened by 25 % (light) or
/// darkened by 25 % (dark) in HSL lightness, as FlatLaf's lighten()/darken() of
/// Tree.textForeground.
class ThemeColors
{
public:
    /// Which of OpenRocket's two themes' colours.
    enum class Theme
    {
        /// FlatLightLaf.properties (OpenRocket's LIGHT).
        LIGHT,
        /// FlatDarculaLaf.properties (OpenRocket's DARK).
        DARK,
    };

    /// The colours, by OpenRocket's key (keyName()).
    enum class Key
    {
        BLUE,
        PURPLE,
        YELLOW,
        ORANGE,
        RED,
        DARK_RED,
        GREEN,
        BACKGROUND,
        BORDER,
        TEXT,
        TEXT_DIM,
        TEXT_DISABLED,
        INFO,
        WARNING,
        ERROR,
        DARK_ERROR,
        MULTI_COMP_EDIT,
        MOTOR_BORDER,
        MOTOR_FILL,
        CG,
        CP,
        URL,
        VISIBILITY_HIDDEN_FOREGROUND,
        FIN_GRID_MAJOR,
        FIN_GRID_MINOR,
        FIN_POINT,
        FIN_SELECTED_POINT,
        FIN_BODY_LINE,
        FIN_SNAP_HIGHLIGHT,
        RULER,
        ICONS_DEFAULT,
        ICONS_TINT_COLOR,
        ICONS_HELP,
        ICONS_OVERRIDE,
        ICONS_OVERRIDE_SUBCOMPONENT,
        ICONS_LOCKED,
        ICONS_DELETE,
        ICONS_PLUS,
        ICONS_DUPLICATE,
        ICONS_PENCIL,
        ICONS_ZOOM_IN,
        ICONS_ZOOM_OUT,
        ICONS_ZOOM_FIT,
        ICONS_IMPORT,
        ICONS_MOVE_UP,
        ICONS_MOVE_DOWN,
        ICONS_UNDO,
        ICONS_REDO,
        ICONS_MAGNET_RED,
        ICONS_MAGNET_WHITE,
    };

    /// Every key, in declaration order.
    static constexpr std::array kAllKeys{
        Key::BLUE,
        Key::PURPLE,
        Key::YELLOW,
        Key::ORANGE,
        Key::RED,
        Key::DARK_RED,
        Key::GREEN,
        Key::BACKGROUND,
        Key::BORDER,
        Key::TEXT,
        Key::TEXT_DIM,
        Key::TEXT_DISABLED,
        Key::INFO,
        Key::WARNING,
        Key::ERROR,
        Key::DARK_ERROR,
        Key::MULTI_COMP_EDIT,
        Key::MOTOR_BORDER,
        Key::MOTOR_FILL,
        Key::CG,
        Key::CP,
        Key::URL,
        Key::VISIBILITY_HIDDEN_FOREGROUND,
        Key::FIN_GRID_MAJOR,
        Key::FIN_GRID_MINOR,
        Key::FIN_POINT,
        Key::FIN_SELECTED_POINT,
        Key::FIN_BODY_LINE,
        Key::FIN_SNAP_HIGHLIGHT,
        Key::RULER,
        Key::ICONS_DEFAULT,
        Key::ICONS_TINT_COLOR,
        Key::ICONS_HELP,
        Key::ICONS_OVERRIDE,
        Key::ICONS_OVERRIDE_SUBCOMPONENT,
        Key::ICONS_LOCKED,
        Key::ICONS_DELETE,
        Key::ICONS_PLUS,
        Key::ICONS_DUPLICATE,
        Key::ICONS_PENCIL,
        Key::ICONS_ZOOM_IN,
        Key::ICONS_ZOOM_OUT,
        Key::ICONS_ZOOM_FIT,
        Key::ICONS_IMPORT,
        Key::ICONS_MOVE_UP,
        Key::ICONS_MOVE_DOWN,
        Key::ICONS_UNDO,
        Key::ICONS_REDO,
        Key::ICONS_MAGNET_RED,
        Key::ICONS_MAGNET_WHITE,
    };

    ThemeColors() = delete;

    /// DARK when @p palette's window text is lighter than its window (in HSL lightness), as Qt
    /// tells a dark colour scheme from its palette; LIGHT otherwise.
    [[nodiscard]] static Theme theme(const QPalette& palette);

    /// theme() of the application's palette (QGuiApplication::palette()), which follows the
    /// operating system's light or dark scheme.
    [[nodiscard]] static Theme currentTheme();

    /// The colour of @p key for @p palette: the value of its theme(), or the palette's own for
    /// ICONS_DEFAULT and VISIBILITY_HIDDEN_FOREGROUND (see the class comment).
    [[nodiscard]] static QColor color(Key key, const QPalette& palette);

    /// color() for the application's palette, read now (call it when painting, not once).
    [[nodiscard]] static QColor color(Key key);

    /// The value of @p key in @p theme; for ICONS_DEFAULT and VISIBILITY_HIDDEN_FOREGROUND,
    /// which come from the palette, the value of OpenRocket's theme file (black/white, and the
    /// lightened/darkened text of FlatLaf's default text colours).
    [[nodiscard]] static QColor themeColor(Key key, Theme theme);

    /// OpenRocket's key of @p key, e.g. "OR.colors.cg", "OR.icons.tintColor".
    [[nodiscard]] static std::string_view keyName(Key key);

    /// The default colours of the components in @p theme (OR.defaults.component.*), as the table
    /// of Preferences::defaultComponentColors() ("BodyComponent" -> "0,0,240", ...), which the
    /// GUI's preference store returns for the current theme (SwingPreferences fills
    /// DEFAULT_COLORS from the theme).
    [[nodiscard]] static ComponentDefaults defaultComponentColors(Theme theme) noexcept;
};

}  // namespace QtRocket::Gui
