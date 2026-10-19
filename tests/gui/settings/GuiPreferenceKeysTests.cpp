#include "QtRocket/gui/settings/GuiPreferenceKeys.h"

#include <gtest/gtest.h>

#include "GuiTestSupport.h"

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
}

TEST_F(GuiPreferenceKeysTest, RecentFileKeysAreNumberedFromTheNewest)
{
    EXPECT_EQ(Keys::mruFileListKey(0), "MRUFileList0");
    EXPECT_EQ(Keys::mruFileListKey(Keys::kMruFileListMaxSize - 1), "MRUFileList8");
}

}  // namespace

}  // namespace QtRocket::Gui
