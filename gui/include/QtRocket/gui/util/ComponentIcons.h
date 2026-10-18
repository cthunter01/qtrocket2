#pragma once

#include <QIcon>
#include <QString>

#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/MassComponent.h"

namespace QtRocket
{
class RocketComponent;
}

namespace QtRocket::Gui
{

/// The icons of the component kinds (OpenRocket's ComponentIcons): a small one for the
/// component tree, a large one and a "disabled" large one for the add buttons, and a small one
/// per mass component type. They are OpenRocket's PNGs (:/icons/components/<name>-small.png and
/// -large.png); with a dark palette the kinds that have a "_dark" variant use it, as OpenRocket's
/// dark theme does (UITheme's getComponentIcon*()).
///
/// The disabled large icon is the large icon with every pixel whose (x + y) is even made
/// transparent (OpenRocket's "raster alpha").
///
/// Each icon is made once per file and handed out again: the same kind gives an icon with the
/// same cacheKey(), as OpenRocket returns the same ImageIcon. The cache is emptied when the
/// QCoreApplication goes, so no pixmap outlives it.
///
/// Deviations from OpenRocket: the light or dark file is chosen by the palette when the icon is
/// asked for, where OpenRocket reloads all of them on a theme change; the icons are not scaled by
/// a UI scale preference (Milestone 5) but by Qt's high-DPI scaling; the icons' descriptions
/// (accessible names) are not kept.
class ComponentIcons
{
public:
    ComponentIcons() = delete;

    /// The small icon of @p kind (getSmallIcon(Class)); a null icon for ROCKET, which has none.
    [[nodiscard]] static QIcon smallIcon(ComponentKind      kind,
                                         ThemeColors::Theme theme = ThemeColors::currentTheme());

    /// The small icon of @p component: a mass component's is the icon of its type
    /// (getSmallIcon(RocketComponent)), any other the icon of its kind.
    [[nodiscard]] static QIcon smallIcon(const RocketComponent& component,
                                         ThemeColors::Theme theme = ThemeColors::currentTheme());

    /// The small icon of the mass component type @p type (getSmallMassTypeIcon()).
    [[nodiscard]] static QIcon smallMassTypeIcon(
        MassComponent::MassComponentType type,
        ThemeColors::Theme               theme = ThemeColors::currentTheme());

    /// The large icon of @p kind (getLargeIcon()); a null icon for ROCKET.
    [[nodiscard]] static QIcon largeIcon(ComponentKind      kind,
                                         ThemeColors::Theme theme = ThemeColors::currentTheme());

    /// The disabled large icon of @p kind (getLargeDisabledIcon()); a null icon for ROCKET.
    [[nodiscard]] static QIcon largeDisabledIcon(
        ComponentKind kind, ThemeColors::Theme theme = ThemeColors::currentTheme());

    /// The file name stem of @p kind's icons in @p theme ("bodytube", "ellipticalfin_dark"),
    /// empty for ROCKET.
    [[nodiscard]] static QString iconName(ComponentKind kind, ThemeColors::Theme theme);

    /// The file name stem of @p type's icon in @p theme ("altimeter", "altimeter_dark").
    [[nodiscard]] static QString massTypeIconName(MassComponent::MassComponentType type,
                                                  ThemeColors::Theme               theme);

    /// The resource of the icon named @p name: ":/icons/components/<name>-small.png" or
    /// "-large.png".
    [[nodiscard]] static QString resourcePath(const QString& name, bool large);
};

}  // namespace QtRocket::Gui
