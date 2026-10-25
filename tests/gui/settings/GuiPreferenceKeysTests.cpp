#include "QtRocket/gui/settings/GuiPreferenceKeys.h"

#include <string>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/rocket/ComponentKind.h"

namespace QtRocket::Gui
{

namespace
{

using GuiPreferenceKeysTest = Test::GuiTest;
namespace Keys              = GuiPreferenceKeys;

TEST_F(GuiPreferenceKeysTest, KeysMatchOpenRocket)
{
    // SwingPreferences.java's constants.
    EXPECT_EQ(Keys::kNodeWindows, "windows");
    EXPECT_EQ(Keys::kLockClickDragRotation, "LockClickDragRotation");
    EXPECT_EQ(Keys::kUpdateRocketWhileDraggingPoint, "UpdateRocketWhileDraggingPoint");
    EXPECT_EQ(Keys::kDefault2dBackgroundColor, "DesignView.default2DBackgroundColor");
    EXPECT_EQ(Keys::kDefault3dBackgroundColor, "DesignView.default3DBackgroundColor");
    EXPECT_EQ(Keys::kDefault2dTextColor, "DesignView.default2DTextColor");
    EXPECT_EQ(Keys::kDefault3dTextColor, "DesignView.default3DTextColor");
    EXPECT_EQ(Keys::kAutoOpenPartsLibrary, "AutoOpenPartsLibrary");
    EXPECT_EQ(Keys::kNodeTables, "tables");
    // ComponentPresetChooserDialog's TABLE_ID and its "AlwaysOpenPreset", TableUIPreferences'
    // prefixes.
    EXPECT_EQ(Keys::kComponentPresetTableId, "CmpPrst.");
    EXPECT_EQ(Keys::kAlwaysOpenPresetSuffix, "AlwaysOpenPreset");
    EXPECT_EQ(Keys::kTableColumnWidthPrefix, ".cw.");
    EXPECT_EQ(Keys::kTableColumnOrderPrefix, ".co.");
    EXPECT_EQ(Keys::kTableColumnVisibilityPrefix, ".cv.");
    // GUIUtil.getDPI(), InnerTubeConfig.PREF_SEPARATION_RELATIVE, MRUDesignFile.
    EXPECT_EQ(Keys::kDpi, "DPI");
    EXPECT_EQ(Keys::kInnerTubeSeparationRelative, "InnerTubeSeparationRelative");
    EXPECT_EQ(Keys::kMruFileListProperty, "MRUFileList");
    EXPECT_EQ(Keys::kMruFileListMaxSize, 9);
    // QtRocket's own.
    EXPECT_EQ(Keys::kWindowGeometryPrefix, "geometry.");
}

TEST_F(GuiPreferenceKeysTest, DefaultsMatchOpenRocket)
{
    // isClickDragRotationLocked(), isUpdateRocketWhileDraggingPoint(), getDPI() and
    // InnerTubeConfig's getBoolean(PREF_SEPARATION_RELATIVE, true).
    EXPECT_FALSE(Keys::kLockClickDragRotationDefault);
    EXPECT_FALSE(Keys::kUpdateRocketWhileDraggingPointDefault);
    EXPECT_EQ(Keys::kDpiDefault, 0);
    EXPECT_TRUE(Keys::kInnerTubeSeparationRelativeDefault);
    // isAutoOpenPartsLibrary(), getBoolean(<name> + "AlwaysOpenPreset", true) and
    // TableUIPreferences' getBoolean(<visibility key>, true).
    EXPECT_TRUE(Keys::kAutoOpenPartsLibraryDefault);
    EXPECT_TRUE(Keys::kAlwaysOpenPresetDefault);
    EXPECT_TRUE(Keys::kTableColumnVisibleDefault);
}

TEST_F(GuiPreferenceKeysTest, ThePartsLibraryKeysNameTheComponentInEnglish)
{
    // getComponentName() + "AlwaysOpenPreset": one key per kind of component, also for the kinds
    // whose presets are body tubes.
    EXPECT_EQ(Keys::alwaysOpenPresetKey(ComponentKind::BODY_TUBE), "Body TubeAlwaysOpenPreset");
    EXPECT_EQ(Keys::alwaysOpenPresetKey(ComponentKind::INNER_TUBE), "Inner TubeAlwaysOpenPreset");
    EXPECT_EQ(Keys::alwaysOpenPresetKey(ComponentKind::TUBE_FIN_SET),
              "Tube Fin SetAlwaysOpenPreset");
    EXPECT_EQ(Keys::alwaysOpenPresetKey(ComponentKind::NOSE_CONE), "Nose ConeAlwaysOpenPreset");
    // TABLE_ID + getComponentName(), then TableUIPreferences' prefix and the column's identifier.
    EXPECT_EQ(Keys::componentPresetTableName(ComponentKind::CENTERING_RING),
              "CmpPrst.Centering Ring");
    const std::string table = Keys::componentPresetTableName(ComponentKind::BODY_TUBE);
    EXPECT_EQ(Keys::tableColumnWidthKey(table, "Manufacturer"),
              "CmpPrst.Body Tube.cw.Manufacturer");
    EXPECT_EQ(Keys::tableColumnOrderKey(table, "Part No."), "CmpPrst.Body Tube.co.Part No.");
    EXPECT_EQ(Keys::tableColumnVisibilityKey(table, "Legacy"), "CmpPrst.Body Tube.cv.Legacy");
}

TEST_F(GuiPreferenceKeysTest, RecentFileKeysAreNumberedFromTheNewest)
{
    EXPECT_EQ(Keys::mruFileListKey(0), "MRUFileList0");
    EXPECT_EQ(Keys::mruFileListKey(Keys::kMruFileListMaxSize - 1), "MRUFileList8");
}

}  // namespace

}  // namespace QtRocket::Gui
