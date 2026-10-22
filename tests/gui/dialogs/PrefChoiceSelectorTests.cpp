#include "QtRocket/gui/dialogs/PrefChoiceSelector.h"

#include <array>
#include <optional>
#include <string>

#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "dialogs/PreferencesPageTranscript.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using PrefChoiceSelectorTest = Gui::Test::GuiTest;

/// The insert positions (pref.dlg.PrefChoiseSelector1..3).
[[nodiscard]] QStringList positions()
{
    return {QStringLiteral("Always ask"), QStringLiteral("Insert in middle"),
            QStringLiteral("Add to end")};
}

TEST_F(PrefChoiceSelectorTest, ShowsTheStoredChoiceAndTheFirstWithoutOne)
{
    InMemoryPreferences      preferences;
    const PrefChoiceSelector absent(preferences, PreferenceKeys::kStageInsertPositionKey,
                                    positions(), nullptr);
    EXPECT_EQ(absent.objectName(), QStringLiteral("StageInsertPosition"));
    EXPECT_EQ(absent.preference(), "StageInsertPosition");
    EXPECT_EQ(absent.count(), 3);
    EXPECT_EQ(absent.currentText(), QStringLiteral("Always ask"));
    // Reading stores nothing.
    EXPECT_EQ(preferences.get(PreferenceKeys::kStageInsertPositionKey), std::nullopt);

    preferences.putInt(PreferenceKeys::kStageInsertPositionKey, 2);
    const PrefChoiceSelector stored(preferences, PreferenceKeys::kStageInsertPositionKey,
                                    positions(), nullptr);
    EXPECT_EQ(stored.currentText(), QStringLiteral("Add to end"));
}

TEST_F(PrefChoiceSelectorTest, AStoredChoiceOutOfRangeShowsTheFirst)
{
    // PreferencesProbe: -1 and 7 show "Always ask"; 3 throws ArrayIndexOutOfBoundsException in
    // OpenRocket (getChoice(key, 3, 0) lets it through), here it shows the first too.
    InMemoryPreferences preferences;
    for (const int choice : std::array{3, -1, 7})
    {
        preferences.putInt(PreferenceKeys::kBodyComponentInsertPositionKey, choice);
        const PrefChoiceSelector selector(
            preferences, PreferenceKeys::kBodyComponentInsertPositionKey, positions(), nullptr);
        EXPECT_EQ(selector.currentText(), QStringLiteral("Always ask")) << choice;
        EXPECT_EQ(selector.storedChoice(), 0) << choice;
    }
}

TEST_F(PrefChoiceSelectorTest, AChosenItemStoresItsIndex)
{
    // PreferencesProbe: the choices 2, 0, 1 store 2, 0, 1.
    InMemoryPreferences preferences;
    PrefChoiceSelector  selector(preferences, PreferenceKeys::kBodyComponentInsertPositionKey,
                                 positions(), nullptr);
    for (const int index : std::array{2, 0, 1})
    {
        Gui::Test::chooseItem(selector, index);
        EXPECT_EQ(preferences.get(PreferenceKeys::kBodyComponentInsertPositionKey),
                  std::to_string(index));
        EXPECT_EQ(selector.currentIndex(), index);
    }
}

TEST_F(PrefChoiceSelectorTest, RefreshShowsAChoiceStoredElsewhere)
{
    InMemoryPreferences preferences;
    PrefChoiceSelector  selector(
        preferences, PreferenceKeys::kRocketInfoFontSize,
        {QStringLiteral("Small"), QStringLiteral("Medium"), QStringLiteral("Large")}, nullptr);
    preferences.putChoice(PreferenceKeys::kRocketInfoFontSize, 1);
    EXPECT_EQ(selector.currentText(), QStringLiteral("Small"));
    selector.refresh();
    EXPECT_EQ(selector.currentText(), QStringLiteral("Medium"));
}

TEST_F(PrefChoiceSelectorTest, TheWheelChoosesNothingWithoutTheFocus)
{
    InMemoryPreferences preferences;
    PrefChoiceSelector  selector(preferences, PreferenceKeys::kStageInsertPositionKey, positions(),
                                 nullptr);
    EXPECT_FALSE(Gui::Test::turnWheel(selector, -120));
    EXPECT_EQ(selector.currentIndex(), 0);
    EXPECT_EQ(preferences.get(PreferenceKeys::kStageInsertPositionKey), std::nullopt);
}

}  // namespace

}  // namespace QtRocket::Gui
