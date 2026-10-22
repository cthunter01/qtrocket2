#include "QtRocket/gui/dialogs/GeneralPreferencesPanel.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <QCheckBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/unit/UnitGroup.h"
#include "ScriptedUserPrompts.h"

namespace QtRocket::Gui
{

namespace
{

using Kind = Gui::Test::ScriptedUserPrompts::Kind;

/// The page over a store in memory, its questions answered by a script.
class GeneralPreferencesPanelTest : public Gui::Test::GuiTest
{
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        m_page = std::make_unique<GeneralPreferencesPanel>(m_preferences, m_prompts, nullptr);
    }

    void TearDown() override
    {
        m_page = nullptr;
        GuiTest::TearDown();
    }

    InMemoryPreferences                      m_preferences;
    Gui::Test::ScriptedUserPrompts           m_prompts;
    std::unique_ptr<GeneralPreferencesPanel> m_page;
};

TEST_F(GeneralPreferencesPanelTest, TheCheckBoxesStoreTheirStates)
{
    // PreferencesProbe: each click stores the new state.
    EXPECT_FALSE(m_page->openLastDesign()->isChecked());
    m_page->openLastDesign()->click();
    EXPECT_EQ(m_preferences.get(PreferenceKeys::kAutoOpenLastDesign), "true");
    EXPECT_TRUE(m_page->showDiscardConfirmation()->isChecked());
    m_page->showDiscardConfirmation()->click();
    EXPECT_EQ(m_preferences.get(PreferenceKeys::kShowDiscardPreferencesConfirmation), "false");
    EXPECT_EQ(m_page->openLastDesign()->objectName(), QStringLiteral("AutoOpenLastDesign"));
    EXPECT_EQ(m_page->showDiscardConfirmation()->objectName(),
              QStringLiteral("IgnoreDiscardPreferencesWarning"));
}

TEST_F(GeneralPreferencesPanelTest, ResetAllPreferencesAsksFirst)
{
    m_preferences.setAutoOpenLastDesignOnStartup(true);
    const QSignalSpy cleared(m_page.get(), &GeneralPreferencesPanel::preferencesCleared);
    m_prompts.answerYesNo(false);
    m_page->resetAllPreferencesButton()->click();
    ASSERT_EQ(m_prompts.prompts().size(), 1U);
    const Gui::Test::ScriptedUserPrompts::Prompt& question = m_prompts.prompts().front();
    EXPECT_EQ(question.kind, Kind::YES_NO);
    EXPECT_EQ(question.title, QStringLiteral("Reset preferences?"));
    EXPECT_EQ(
        question.texts,
        QStringList({QStringLiteral("WARNING"),
                     QStringLiteral("Are you sure you want to reset all your preferences?")}));
    EXPECT_TRUE(m_preferences.isAutoOpenLastDesignOnStartupEnabled());
    EXPECT_EQ(cleared.count(), 0);
}

TEST_F(GeneralPreferencesPanelTest, ResetAllPreferencesEmptiesTheStoreAndResetsTheUnits)
{
    m_preferences.setAutoOpenLastDesignOnStartup(true);
    m_preferences.getNode("windows").put("position.ComponentConfigDialog", "10,20");
    static_cast<void>(unitGroup(UnitGroupId::LENGTH).setDefaultUnit(std::string_view{"in"}));
    const QSignalSpy cleared(m_page.get(), &GeneralPreferencesPanel::preferencesCleared);
    m_prompts.answerYesNo(true);
    EXPECT_TRUE(m_page->resetAllPreferences());

    // SwingPreferences.clearPreferences(): the node removed, the default units reset and stored.
    EXPECT_EQ(m_preferences.keys(), std::vector<std::string>{});
    EXPECT_EQ(m_preferences.childrenNames(), std::vector<std::string>{"units"});
    EXPECT_EQ(m_preferences.getNode("units").get("LENGTH"), "cm");
    EXPECT_EQ(unitGroup(UnitGroupId::LENGTH).getDefaultUnit().getUnit(), "cm");
    // Then the message, with QtRocket's name.
    ASSERT_EQ(m_prompts.prompts().size(), 2U);
    const Gui::Test::ScriptedUserPrompts::Prompt& message = m_prompts.prompts().back();
    EXPECT_EQ(message.kind, Kind::MESSAGE);
    EXPECT_EQ(message.title, QStringLiteral("Reload QtRocket"));
    EXPECT_EQ(message.texts,
              QStringList({QStringLiteral("WARNING"),
                           QStringLiteral("You may need to restart QtRocket for some of the "
                                          "changes to take effect.")}));
    EXPECT_EQ(cleared.count(), 1);
}

TEST_F(GeneralPreferencesPanelTest, TheResetButtonIsJavas)
{
    EXPECT_EQ(m_page->resetAllPreferencesButton()->text(), QStringLiteral("Reset all preferences"));
    EXPECT_EQ(m_page->resetAllPreferencesButton()->toolTip(),
              QStringLiteral("Reset all the preferences, including cached preferences (UI "
                             "settings, recent files, etc.)"));
}

}  // namespace

}  // namespace QtRocket::Gui
