#pragma once

#include <format>
#include <string>
#include <string_view>

#include "QtRocket/rocket/ComponentKind.h"

/// The preference keys only the GUI uses, with the same string values as OpenRocket's (most are
/// constants of SwingPreferences; the others are named where OpenRocket uses them), so that
/// QtRocket's preferences file reads like OpenRocket's store key for key. The keys of the core
/// (ApplicationPreferences) are in QtRocket/preferences/PreferenceKeys.h.
///
/// Naming as in PreferenceKeys.h: a Java constant `NAME_IN_UPPER_CASE` is `kNameInUpperCase`
/// (DEFAULT_2D_BACKGROUND_COLOR -> kDefault2dBackgroundColor); a key that Java writes as a string
/// literal, or a constant whose Java name says nothing outside its class, is named after its
/// text, with its Java origin in the comment. Each key's default is OpenRocket's, as a
/// k...Default constant beside it.
///
/// Not here yet, with the milestone of the code that reads them: SwingPreferences' UI_SCALE,
/// UI_FONT_SIZE, UI_FONT_STYLE, UI_FONT_TRACKING (the UI page, M5), UPDATE_PLATFORM (update
/// checks, not ported), NODE_PLOT_DEFAULT_APPEARANCES, "backgroundFlight", "exports" and
/// "exportsComponentAnalysis" (M4), "decalEditorPreference" (M5); never: getTableColumnWidth()'s
/// "cw.<key>.<index>" in NODE_TABLES, which no code of OpenRocket calls.
///
/// Milestone 3's keys of the motor chooser and the preset favourites are the core's, since the
/// core reads them (QtRocket/preferences/PreferenceKeys.h: MotorHideSimilar,
/// MotorHideUnavailable, MotorNameColumn, PreferredThrustCurveMotors, MatchForeDiameter,
/// MatchAftDiameter; motor/MotorPreferences: "excludedMotorManufacturers",
/// "motorFilterLimitLength", "motorFilterLimitDiameter"; rocket/preset/ComponentPresetFavorites:
/// "favoritePresets"). Here are the parts library's.
namespace QtRocket::Gui::GuiPreferenceKeys
{

// ---------------------------------------------------------------- windows (SwingPreferences)

/// SwingPreferences.NODE_WINDOWS: the node of the windows' geometry (WindowGeometry).
inline constexpr std::string_view kNodeWindows = "windows";
/// The key of a window's geometry in kNodeWindows is this prefix and the window's name
/// ("geometry.MainWindow"), its value QWidget::saveGeometry() in Base64. QtRocket's own:
/// OpenRocket stores "position.<class>" ("x,y") and "size.<class>" ("w,h" or "max") instead.
inline constexpr std::string_view kWindowGeometryPrefix = "geometry.";

// --------------------------------------------------------------- the design view and figures

/// SwingPreferences.LOCK_CLICK_DRAG_ROTATION: whether a left drag in the figure no longer rotates
/// the rocket.
inline constexpr std::string_view kLockClickDragRotation        = "LockClickDragRotation";
inline constexpr bool             kLockClickDragRotationDefault = false;

/// SwingPreferences.UPDATE_ROCKET_WHILE_DRAGGING_POINT: whether the fin point editor changes the
/// rocket while a point is dragged (false: the rocket is frozen until the point is dropped).
inline constexpr std::string_view kUpdateRocketWhileDraggingPoint =
    "UpdateRocketWhileDraggingPoint";
inline constexpr bool kUpdateRocketWhileDraggingPointDefault = false;

/// SwingPreferences.DEFAULT_2D_BACKGROUND_COLOR, DEFAULT_3D_BACKGROUND_COLOR,
/// DEFAULT_2D_TEXT_COLOR, DEFAULT_3D_TEXT_COLOR: the design view's colours, "R,G,B"
/// (Preferences::getColor()). No default: an absent key means the theme's colour.
inline constexpr std::string_view kDefault2dBackgroundColor = "DesignView.default2DBackgroundColor";
inline constexpr std::string_view kDefault3dBackgroundColor = "DesignView.default3DBackgroundColor";
inline constexpr std::string_view kDefault2dTextColor       = "DesignView.default2DTextColor";
inline constexpr std::string_view kDefault3dTextColor       = "DesignView.default3DTextColor";

/// GUIUtil.getDPI()'s "DPI": the screen's resolution in tenths of a dot per inch for the figure's
/// absolute scale; a value below 10 (the default 0) means the screen's own resolution.
inline constexpr std::string_view kDpi        = "DPI";
inline constexpr int              kDpiDefault = 0;

// ------------------------------------------------------------------- component dialogs

/// InnerTubeConfig.PREF_SEPARATION_RELATIVE: whether the cluster separation of an inner tube is
/// entered relative to its outer diameter.
inline constexpr std::string_view kInnerTubeSeparationRelative = "InnerTubeSeparationRelative";
inline constexpr bool             kInnerTubeSeparationRelativeDefault = true;

// ------------------------------------------------------------------- the parts library

/// SwingPreferences.AUTO_OPEN_PARTS_LIBRARY: whether a new component's configuration dialog
/// opens the parts library at once (ComponentConfigDialog.showDialog(), for a component whose
/// kind takes presets and whose kind's kAlwaysOpenPresetSuffix preference is on too); the
/// General preferences page's "Automatically open the Parts Library dialog".
inline constexpr std::string_view kAutoOpenPartsLibrary        = "AutoOpenPartsLibrary";
inline constexpr bool             kAutoOpenPartsLibraryDefault = true;

/// The parts library's "Always open this dialog when creating a new %1", per kind of component:
/// the key is the component's English name and this suffix ("Body TubeAlwaysOpenPreset":
/// ComponentConfigDialog and ComponentPresetChooserDialog's getComponentName() +
/// "AlwaysOpenPreset"), alwaysOpenPresetKey().
inline constexpr std::string_view kAlwaysOpenPresetSuffix  = "AlwaysOpenPreset";
inline constexpr bool             kAlwaysOpenPresetDefault = true;

/// The key of kAlwaysOpenPresetSuffix's preference for a component of @p kind: its English name
/// (displayName(), as OpenRocket's getComponentName() with its English messages; never a
/// translated text, so that the key is the same in every language) and the suffix.
[[nodiscard]] inline std::string alwaysOpenPresetKey(ComponentKind kind)
{
    return std::format("{}{}", displayName(kind), kAlwaysOpenPresetSuffix);
}

/// SwingPreferences.NODE_TABLES: the node of the tables' column layouts (TableUIPreferences): for
/// a table named T, the keys T + kTableColumnWidthPrefix + the column (its width in pixels,
/// an int), T + kTableColumnOrderPrefix + the column (its view index) and T +
/// kTableColumnVisibilityPrefix + the column (whether it is shown, true by default), where the
/// column is its identifier (Swing's TableColumn.getIdentifier(): its header text).
inline constexpr std::string_view kNodeTables = "tables";
/// TableUIPreferences.TABLE_COLUMN_WIDTH_PREFIX, ..._ORDER_PREFIX, ..._VISIBILITY_PREFIX.
inline constexpr std::string_view kTableColumnWidthPrefix      = ".cw.";
inline constexpr std::string_view kTableColumnOrderPrefix      = ".co.";
inline constexpr std::string_view kTableColumnVisibilityPrefix = ".cv.";
/// The default of a column's kTableColumnVisibilityPrefix key.
inline constexpr bool kTableColumnVisibleDefault = true;

/// ComponentPresetChooserDialog.TABLE_ID: the parts library's table is named this prefix and the
/// component's English name, componentPresetTableName().
inline constexpr std::string_view kComponentPresetTableId = "CmpPrst.";

/// The name of the parts library's table for a component of @p kind: "CmpPrst.Body Tube"
/// (TABLE_ID + getComponentName(), the English name as alwaysOpenPresetKey()'s).
[[nodiscard]] inline std::string componentPresetTableName(ComponentKind kind)
{
    return std::format("{}{}", kComponentPresetTableId, displayName(kind));
}

/// The keys of the column @p column of the table @p table in kNodeTables: its width, its order
/// and its visibility ("CmpPrst.Body Tube.cw.Manufacturer").
[[nodiscard]] inline std::string tableColumnWidthKey(std::string_view table,
                                                     std::string_view column)
{
    return std::format("{}{}{}", table, kTableColumnWidthPrefix, column);
}
[[nodiscard]] inline std::string tableColumnOrderKey(std::string_view table,
                                                     std::string_view column)
{
    return std::format("{}{}{}", table, kTableColumnOrderPrefix, column);
}
[[nodiscard]] inline std::string tableColumnVisibilityKey(std::string_view table,
                                                          std::string_view column)
{
    return std::format("{}{}{}", table, kTableColumnVisibilityPrefix, column);
}

// ---------------------------------------------------------------------- recent files

/// MRUDesignFile.MRU_FILE_LIST_PROPERTY: the recent design files are the keys "MRUFileList0"
/// (the newest) to "MRUFileList8" of the root node, each an absolute path (mruFileListKey()).
inline constexpr std::string_view kMruFileListProperty = "MRUFileList";
/// MRUDesignFile.MAX_SIZE: the number of recent files kept.
inline constexpr int kMruFileListMaxSize = 9;

/// The key of the recent file @p index (0 the newest): "MRUFileList<index>".
[[nodiscard]] inline std::string mruFileListKey(int index)
{
    return std::format("{}{}", kMruFileListProperty, index);
}

}  // namespace QtRocket::Gui::GuiPreferenceKeys
