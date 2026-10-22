#include "QtRocket/gui/dialogs/UnitsPreferencesPanel.h"

#include <string>
#include <vector>

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/dialogs/DefaultUnitSelector.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "dialogs/PreferencesPageTranscript.h"

namespace QtRocket::Gui
{

namespace
{

using UnitsPreferencesPanelTest = Gui::Test::GuiTest;

TEST_F(UnitsPreferencesPanelTest, HasAComboBoxForEveryGroupInJavasOrder)
{
    InMemoryPreferences         preferences;
    const UnitsPreferencesPanel page(preferences, nullptr);
    std::vector<UnitGroupId>    expected(UnitsPreferencesPanel::kLeftGroups.begin(),
                                         UnitsPreferencesPanel::kLeftGroups.end());
    expected.insert(expected.end(), UnitsPreferencesPanel::kRightGroups.begin(),
                    UnitsPreferencesPanel::kRightGroups.end());
    std::vector<UnitGroupId> groups;
    for (const DefaultUnitSelector* selector : page.unitSelectors())
    {
        groups.push_back(selector->group());
    }
    EXPECT_EQ(groups, expected);
    EXPECT_EQ(page.unitSelector(UnitGroupId::LENGTH), page.unitSelectors().front());
    EXPECT_EQ(page.unitSelector(UnitGroupId::COEFFICIENT), nullptr);
    // Each label names its combo box (its buddy).
    EXPECT_EQ(page.secondaryStabilityLabel()->buddy(),
              page.unitSelector(UnitGroupId::SECONDARY_STABILITY));
}

TEST_F(UnitsPreferencesPanelTest, AChosenUnitIsInForceAtOnceAndStoredByTheDialog)
{
    InMemoryPreferences         preferences;
    const UnitsPreferencesPanel page(preferences, nullptr);
    DefaultUnitSelector*        velocity = page.unitSelector(UnitGroupId::VELOCITY);
    Gui::Test::chooseItem(*velocity, 1);
    EXPECT_EQ(unitGroup(UnitGroupId::VELOCITY).getDefaultUnit().getUnit(), "km/h");
    // The dialog's Apply or OK stores the default units (storeDefaultUnits()).
    EXPECT_EQ(preferences.findNode(PreferenceKeys::kUnitsNode), nullptr);
    UnitGroup::resetDefaultUnits();
}

TEST_F(UnitsPreferencesPanelTest, TheSecondaryStabilityRowFollowsItsCheckBox)
{
    InMemoryPreferences preferences;
    preferences.setDisplaySecondaryStability(false);
    const UnitsPreferencesPanel page(preferences, nullptr);
    const DefaultUnitSelector*  secondary = page.unitSelector(UnitGroupId::SECONDARY_STABILITY);
    EXPECT_FALSE(page.displaySecondaryStabilityCheckBox()->isChecked());
    EXPECT_FALSE(page.secondaryStabilityLabel()->isEnabled());
    EXPECT_FALSE(secondary->isEnabled());
    EXPECT_EQ(page.displaySecondaryStabilityCheckBox()->objectName(),
              QStringLiteral("DisplaySecondaryStability"));

    page.displaySecondaryStabilityCheckBox()->click();
    EXPECT_TRUE(preferences.isDisplaySecondaryStability());
    EXPECT_EQ(preferences.get(PreferenceKeys::kDisplaySecondaryStability), "true");
    EXPECT_TRUE(page.secondaryStabilityLabel()->isEnabled());
    EXPECT_TRUE(secondary->isEnabled());
}

TEST_F(UnitsPreferencesPanelTest, DefaultMetricAndImperialSetEveryGroupAndShowIt)
{
    InMemoryPreferences         preferences;
    const UnitsPreferencesPanel page(preferences, nullptr);
    page.defaultImperialButton()->click();
    EXPECT_EQ(page.unitSelector(UnitGroupId::LENGTH)->currentText(), QStringLiteral("in"));
    EXPECT_EQ(unitGroup(UnitGroupId::LENGTH).getDefaultUnit().getUnit(), "in");
    page.defaultMetricButton()->click();
    EXPECT_EQ(page.unitSelector(UnitGroupId::LENGTH)->currentText(), QStringLiteral("cm"));
    EXPECT_EQ(unitGroup(UnitGroupId::LENGTH).getDefaultUnit().getUnit(), "cm");
    EXPECT_EQ(preferences.findNode(PreferenceKeys::kUnitsNode), nullptr);
    UnitGroup::resetDefaultUnits();
}

}  // namespace

}  // namespace QtRocket::Gui
