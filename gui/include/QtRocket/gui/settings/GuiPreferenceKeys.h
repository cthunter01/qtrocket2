#pragma once

#include <format>
#include <string>
#include <string_view>

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
/// Not here yet, with the milestone of the code that reads them: SwingPreferences'
/// AUTO_OPEN_PARTS_LIBRARY (M3), UI_SCALE, UI_FONT_SIZE, UI_FONT_STYLE, UI_FONT_TRACKING (the UI
/// page, M5), UPDATE_PLATFORM (update checks, not ported), NODE_TABLES and
/// NODE_PLOT_DEFAULT_APPEARANCES, "backgroundFlight", "exports" and "exportsComponentAnalysis"
/// (M4), "favoritePresets" and "excludedMotorManufacturers" (M3), "decalEditorPreference" (M5).
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
