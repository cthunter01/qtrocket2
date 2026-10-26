#pragma once

#include <array>
#include <utility>
#include <vector>

#include <QIcon>
#include <QRgb>
#include <QString>
#include <QStringList>

#include "QtRocket/gui/util/ThemeColors.h"

class QFont;

namespace QtRocket::Gui
{

/// OpenRocket's icons (Icons.java) that the GUI shows: the File and Edit actions (and the flight
/// configuration tab's Rename), the component tree's markers, the figure's tools and the window
/// icon. Each is one of OpenRocket's SVGs, in
/// the resources at :/icons/actions/ (lower-case names: Icons.java asks for cd-override.svg, the
/// file is CD-override.svg), drawn by an SvgIconEngine with the same colour mapping as
/// Icons.java's loadIcon()/loadSvgIcon(): black is OR.icons.default unless the icon names another
/// key for it, and the other placeholders are the keys Icons.java names. OpenRocket's raster
/// fallbacks (pix/icons/*.png) are not needed: every SVG is there.
///
/// Size: OpenRocket sizes an SVG icon to 1.25 times the font size (times 0.75 for the
/// "subcomponent" override markers); a QIcon has no size of its own, so whoever draws one picks
/// it (Qt's styles for menus and buttons, iconHeight() for the tree's markers).
class Icons
{
public:
    /// The icons, by Icons.java's names.
    enum class Id
    {
        FILE_NEW,
        FILE_OPEN,
        FILE_OPEN_RECENT,
        FILE_OPEN_EXAMPLE,
        FILE_SAVE,
        FILE_SAVE_AS,
        FILE_CLOSE,
        FILE_QUIT,
        EDIT_UNDO,
        EDIT_REDO,
        EDIT_EDIT,
        EDIT_RENAME,
        EDIT_CUT,
        EDIT_COPY,
        EDIT_PASTE,
        EDIT_DUPLICATE,
        EDIT_DELETE,
        HELP_ABOUT,
        ZOOM_IN,
        ZOOM_OUT,
        ZOOM_RESET,
        PAN_VIEW,
        RULER,
        SNAP_CLICK,
        SNAP,
        PREFERENCES,
        CONFIGURE,
        HELP,
        UP,
        DOWN,
        RESET,
        MASS_OVERRIDE,
        MASS_OVERRIDE_SUBCOMPONENT,
        CG_OVERRIDE,
        CG_OVERRIDE_SUBCOMPONENT,
        CD_OVERRIDE,
        CD_OVERRIDE_SUBCOMPONENT,
        COMPONENT_HIDDEN,
        COMPONENT_SHOWING,
        COMPONENT_DISABLED,
        COMPONENT_ENABLED,
        LOCKED,
        UNLOCKED,
    };

    /// Every icon, in declaration order (Icons.java's order).
    static constexpr std::array kAllIds{
        Id::FILE_NEW,
        Id::FILE_OPEN,
        Id::FILE_OPEN_RECENT,
        Id::FILE_OPEN_EXAMPLE,
        Id::FILE_SAVE,
        Id::FILE_SAVE_AS,
        Id::FILE_CLOSE,
        Id::FILE_QUIT,
        Id::EDIT_UNDO,
        Id::EDIT_REDO,
        Id::EDIT_EDIT,
        Id::EDIT_RENAME,
        Id::EDIT_CUT,
        Id::EDIT_COPY,
        Id::EDIT_PASTE,
        Id::EDIT_DUPLICATE,
        Id::EDIT_DELETE,
        Id::HELP_ABOUT,
        Id::ZOOM_IN,
        Id::ZOOM_OUT,
        Id::ZOOM_RESET,
        Id::PAN_VIEW,
        Id::RULER,
        Id::SNAP_CLICK,
        Id::SNAP,
        Id::PREFERENCES,
        Id::CONFIGURE,
        Id::HELP,
        Id::UP,
        Id::DOWN,
        Id::RESET,
        Id::MASS_OVERRIDE,
        Id::MASS_OVERRIDE_SUBCOMPONENT,
        Id::CG_OVERRIDE,
        Id::CG_OVERRIDE_SUBCOMPONENT,
        Id::CD_OVERRIDE,
        Id::CD_OVERRIDE_SUBCOMPONENT,
        Id::COMPONENT_HIDDEN,
        Id::COMPONENT_SHOWING,
        Id::COMPONENT_DISABLED,
        Id::COMPONENT_ENABLED,
        Id::LOCKED,
        Id::UNLOCKED,
    };

    Icons() = delete;

    /// The icon @p id, drawn in the application palette's colours at the time it is painted. A
    /// missing resource is reported as an error condition (Guard) and gives a null icon, as
    /// Icons.java reports it and returns null.
    [[nodiscard]] static QIcon icon(Id id);

    /// The resource of @p id, ":/icons/actions/<file>.svg".
    [[nodiscard]] static QString resourcePath(Id id);

    /// Icons.java's name of @p id ("FILE_NEW").
    [[nodiscard]] static QString name(Id id);

    /// Icons.java's colour map of @p id: each placeholder RGB of its SVG (0xRRGGBB; 0 is black)
    /// and the key whose colour stands in for it when it is drawn (SvgIconEngine).
    [[nodiscard]] static std::vector<std::pair<QRgb, ThemeColors::Key>> colorMap(Id id);

    /// Icons.java's scale multiplier of @p id: 0.75 for the *_SUBCOMPONENT markers, else 1.
    [[nodiscard]] static double scaleMultiplier(Id id);

    /// The height OpenRocket gives the icon @p id next to text in @p font: the font's pixel size
    /// times 1.25 (ICON_FONT_SIZE_MULTIPLIER) times scaleMultiplier(), rounded (the icons are
    /// square).
    [[nodiscard]] static int iconHeight(Id id, const QFont& font);

    /// The window icon: OpenRocket's icon-016, -048, -064 and -256.png (GUIUtil's application
    /// icons; GUIUtil also asks for an icon-032.png, which OpenRocket does not have).
    [[nodiscard]] static QIcon windowIcon();

    /// The resources of windowIcon(), ":/icons/app/icon-<size>.png", smallest first.
    [[nodiscard]] static QStringList windowIconResources();
};

}  // namespace QtRocket::Gui
