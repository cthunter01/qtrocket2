// The preferences dialog from the design windows: every window's Edit > Preferences opens it
// over that window, and its Apply, OK and Cancel refresh the design view of every window
// (WindowManager::showPreferences()).

#include <memory>
#include <string_view>
#include <vector>

#include <QAction>
#include <QCheckBox>
#include <QCoreApplication>
#include <QEvent>
#include <QPointer>
#include <QPushButton>
#include <QSize>
#include <QString>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
// The complete type of the combo boxes the tests choose in.
#include "QtRocket/gui/dialogs/DefaultUnitSelector.h"  // IWYU pragma: keep
#include "QtRocket/gui/dialogs/DesignPreferencesPanel.h"
#include "QtRocket/gui/dialogs/PreferencesDialog.h"
#include "QtRocket/gui/dialogs/UnitsPreferencesPanel.h"
#include "QtRocket/gui/figure/RocketFigure.h"
#include "QtRocket/gui/figure/RocketPanel.h"
#include "QtRocket/gui/figure/ScaleScrollArea.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/widgets/ConfigurationComboBox.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "TestGuiServices.h"
#include "dialogs/PreferencesPageTranscript.h"

namespace QtRocket::Gui
{

namespace
{

using LastWindowBehaviour = WindowManager::LastWindowBehaviour;

/// Delivers the posted events until the windows and their figures are done.
void settle()
{
    constexpr int kPasses = 4;
    for (int pass = 0; pass < kPasses; ++pass)
    {
        QCoreApplication::processEvents();
    }
}

/// Two windows of new designs over the test's services.
class PreferencesWindowTest : public Gui::Test::GuiTest
{
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        m_manager = std::make_unique<WindowManager>(m_services.services(), m_clipboard,
                                                    LastWindowBehaviour::EXIT);
        m_first   = &m_manager->newDesign();
        m_second  = &m_manager->newDesign();
        settle();
    }

    void TearDown() override
    {
        m_manager = nullptr;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        UnitGroup::resetDefaultUnits();
        GuiTest::TearDown();
    }

    /// Edit > Preferences of @p window: the dialog it opened, or null.
    [[nodiscard]] PreferencesDialog* openPreferences(MainWindow& window) const
    {
        window.menus().preferencesAction()->trigger();
        return m_manager->preferencesDialog();
    }

    [[nodiscard]] static std::string_view rulerUnit(const MainWindow& window)
    {
        return window.componentRocketPanel()->scrollArea().rulerUnit().getUnit();
    }

    Gui::Test::TestGuiServices     m_services;
    OpenRocketClipboard            m_clipboard;
    std::unique_ptr<WindowManager> m_manager;
    MainWindow*                    m_first  = nullptr;
    MainWindow*                    m_second = nullptr;
};

TEST_F(PreferencesWindowTest, EditPreferencesOpensTheDialogOverItsWindow)
{
    const QAction* const preferences = m_first->menus().preferencesAction();
    ASSERT_NE(preferences, nullptr);
    EXPECT_TRUE(preferences->isEnabled());
    PreferencesDialog* const dialogShown = openPreferences(*m_first);
    ASSERT_NE(dialogShown, nullptr);
    PreferencesDialog&                dialog = *dialogShown;
    const QPointer<PreferencesDialog> shown(&dialog);
    EXPECT_EQ(dialog.parentWidget(), m_first);
    EXPECT_TRUE(dialog.isVisible());
    EXPECT_EQ(dialog.windowModality(), Qt::ApplicationModal);
    dialog.okButton()->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(shown.isNull());
    EXPECT_EQ(m_manager->preferencesDialog(), nullptr);
}

TEST_F(PreferencesWindowTest, TheDesignViewsAreThoseOfEveryWindow)
{
    EXPECT_EQ(m_manager->rocketPanels(),
              (std::vector<RocketPanel*>{m_first->componentRocketPanel(),
                                         m_second->componentRocketPanel()}));
}

TEST_F(PreferencesWindowTest, ApplyAndCancelRefreshEveryWindow)
{
    const PreferencesDialog* const shown = openPreferences(*m_second);
    ASSERT_NE(shown, nullptr);
    const PreferencesDialog& dialog = *shown;
    Gui::Test::chooseItem(*dialog.unitsPage()->unitSelector(UnitGroupId::LENGTH), 3);  // in
    EXPECT_EQ(rulerUnit(*m_first), "cm");
    dialog.applyButton()->click();
    EXPECT_EQ(rulerUnit(*m_first), "in");
    EXPECT_EQ(rulerUnit(*m_second), "in");

    Gui::Test::chooseItem(*dialog.unitsPage()->unitSelector(UnitGroupId::LENGTH), 2);  // m
    m_services.prompts().answerDiscardChanges({.discard = true, .doNotAskAgain = false});
    dialog.cancelButton()->click();
    EXPECT_EQ(unitGroup(UnitGroupId::LENGTH).getDefaultUnit().getUnit(), "in");
    EXPECT_EQ(rulerUnit(*m_first), "in");
    EXPECT_EQ(rulerUnit(*m_second), "in");
}

TEST_F(PreferencesWindowTest, OkRefreshesTheConfigurationNamesOfEveryWindow)
{
    const PreferencesDialog* const shown = openPreferences(*m_first);
    ASSERT_NE(shown, nullptr);
    const PreferencesDialog& dialog = *shown;
    m_services.preferences().setDefaultFlightConfigName("Flight {motors}");
    dialog.okButton()->click();
    for (const MainWindow* window : {m_first, m_second})
    {
        EXPECT_EQ(window->componentRocketPanel()->configurationSelector().itemText(0),
                  QStringLiteral("Flight No motors"));
    }
}

TEST_F(PreferencesWindowTest, TheMarkersBoxBringsEveryFigureUpToDate)
{
    const PreferencesDialog* const shown = openPreferences(*m_first);
    ASSERT_NE(shown, nullptr);
    const PreferencesDialog& dialog = *shown;
    for (const MainWindow* window : {m_first, m_second})
    {
        window->componentRocketPanel()->figure().setMinimumSize(QSize(1, 1));
    }
    dialog.designPage()->showMarkers()->click();
    for (const MainWindow* window : {m_first, m_second})
    {
        const RocketFigure& figure = window->componentRocketPanel()->figure();
        EXPECT_EQ(figure.minimumSize(), figure.canvasSize());
    }
    dialog.okButton()->click();
}

TEST_F(PreferencesWindowTest, ANewDialogClosesTheOpenOneAsOk)
{
    // PreferencesDialog.showPreferences() disposes the previous dialog, which stores.
    PreferencesDialog* const shown = openPreferences(*m_first);
    ASSERT_NE(shown, nullptr);
    PreferencesDialog&                first = *shown;
    const QPointer<PreferencesDialog> firstShown(&first);
    Gui::Test::chooseItem(*first.unitsPage()->unitSelector(UnitGroupId::MASS), 1);  // kg
    const PreferencesDialog& second = m_manager->showPreferences(*m_second);
    EXPECT_NE(&second, firstShown.data());
    ASSERT_FALSE(firstShown.isNull());
    EXPECT_TRUE(first.isClosed());
    EXPECT_EQ(m_services.preferences().getNode(PreferenceKeys::kUnitsNode).get("MASS"), "kg");
    EXPECT_EQ(m_manager->preferencesDialog(), &second);
    EXPECT_EQ(second.parentWidget(), m_second);
    second.okButton()->click();
}

}  // namespace

}  // namespace QtRocket::Gui
