#include "QtRocket/gui/util/Icons.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <string_view>
#include <vector>

#include <QFile>
#include <QFont>
#include <QFontInfo>
#include <QIcon>
#include <QRgb>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/SvgIconEngine.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using Id          = Icons::Id;
using Key         = ThemeColors::Key;
using Placeholder = SvgIconEngine::Placeholder;

/// Icons.java's SVG_THEME_COLOR_RGB: the black an SVG draws in.
constexpr QRgb kBlack = 0x000000;

/// Icons.java's ICON_FONT_SIZE_MULTIPLIER.
constexpr double kIconFontSizeMultiplier = 1.25;

/// Icons.java's scale multiplier of the subcomponent override markers.
constexpr double kSubcomponentScale = 0.75;

/// An icon of Icons.java: its name, its SVG and the placeholders of its colour map (at most
/// three).
struct Entry
{
    Id                         id;
    std::string_view           name;
    std::string_view           file;
    std::array<Placeholder, 3> colors;
    std::size_t                colorCount;
    double                     scale = 1.0;
};

/// loadIcon(svg, png, name) and loadSvgIcon(svg, name): black is OR.icons.default.
constexpr Entry plain(Id id, std::string_view name, std::string_view file)
{
    return {.id         = id,
            .name       = name,
            .file       = file,
            .colors     = {{{kBlack, Key::ICONS_DEFAULT}}},
            .colorCount = 1};
}

/// loadIcon(svg, png, name, colorKey[, scale]): black is @p key.
constexpr Entry single(Id id, std::string_view name, std::string_view file, Key key,
                       double scale = 1.0)
{
    return {.id         = id,
            .name       = name,
            .file       = file,
            .colors     = {{{kBlack, key}}},
            .colorCount = 1,
            .scale      = scale};
}

/// loadIcon(svg, png, name, Map.of(black, OR.icons.default, rgb, key)).
constexpr Entry tinted(Id id, std::string_view name, std::string_view file, QRgb rgb, Key key)
{
    return {.id         = id,
            .name       = name,
            .file       = file,
            .colors     = {{{kBlack, Key::ICONS_DEFAULT}, {rgb, key}}},
            .colorCount = 2};
}

/// The magnets' Map.of(0xE86B55, OR.icons.magnet.red, 0xE1E3E2, OR.icons.magnet.white): no black,
/// so their black and white stay as drawn.
constexpr Entry magnet(Id id, std::string_view name, std::string_view file)
{
    return {.id     = id,
            .name   = name,
            .file   = file,
            .colors = {{{0xE86B55, Key::ICONS_MAGNET_RED}, {0xE1E3E2, Key::ICONS_MAGNET_WHITE}}},
            .colorCount = 2};
}

// Icons.java, in its order. The SVG files are in gui/resources/icons/actions/.
constexpr std::array<Entry, Icons::kAllIds.size()> kEntries{{
    tinted(Id::FILE_NEW, "FILE_NEW", "file-plus-corner", 0x499C54, Key::ICONS_PLUS),
    plain(Id::FILE_OPEN, "FILE_OPEN", "file"),
    tinted(Id::FILE_OPEN_RECENT, "FILE_OPEN_RECENT", "file-clock", 0x2D2DBD, Key::ICONS_TINT_COLOR),
    tinted(Id::FILE_OPEN_EXAMPLE, "FILE_OPEN_EXAMPLE", "file-info", 0x2D2DBD,
           Key::ICONS_TINT_COLOR),
    plain(Id::FILE_SAVE, "FILE_SAVE", "save"),
    tinted(Id::FILE_SAVE_AS, "FILE_SAVE_AS", "save-as", 0xFAC132, Key::ICONS_PENCIL),
    tinted(Id::FILE_CLOSE, "FILE_CLOSE", "circle-x", 0xc80000, Key::RED),
    single(Id::FILE_QUIT, "FILE_QUIT", "power", Key::RED),
    single(Id::EDIT_UNDO, "EDIT_UNDO", "undo", Key::ICONS_UNDO),
    single(Id::EDIT_REDO, "EDIT_REDO", "redo", Key::ICONS_REDO),
    tinted(Id::EDIT_EDIT, "EDIT_EDIT", "square-pen", 0xFAC132, Key::ICONS_PENCIL),
    tinted(Id::EDIT_RENAME, "EDIT_RENAME", "rename", 0x2D2DBD, Key::ICONS_TINT_COLOR),
    plain(Id::EDIT_CUT, "EDIT_CUT", "scissors"),
    plain(Id::EDIT_COPY, "EDIT_COPY", "copy"),
    tinted(Id::EDIT_PASTE, "EDIT_PASTE", "clipboard-paste", 0xc80000, Key::ICONS_IMPORT),
    tinted(Id::EDIT_DUPLICATE, "EDIT_DUPLICATE", "copy-plus", 0x2d2dbd, Key::ICONS_DUPLICATE),
    single(Id::EDIT_DELETE, "EDIT_DELETE", "trash", Key::ICONS_DELETE),
    single(Id::HELP_ABOUT, "HELP_ABOUT", "info", Key::ICONS_HELP),
    tinted(Id::ZOOM_IN, "ZOOM_IN", "zoom-in", 0x499C54, Key::ICONS_ZOOM_IN),
    tinted(Id::ZOOM_OUT, "ZOOM_OUT", "zoom-out", 0xC80000, Key::ICONS_ZOOM_OUT),
    tinted(Id::ZOOM_RESET, "ZOOM_RESET", "zoom-fit", 0x2d2dbd, Key::ICONS_ZOOM_FIT),
    single(Id::PAN_VIEW, "PAN_VIEW", "move", Key::PURPLE),
    tinted(Id::RULER, "RULER", "ruler-dimension-line", 0xFAC132, Key::RULER),
    magnet(Id::SNAP_CLICK, "SNAP_CLICK", "magnet-click"),
    magnet(Id::SNAP, "SNAP", "magnet-filled"),
    plain(Id::PREFERENCES, "PREFERENCES", "cog"),
    tinted(Id::CONFIGURE, "CONFIGURE", "file-cog", 0x2D2DBD, Key::ICONS_TINT_COLOR),
    plain(Id::HELP, "HELP", "info"),
    single(Id::UP, "UP", "arrow-big-up-dash", Key::ICONS_MOVE_UP),
    single(Id::DOWN, "DOWN", "arrow-big-down-dash", Key::ICONS_MOVE_DOWN),
    plain(Id::RESET, "RESET", "rotate-cw"),
    single(Id::MASS_OVERRIDE, "MASS_OVERRIDE", "weight", Key::ICONS_OVERRIDE),
    single(Id::MASS_OVERRIDE_SUBCOMPONENT, "MASS_OVERRIDE_SUBCOMPONENT", "weight",
           Key::ICONS_OVERRIDE_SUBCOMPONENT, kSubcomponentScale),
    single(Id::CG_OVERRIDE, "CG_OVERRIDE", "cg-override", Key::ICONS_OVERRIDE),
    single(Id::CG_OVERRIDE_SUBCOMPONENT, "CG_OVERRIDE_SUBCOMPONENT", "cg-override",
           Key::ICONS_OVERRIDE_SUBCOMPONENT, kSubcomponentScale),
    single(Id::CD_OVERRIDE, "CD_OVERRIDE", "cd-override", Key::ICONS_OVERRIDE),
    single(Id::CD_OVERRIDE_SUBCOMPONENT, "CD_OVERRIDE_SUBCOMPONENT", "cd-override",
           Key::ICONS_OVERRIDE_SUBCOMPONENT, kSubcomponentScale),
    plain(Id::COMPONENT_HIDDEN, "COMPONENT_HIDDEN", "eye-off"),
    plain(Id::COMPONENT_SHOWING, "COMPONENT_SHOWING", "eye"),
    plain(Id::COMPONENT_DISABLED, "COMPONENT_DISABLED", "ban"),
    tinted(Id::COMPONENT_ENABLED, "COMPONENT_ENABLED", "circle-check", 0x499C54, Key::GREEN),
    tinted(Id::LOCKED, "LOCKED", "lock", 0xf1c066, Key::ICONS_LOCKED),
    plain(Id::UNLOCKED, "UNLOCKED", "lock-open"),
}};

/// The window icons' sizes: GUIUtil's (it also asks for icon-032.png, which OpenRocket does not
/// have, and skips it).
constexpr std::array<std::string_view, 4> kWindowIconSizes{"016", "048", "064", "256"};

const Entry& entry(Id id)
{
    const auto index = static_cast<std::size_t>(id);
    QTROCKET_ASSERT(index < kEntries.size() && kEntries.at(index).id == id);
    return kEntries.at(index);
}

}  // namespace

QIcon Icons::icon(Id id)
{
    const QString path = resourcePath(id);
    if (!QFile::exists(path))
    {
        Guard::reportErrorCondition(QStringLiteral("Image file %1 not found, ignoring.").arg(path));
        return {};
    }
    return QIcon(makeQtOwned<SvgIconEngine>(path, colorMap(id)));
}

std::vector<Placeholder> Icons::colorMap(Id id)
{
    const Entry&             value = entry(id);
    std::vector<Placeholder> placeholders;
    placeholders.reserve(value.colorCount);
    for (std::size_t index = 0; index < value.colorCount; ++index)
    {
        placeholders.push_back(value.colors.at(index));
    }
    return placeholders;
}

QString Icons::resourcePath(Id id)
{
    return QStringLiteral(":/icons/actions/") + QtStrings::fromUtf8(entry(id).file) +
           QStringLiteral(".svg");
}

QString Icons::name(Id id)
{
    return QtStrings::fromUtf8(entry(id).name);
}

double Icons::scaleMultiplier(Id id)
{
    return entry(id).scale;
}

int Icons::iconHeight(Id id, const QFont& font)
{
    const double height =
        QFontInfo(font).pixelSize() * kIconFontSizeMultiplier * scaleMultiplier(id);
    return static_cast<int>(std::lround(height));
}

QIcon Icons::windowIcon()
{
    QIcon icon;
    for (const QString& path : windowIconResources())
    {
        icon.addFile(path);
    }
    return icon;
}

QStringList Icons::windowIconResources()
{
    QStringList resources;
    for (const std::string_view size : kWindowIconSizes)
    {
        resources.append(QStringLiteral(":/icons/app/icon-") + QtStrings::fromUtf8(size) +
                         QStringLiteral(".png"));
    }
    return resources;
}

}  // namespace QtRocket::Gui
