#include "QtRocket/gui/util/ComponentIcons.h"

#include <array>
#include <cstddef>
#include <map>
#include <string_view>
#include <utility>

#include <QCoreApplication>
#include <QFile>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QString>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using Theme    = ThemeColors::Theme;
using MassType = MassComponent::MassComponentType;

/// The file name stems of an icon in OpenRocket's light theme and in its dark one (UITheme's
/// getComponentIcon*() of LIGHT and DARK).
struct Names
{
    std::string_view light;
    std::string_view dark;
};

constexpr Names names(std::string_view light, std::string_view dark)
{
    return {.light = light, .dark = dark};
}

/// By ComponentKind, in its order; ROCKET has no icon.
constexpr std::array<Names, kAllComponentKinds.size()> kKindNames{{
    names("", ""),                                 // ROCKET
    names("stage", "stage"),                       // AXIAL_STAGE
    names("boosters", "boosters"),                 // PARALLEL_STAGE
    names("pods", "pods_dark"),                    // POD_SET
    names("bodytube", "bodytube"),                 // BODY_TUBE
    names("transition", "transition"),             // TRANSITION
    names("nosecone", "nosecone"),                 // NOSE_CONE
    names("trapezoidfin", "trapezoidfin"),         // TRAPEZOID_FIN_SET
    names("ellipticalfin", "ellipticalfin_dark"),  // ELLIPTICAL_FIN_SET
    names("freeformfin", "freeformfin_dark"),      // FREEFORM_FIN_SET
    names("tubefin", "tubefin"),                   // TUBE_FIN_SET
    names("launchlug", "launchlug_dark"),          // LAUNCH_LUG
    names("railbutton", "railbutton"),             // RAIL_BUTTON
    names("innertube", "innertube"),               // INNER_TUBE
    names("tubecoupler", "tubecoupler"),           // TUBE_COUPLER
    names("engineblock", "engineblock"),           // ENGINE_BLOCK
    names("centeringring", "centeringring"),       // CENTERING_RING
    names("bulkhead", "bulkhead"),                 // BULKHEAD
    names("mass", "mass_dark"),                    // MASS_COMPONENT
    names("shockcord", "shockcord_dark"),          // SHOCK_CORD
    names("parachute", "parachute_dark"),          // PARACHUTE
    names("streamer", "streamer_dark"),            // STREAMER
}};

/// By MassComponentType, in its order.
constexpr std::array<Names, MassComponent::kAllMassComponentTypes.size()> kMassTypeNames{{
    names("mass", "mass_dark"),                            // MASSCOMPONENT
    names("altimeter", "altimeter_dark"),                  // ALTIMETER
    names("flight-comp", "flight-comp_dark"),              // FLIGHTCOMPUTER
    names("deployment-charge", "deployment-charge_dark"),  // DEPLOYMENTCHARGE
    names("tracker", "tracker_dark"),                      // TRACKER
    names("payload", "payload"),                           // PAYLOAD
    names("recovery-hardware", "recovery-hardware_dark"),  // RECOVERYHARDWARE
    names("battery", "battery_dark"),                      // BATTERY
}};

/// The kinds of icons made from one file.
enum class Variant
{
    SMALL,
    LARGE,
    LARGE_DISABLED,
};

/// The icons made so far, by file and variant.
using Cache = std::map<std::pair<QString, Variant>, QIcon>;

Cache& cache()
{
    static Cache s_cache;
    return s_cache;
}

void clearCache()
{
    cache().clear();
}

[[nodiscard]] QString nameOf(const Names& names, Theme theme)
{
    return QtStrings::fromUtf8(theme == Theme::LIGHT ? names.light : names.dark);
}

/// The large icon with every pixel whose (x + y) is even cleared (ComponentIcons.loadLarge).
[[nodiscard]] QIcon disabledIcon(const QString& path)
{
    QImage image(path);
    image.convertTo(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            if ((x + y) % 2 == 0)
            {
                image.setPixel(x, y, 0);
            }
        }
    }
    return {QPixmap::fromImage(image)};
}

/// The icon of the file @p name in @p variant, made once; a null icon for an empty name, and for
/// a missing file, which is reported as OpenRocket does ("Couldn't find file").
[[nodiscard]] QIcon iconOf(const QString& name, Variant variant)
{
    if (name.isEmpty())
    {
        return {};
    }
    Cache&     icons = cache();
    const auto key   = std::make_pair(name, variant);
    if (const auto found = icons.find(key); found != icons.end())
    {
        return found->second;
    }
    const QString path = ComponentIcons::resourcePath(name, variant != Variant::SMALL);
    if (!QFile::exists(path))
    {
        Guard::reportErrorCondition(QStringLiteral("ERROR:  Couldn't find file: ") + path);
        return {};
    }
    if (icons.empty())
    {
        // Pixmaps must not outlive the application: empty the cache when it goes.
        qAddPostRoutine(clearCache);
    }
    QIcon icon = variant == Variant::LARGE_DISABLED ? disabledIcon(path) : QIcon(path);
    icons.emplace(key, icon);
    return icon;
}

}  // namespace

QIcon ComponentIcons::smallIcon(ComponentKind kind, Theme theme)
{
    return iconOf(iconName(kind, theme), Variant::SMALL);
}

QIcon ComponentIcons::smallIcon(const RocketComponent& component, Theme theme)
{
    if (const auto* mass = dynamic_cast<const MassComponent*>(&component))
    {
        return smallMassTypeIcon(mass->getMassComponentType(), theme);
    }
    return smallIcon(component.kind(), theme);
}

QIcon ComponentIcons::smallMassTypeIcon(MassType type, Theme theme)
{
    return iconOf(massTypeIconName(type, theme), Variant::SMALL);
}

QIcon ComponentIcons::largeIcon(ComponentKind kind, Theme theme)
{
    return iconOf(iconName(kind, theme), Variant::LARGE);
}

QIcon ComponentIcons::largeDisabledIcon(ComponentKind kind, Theme theme)
{
    return iconOf(iconName(kind, theme), Variant::LARGE_DISABLED);
}

QString ComponentIcons::iconName(ComponentKind kind, Theme theme)
{
    const auto index = static_cast<std::size_t>(kind);
    QTROCKET_ASSERT(index < kKindNames.size());
    return nameOf(kKindNames.at(index), theme);
}

QString ComponentIcons::massTypeIconName(MassType type, Theme theme)
{
    const auto index = static_cast<std::size_t>(type);
    QTROCKET_ASSERT(index < kMassTypeNames.size());
    return nameOf(kMassTypeNames.at(index), theme);
}

QString ComponentIcons::resourcePath(const QString& name, bool large)
{
    return QStringLiteral(":/icons/components/") + name +
           (large ? QStringLiteral("-large.png") : QStringLiteral("-small.png"));
}

}  // namespace QtRocket::Gui
