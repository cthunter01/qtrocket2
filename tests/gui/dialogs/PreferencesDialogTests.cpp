#include "QtRocket/gui/dialogs/PreferencesDialog.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QCheckBox>
#include <QCoreApplication>
#include <QDialog>
#include <QEvent>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/dialogs/DefaultUnitSelector.h"
#include "QtRocket/gui/dialogs/DesignPreferencesPanel.h"
#include "QtRocket/gui/dialogs/GeneralPreferencesPanel.h"
#include "QtRocket/gui/dialogs/PrefChoiceSelector.h"
#include "QtRocket/gui/dialogs/UnitsPreferencesPanel.h"
#include "QtRocket/gui/figure/DesignPhysics.h"
#include "QtRocket/gui/figure/RocketFigure.h"
#include "QtRocket/gui/figure/RocketPanel.h"
#include "QtRocket/gui/figure/ScaleScrollArea.h"
#include "QtRocket/gui/widgets/ConfigurationComboBox.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"
#include "dialogs/PreferencesPagePins.h"
#include "dialogs/PreferencesPageTranscript.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using Kind = Gui::Test::ScriptedUserPrompts::Kind;
using Tab  = PreferencesDialog::Tab;

/// Delivers the posted events until the layouts and the figure are done.
void settle()
{
    constexpr int kPasses = 4;
    for (int pass = 0; pass < kPasses; ++pass)
    {
        QCoreApplication::processEvents();
    }
}

/// The units node of @p preferences as "GROUP=unit", sorted.
[[nodiscard]] std::vector<std::string> storedUnits(Preferences& preferences)
{
    std::vector<std::string> units;
    const Preferences&       node = preferences.getNode(PreferenceKeys::kUnitsNode);
    for (const std::string& key : node.keys())
    {
        units.push_back(key + '=' + node.get(key).value_or("<none>"));
    }
    return units;
}

/// The name of the default unit of @p group.
[[nodiscard]] std::string defaultUnit(UnitGroupId group)
{
    return unitGroup(group).getDefaultUnit().getUnit();
}

/// The test's services (a store in memory, scripted questions), a design view of a new design
/// when the test makes one, and the dialog the test makes.
class PreferencesDialogTest : public Gui::Test::GuiTest
{
protected:
    void TearDown() override
    {
        m_dialog.reset();
        m_panel.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        GuiTest::TearDown();
    }

    [[nodiscard]] InMemoryPreferences& preferences() noexcept { return m_services.preferences(); }
    [[nodiscard]] Gui::Test::ScriptedUserPrompts& prompts() noexcept
    {
        return m_services.prompts();
    }

    /// The design view of a new design, 800 x 300 pixels and shown.
    RocketPanel& makePanel()
    {
        m_document = OpenRocketDocumentFactory::createNewRocket();
        m_panel    = std::make_unique<RocketPanel>(*m_document, preferences(), nullptr);
        m_panel->resize(RocketPanel::kPreferredSize);
        m_panel->show();
        settle();
        return *m_panel;
    }

    /// The dialog over the test's services and design view (if any), on @p tab, not shown.
    PreferencesDialog& makeDialog(Tab tab = Tab::GENERAL)
    {
        m_dialog = std::make_unique<PreferencesDialog>(
            preferences(), prompts(),
            [this] {
                return m_panel == nullptr ? std::vector<RocketPanel*>{}
                                          : std::vector<RocketPanel*>{m_panel.get()};
            },
            tab, nullptr);
        return *m_dialog;
    }

    Gui::Test::TestGuiServices          m_services;
    std::unique_ptr<OpenRocketDocument> m_document;
    std::unique_ptr<RocketPanel>        m_panel;
    std::unique_ptr<PreferencesDialog>  m_dialog;
};

// ==================================================================== PreferencesDialogTest

TEST_F(PreferencesDialogTest, ApplyStoresDefaultUnitsAndRefreshesTheVisibleRocket)
{
    // Port of PreferencesDialogTest.applyStoresDefaultUnitsAndRefreshesTheVisibleRocket: Java
    // verifies the calls on mocks; here their effects are checked.
    RocketPanel& panel = makePanel();
    ASSERT_TRUE(unitGroup(UnitGroupId::LENGTH).setDefaultUnit(std::string_view{"in"}));
    panel.figure().setMinimumSize(QSize(1, 1));
    preferences().setDefaultFlightConfigName("Flight {motors}");
    const QSignalSpy                  extras(&panel.physics(), &DesignPhysics::updated);
    const std::array<RocketPanel*, 1> panels{&panel};

    PreferencesDialog::storeAndApplyPreferences(preferences(), panels);

    // verify(preferences).storeDefaultUnits(): OpenRocket's units node (PreferencesProbe).
    EXPECT_EQ(
        storedUnits(preferences()),
        Gui::Test::transcriptOf(Gui::Test::PreferencesPagePins::kStoredUnitsWithLengthInches));
    // verify(rocketPanel).updateExtras()
    EXPECT_EQ(extras.count(), 1);
    // verify(rocketPanel).updateFigures(): the figure asks for its canvas again.
    EXPECT_EQ(panel.figure().minimumSize(), panel.figure().canvasSize());
    // verify(rocketPanel).updateRulers()
    EXPECT_EQ(panel.scrollArea().rulerUnit().getUnit(), "in");
    // QtRocket's addition: the configuration names (ConfigurationComboBox::refresh()).
    EXPECT_EQ(panel.configurationSelector().itemText(0), QStringLiteral("Flight No motors"));
    UnitGroup::resetDefaultUnits();
}

TEST_F(PreferencesDialogTest, ApplyStoresDefaultUnitsWithoutAnOpenRocketFrame)
{
    // Port of PreferencesDialogTest.applyStoresDefaultUnitsWithoutAnOpenRocketFrame.
    ASSERT_TRUE(unitGroup(UnitGroupId::LENGTH).setDefaultUnit(std::string_view{"in"}));
    PreferencesDialog::storeAndApplyPreferences(preferences(), {});
    // verify(preferences).storeDefaultUnits()
    EXPECT_EQ(
        storedUnits(preferences()),
        Gui::Test::transcriptOf(Gui::Test::PreferencesPagePins::kStoredUnitsWithLengthInches));
    UnitGroup::resetDefaultUnits();
}

// ======================================================================== the dialog itself

TEST_F(PreferencesDialogTest, HasJavasTitleTabsAndButtons)
{
    const PreferencesDialog& dialog = makeDialog();
    EXPECT_EQ(dialog.objectName(), QStringLiteral("preferencesDialog"));
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Preferences"));
    EXPECT_EQ(dialog.windowModality(), Qt::ApplicationModal);

    const QTabWidget& tabs = *dialog.tabs();
    ASSERT_EQ(tabs.count(), 3);
    EXPECT_EQ(tabs.tabText(0), QStringLiteral("General"));
    EXPECT_EQ(tabs.tabToolTip(0), QStringLiteral("Miscellaneous options"));
    EXPECT_EQ(tabs.widget(0), dialog.generalPage());
    EXPECT_EQ(tabs.tabText(1), QStringLiteral("Design"));
    EXPECT_EQ(tabs.tabToolTip(1), QStringLiteral("Design"));
    EXPECT_EQ(tabs.widget(1), dialog.designPage());
    EXPECT_EQ(tabs.tabText(2), QStringLiteral("Units"));
    EXPECT_EQ(tabs.tabToolTip(2), QStringLiteral("Default units"));
    EXPECT_EQ(tabs.widget(2), dialog.unitsPage());
    EXPECT_EQ(tabs.currentIndex(), 0);

    EXPECT_EQ(dialog.cancelButton()->text(), QStringLiteral("Cancel"));
    EXPECT_EQ(dialog.cancelButton()->toolTip(),
              QStringLiteral("Discard changes and close the dialog"));
    EXPECT_EQ(dialog.applyButton()->text(), QStringLiteral("Apply"));
    EXPECT_EQ(dialog.applyButton()->toolTip(),
              QStringLiteral("Apply changes without closing the dialog"));
    EXPECT_EQ(dialog.okButton()->text(), QStringLiteral("OK"));
    EXPECT_EQ(dialog.okButton()->toolTip(), QStringLiteral("Keep changes and close the dialog"));
    EXPECT_TRUE(dialog.okButton()->isDefault());
    EXPECT_FALSE(dialog.isClosed());
}

TEST_F(PreferencesDialogTest, OpensOnTheTabAskedFor)
{
    const PreferencesDialog& dialog = makeDialog(Tab::UNITS);
    EXPECT_EQ(dialog.tabs()->currentWidget(), dialog.unitsPage());
}

TEST_F(PreferencesDialogTest, ApplyStoresRefreshesAndTakesANewSnapshot)
{
    const RocketPanel&       panel  = makePanel();
    const PreferencesDialog& dialog = makeDialog(Tab::UNITS);
    Gui::Test::chooseItem(*dialog.unitsPage()->unitSelector(UnitGroupId::LENGTH), 3);  // in
    dialog.generalPage()->openLastDesign()->click();
    EXPECT_EQ(preferences().findNode(PreferenceKeys::kUnitsNode), nullptr);
    EXPECT_EQ(panel.scrollArea().rulerUnit().getUnit(), "cm");

    dialog.applyButton()->click();
    EXPECT_FALSE(dialog.isClosed());
    EXPECT_EQ(preferences().getNode(PreferenceKeys::kUnitsNode).get("LENGTH"), "in");
    EXPECT_EQ(panel.scrollArea().rulerUnit().getUnit(), "in");
    EXPECT_EQ(dialog.snapshot(), preferences());
    UnitGroup::resetDefaultUnits();
}

TEST_F(PreferencesDialogTest, OkStoresRefreshesAndCloses)
{
    const RocketPanel&       panel  = makePanel();
    const PreferencesDialog& dialog = makeDialog(Tab::UNITS);
    Gui::Test::chooseItem(*dialog.unitsPage()->unitSelector(UnitGroupId::LENGTH), 2);  // m
    dialog.okButton()->click();
    EXPECT_TRUE(dialog.isClosed());
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(preferences().getNode(PreferenceKeys::kUnitsNode).get("LENGTH"), "m");
    EXPECT_EQ(panel.scrollArea().rulerUnit().getUnit(), "m");
    // No question.
    EXPECT_TRUE(prompts().prompts().empty());
    UnitGroup::resetDefaultUnits();
}

TEST_F(PreferencesDialogTest, EscapeAndTheCloseButtonStoreAsOkDoes)
{
    // Java: both dispose the dialog, whose windowClosed listener stores (storePreferences is
    // still true).
    PreferencesDialog& dialog = makeDialog(Tab::UNITS);
    ASSERT_TRUE(Gui::Test::showActive(dialog));
    Gui::Test::chooseItem(*dialog.unitsPage()->unitSelector(UnitGroupId::MASS), 1);  // kg
    QTest::keyClick(&dialog, Qt::Key_Escape);
    EXPECT_TRUE(dialog.isClosed());
    EXPECT_FALSE(dialog.isVisible());
    EXPECT_EQ(preferences().getNode(PreferenceKeys::kUnitsNode).get("MASS"), "kg");
    EXPECT_TRUE(prompts().prompts().empty());

    m_dialog.reset();
    UnitGroup::resetDefaultUnits();
    preferences().reset();
    PreferencesDialog& closed = makeDialog(Tab::UNITS);
    ASSERT_TRUE(Gui::Test::showActive(closed));
    Gui::Test::chooseItem(*closed.unitsPage()->unitSelector(UnitGroupId::MASS), 1);
    EXPECT_TRUE(closed.close());
    EXPECT_TRUE(closed.isClosed());
    EXPECT_EQ(preferences().getNode(PreferenceKeys::kUnitsNode).get("MASS"), "kg");
    UnitGroup::resetDefaultUnits();
}

TEST_F(PreferencesDialogTest, CancelAsksThenRestoresTheSnapshotAndTheUnits)
{
    // A store whose units were applied before (the "units" node holds them).
    preferences().storeDefaultUnits();
    preferences().setShowMarkers(true);
    const InMemoryPreferences before(preferences());
    const RocketPanel&        panel  = makePanel();
    const PreferencesDialog&  dialog = makeDialog(Tab::UNITS);
    Gui::Test::chooseItem(*dialog.unitsPage()->unitSelector(UnitGroupId::LENGTH), 3);  // in
    dialog.generalPage()->openLastDesign()->click();
    dialog.designPage()->showMarkers()->click();
    dialog.applyButton()->click();
    const InMemoryPreferences applied(preferences());
    Gui::Test::chooseItem(*dialog.unitsPage()->unitSelector(UnitGroupId::LENGTH), 5);  // ft
    Gui::Test::chooseItem(*dialog.designPage()->stageInsertPosition(), 2);

    prompts().answerDiscardChanges({.discard = true, .doNotAskAgain = false});
    dialog.cancelButton()->click();
    ASSERT_EQ(prompts().prompts().size(), 1U);
    EXPECT_EQ(prompts().prompts().front().kind, Kind::DISCARD_CHANGES);
    EXPECT_EQ(prompts().prompts().front().texts, QStringList{QStringLiteral("PREFERENCE_CHANGES")});
    EXPECT_TRUE(dialog.isClosed());
    EXPECT_EQ(dialog.result(), QDialog::Rejected);
    // Back to the Apply: the store as it was then and its units in force, the design view
    // refreshed with them.
    // The confirmation's preference is written back as the dialog closes (Java's windowClosed).
    InMemoryPreferences expected(applied);
    expected.setShowDiscardPreferencesConfirmation(true);
    EXPECT_EQ(preferences(), expected);
    EXPECT_NE(preferences(), before);
    EXPECT_EQ(defaultUnit(UnitGroupId::LENGTH), "in");
    EXPECT_EQ(panel.scrollArea().rulerUnit().getUnit(), "in");
    UnitGroup::resetDefaultUnits();
}

TEST_F(PreferencesDialogTest, CancelPutsBackTheUnitsOfAStoreThatHoldsNone)
{
    // The user's decision U4: OpenRocket only loads the snapshot's units, which leaves "ft" in
    // force here, where the store holds no units.
    PreferencesDialog& dialog = makeDialog(Tab::UNITS);
    Gui::Test::chooseItem(*dialog.unitsPage()->unitSelector(UnitGroupId::LENGTH), 5);  // ft
    EXPECT_EQ(defaultUnit(UnitGroupId::LENGTH), "ft");
    preferences().setShowDiscardPreferencesConfirmation(false);
    EXPECT_TRUE(dialog.cancel());
    EXPECT_EQ(defaultUnit(UnitGroupId::LENGTH), "cm");
    EXPECT_EQ(preferences().findNode(PreferenceKeys::kUnitsNode), nullptr);
    EXPECT_TRUE(prompts().prompts().empty());
}

TEST_F(PreferencesDialogTest, CancelAnsweredNoChangesNothing)
{
    PreferencesDialog& dialog = makeDialog();
    dialog.generalPage()->openLastDesign()->click();
    prompts().answerDiscardChanges({.discard = false, .doNotAskAgain = false});
    EXPECT_FALSE(dialog.cancel());
    EXPECT_FALSE(dialog.isClosed());
    EXPECT_TRUE(preferences().isAutoOpenLastDesignOnStartupEnabled());
    EXPECT_TRUE(preferences().isShowDiscardPreferencesConfirmation());
}

TEST_F(PreferencesDialogTest, DontAskAgainTurnsTheQuestionOffWhateverTheAnswer)
{
    PreferencesDialog& dialog = makeDialog();
    dialog.generalPage()->openLastDesign()->click();
    prompts().answerDiscardChanges({.discard = false, .doNotAskAgain = true});
    EXPECT_FALSE(dialog.cancel());
    EXPECT_FALSE(dialog.isClosed());
    EXPECT_FALSE(preferences().isShowDiscardPreferencesConfirmation());
    // The second Cancel asks nothing; the confirmation's preference survives the restore.
    EXPECT_TRUE(dialog.cancel());
    EXPECT_EQ(prompts().prompts().size(), 1U);
    EXPECT_FALSE(preferences().isAutoOpenLastDesignOnStartupEnabled());
    EXPECT_EQ(preferences().get(PreferenceKeys::kShowDiscardPreferencesConfirmation), "false");
}

TEST_F(PreferencesDialogTest, TheConfirmationsBoxSurvivesCancel)
{
    PreferencesDialog& dialog = makeDialog();
    dialog.generalPage()->showDiscardConfirmation()->click();
    dialog.designPage()->showDiscardConfirmation()->click();
    // Unticked, so Cancel asks nothing.
    EXPECT_TRUE(dialog.cancel());
    EXPECT_TRUE(prompts().prompts().empty());
    EXPECT_EQ(preferences().get(PreferenceKeys::kShowDiscardPreferencesConfirmation), "false");
    EXPECT_EQ(preferences().get(PreferenceKeys::kShowDiscardConfirmation), std::nullopt);
}

TEST_F(PreferencesDialogTest, OkWritesTheConfirmationsPreference)
{
    // Java's windowClosed listener writes it back on every close.
    PreferencesDialog& dialog = makeDialog();
    dialog.accept();
    EXPECT_EQ(preferences().get(PreferenceKeys::kShowDiscardPreferencesConfirmation), "true");
}

TEST_F(PreferencesDialogTest, CancelRemovesWhatWasStoredAfterTheSnapshot)
{
    // PreferencesStore's deviation: OpenRocket's import of its snapshot keeps a key it lacks.
    PreferencesDialog& dialog = makeDialog();
    preferences().put("AddedKey", "added");
    preferences().setShowDiscardPreferencesConfirmation(false);
    EXPECT_TRUE(dialog.cancel());
    EXPECT_EQ(preferences().get("AddedKey"), std::nullopt);
}

TEST_F(PreferencesDialogTest, ResetAllPreferencesShowsThePreferencesAnew)
{
    preferences().setAutoOpenLastDesignOnStartup(true);
    preferences().setDefaultMach(0.5);
    const PreferencesDialog&                dialog = makeDialog(Tab::UNITS);
    const QPointer<GeneralPreferencesPanel> oldGeneral(dialog.generalPage());
    ASSERT_TRUE(unitGroup(UnitGroupId::LENGTH).setDefaultUnit(std::string_view{"in"}));

    prompts().answerYesNo(true);
    EXPECT_TRUE(dialog.generalPage()->resetAllPreferences());
    // Closed as by OK (Java's dispose()) and shown anew: new pages on the General tab, which
    // show the reset store, and a new snapshot.
    EXPECT_FALSE(dialog.isClosed());
    EXPECT_NE(dialog.generalPage(), oldGeneral.data());
    EXPECT_EQ(dialog.tabs()->count(), 3);
    EXPECT_EQ(dialog.tabs()->currentWidget(), dialog.generalPage());
    EXPECT_FALSE(dialog.generalPage()->openLastDesign()->isChecked());
    EXPECT_EQ(dialog.designPage()->defaultMachSpinner()->text(), QStringLiteral("0.300"));
    EXPECT_EQ(dialog.unitsPage()->unitSelector(UnitGroupId::LENGTH)->currentText(),
              QStringLiteral("cm"));
    // The store: the factory units, and the confirmation's preference written back on closing.
    EXPECT_EQ(preferences().keys(), std::vector<std::string>{"IgnoreDiscardPreferencesWarning"});
    EXPECT_EQ(preferences().childrenNames(), std::vector<std::string>{"units"});
    EXPECT_EQ(dialog.snapshot(), preferences());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(oldGeneral.isNull());
}

TEST_F(PreferencesDialogTest, APendingEditIsCommittedByOkAndDroppedByCancel)
{
    {
        PreferencesDialog& dialog = makeDialog(Tab::DESIGN);
        ASSERT_TRUE(Gui::Test::showActive(dialog));
        Gui::Test::typeInto(*dialog.designPage()->defaultMachSpinner(), QStringLiteral("0.5"));
        // A click that takes no focus (macOS).
        dialog.okButton()->click();
        EXPECT_EQ(preferences().get(PreferenceKeys::kDefaultMachNumber), "0.5");
    }
    m_dialog.reset();
    preferences().reset();
    {
        PreferencesDialog& dialog = makeDialog(Tab::DESIGN);
        ASSERT_TRUE(Gui::Test::showActive(dialog));
        Gui::Test::typeInto(*dialog.designPage()->defaultMachSpinner(), QStringLiteral("0.6"));
        preferences().setShowDiscardPreferencesConfirmation(false);
        EXPECT_TRUE(dialog.cancel());
        // Hidden, the spin box has lost the focus; its commit stores nothing now.
        EXPECT_FALSE(dialog.isVisible());
        static_cast<void>(dialog.designPage()->defaultMachSpinner()->commitEdit());
        EXPECT_EQ(preferences().get(PreferenceKeys::kDefaultMachNumber), std::nullopt);
    }
}

TEST_F(PreferencesDialogTest, TheMarkersBoxBringsTheFiguresUpToDate)
{
    const RocketPanel&       panel  = makePanel();
    const PreferencesDialog& dialog = makeDialog(Tab::DESIGN);
    panel.figure().setMinimumSize(QSize(1, 1));
    dialog.designPage()->showMarkers()->click();
    EXPECT_TRUE(preferences().isShowMarkers());
    EXPECT_EQ(panel.figure().minimumSize(), panel.figure().canvasSize());
}

TEST_F(PreferencesDialogTest, ShowPreferencesShowsItModalAndDeletesItOnceClosed)
{
    QWidget parent;
    parent.show();
    PreferencesDialog& dialog =
        PreferencesDialog::showPreferences(preferences(), prompts(), {}, parent, Tab::DESIGN);
    const QPointer<PreferencesDialog> shown(&dialog);
    EXPECT_EQ(dialog.parentWidget(), &parent);
    EXPECT_TRUE(dialog.isVisible());
    EXPECT_TRUE(dialog.isModal());
    EXPECT_EQ(dialog.tabs()->currentWidget(), dialog.designPage());
    dialog.okButton()->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(shown.isNull());
}

}  // namespace

}  // namespace QtRocket::Gui
