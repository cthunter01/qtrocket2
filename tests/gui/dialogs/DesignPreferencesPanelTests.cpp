#include "QtRocket/gui/dialogs/DesignPreferencesPanel.h"

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <QCheckBox>
#include <QSignalSpy>
#include <QString>
#include <QWidget>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/dialogs/PrefChoiceSelector.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "dialogs/PreferencesPageTranscript.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

/// The page in a window of its own (the spin box needs the focus to be typed into) over a store
/// in memory, its binding kept by a scope as the dialog keeps it.
class DesignPreferencesPanelTest : public Gui::Test::GuiTest
{
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        m_window = std::make_unique<QWidget>();
        m_scope  = std::make_unique<BindingScope>(*m_window, nullptr);
        m_page   = makeChild<DesignPreferencesPanel>(m_window.get(), m_preferences, *m_scope);
    }

    void TearDown() override
    {
        // The scope first, as the dialog's goes before its pages; the window with the page.
        m_scope  = nullptr;
        m_page   = nullptr;
        m_window = nullptr;
        GuiTest::TearDown();
    }

    /// Types @p text into the default Mach number's spin box and commits it (commitEdit(), what
    /// Enter and the focus leaving do).
    void typeMach(const QString& text)
    {
        UnitSpinBox& spinner = *m_page->defaultMachSpinner();
        Gui::Test::typeInto(spinner, text);
        static_cast<void>(spinner.commitEdit());
    }

    [[nodiscard]] std::optional<std::string> storedMach() const
    {
        return m_preferences.get(PreferenceKeys::kDefaultMachNumber);
    }

    InMemoryPreferences           m_preferences;
    std::unique_ptr<QWidget>      m_window;
    std::unique_ptr<BindingScope> m_scope;
    DesignPreferencesPanel*       m_page = nullptr;
};

TEST_F(DesignPreferencesPanelTest, TheChoicesStoreTheirIndices)
{
    // PreferencesProbe: each combo box's choices 2, 0, 1 store 2, 0, 1 under its key.
    const std::array<PrefChoiceSelector*, 3> selectors{m_page->bodyComponentInsertPosition(),
                                                       m_page->stageInsertPosition(),
                                                       m_page->rocketInfoFontSize()};
    const std::array<std::string_view, 3>    keys{PreferenceKeys::kBodyComponentInsertPositionKey,
                                                  PreferenceKeys::kStageInsertPositionKey,
                                                  PreferenceKeys::kRocketInfoFontSize};
    for (std::size_t box = 0; box < selectors.size(); ++box)
    {
        EXPECT_EQ(selectors.at(box)->preference(), keys.at(box));
        for (const int index : std::array{2, 0, 1})
        {
            Gui::Test::chooseItem(*selectors.at(box), index);
            EXPECT_EQ(m_preferences.get(keys.at(box)), std::to_string(index));
        }
    }
}

TEST_F(DesignPreferencesPanelTest, TheDefaultMachIsStoredAsTypedWithinItsBounds)
{
    ASSERT_TRUE(Gui::Test::showActive(*m_window));
    const UnitSpinBox& spinner = *m_page->defaultMachSpinner();
    EXPECT_EQ(spinner.objectName(), QStringLiteral("DefaultMach"));
    EXPECT_EQ(spinner.text(), QStringLiteral("0.300"));
    // PreferencesProbe: the text, the stored value.
    typeMach(QStringLiteral("0.5"));
    EXPECT_EQ(spinner.text(), QStringLiteral("0.500"));
    EXPECT_EQ(storedMach(), "0.5");
    typeMach(QStringLiteral("0.95"));
    EXPECT_EQ(spinner.text(), QStringLiteral("0.900"));
    EXPECT_EQ(storedMach(), "0.9");
    typeMach(QStringLiteral("0.05"));
    EXPECT_EQ(spinner.text(), QStringLiteral("0.100"));
    EXPECT_EQ(storedMach(), "0.1");
    typeMach(QStringLiteral("0.3"));
    EXPECT_EQ(spinner.text(), QStringLiteral("0.300"));
    EXPECT_EQ(storedMach(), "0.3");
    // OpenRocket clamps an infinite value to 0.9 (0.900, stored 0.9); the user's decision U4: it
    // sets nothing (DoubleBinding).
    typeMach(QStringLiteral("1/0"));
    EXPECT_EQ(spinner.text(), QStringLiteral("0.300"));
    EXPECT_EQ(storedMach(), "0.3");
    // Text that is no expression sets nothing (OpenRocket sets the last value again).
    typeMach(QStringLiteral("abc"));
    EXPECT_EQ(spinner.text(), QStringLiteral("0.300"));
    EXPECT_EQ(storedMach(), "0.3");
    typeMach(QStringLiteral("0.123456"));
    EXPECT_EQ(spinner.text(), QStringLiteral("0.123"));
    EXPECT_EQ(storedMach(), "0.123456");
    EXPECT_DOUBLE_EQ(m_preferences.getDefaultMach(), 0.123456);
}

TEST_F(DesignPreferencesPanelTest, TheDefaultMachStepsByAThousandthWithinItsBounds)
{
    DoubleBinding& mach = *m_page->defaultMachBinding();
    mach.setValue(0.7);
    EXPECT_EQ(storedMach(), "0.7");
    // PreferencesProbe: 0.701, 0.702, 0.703; nothing below 0.1.
    for (const double next : {0.701, 0.702, 0.703})
    {
        ASSERT_TRUE(mach.stepUp());
        EXPECT_NEAR(m_preferences.getDefaultMach(), next, 1e-12);
    }
    EXPECT_EQ(m_page->defaultMachSpinner()->text(), QStringLiteral("0.703"));
    mach.setValue(0.1);
    EXPECT_EQ(mach.previousValue(), std::nullopt);
}

TEST_F(DesignPreferencesPanelTest, TheCheckBoxesStoreTheirStates)
{
    // PreferencesProbe: each click stores the new state.
    EXPECT_FALSE(m_page->alwaysOpenLeftmostTab()->isChecked());
    m_page->alwaysOpenLeftmostTab()->click();
    EXPECT_EQ(m_preferences.get(PreferenceKeys::kOpenLeftmostDesignTab), "true");
    EXPECT_TRUE(m_page->showDiscardConfirmation()->isChecked());
    m_page->showDiscardConfirmation()->click();
    EXPECT_EQ(m_preferences.get(PreferenceKeys::kShowDiscardConfirmation), "false");

    const QSignalSpy markers(m_page, &DesignPreferencesPanel::showMarkersChanged);
    EXPECT_FALSE(m_page->showMarkers()->isChecked());
    m_page->showMarkers()->click();
    EXPECT_EQ(m_preferences.get(PreferenceKeys::kShowMarkers), "true");
    EXPECT_EQ(markers.count(), 1);
    // Java's ActionListener: a change that is not the user's tells nothing.
    m_page->showMarkers()->setChecked(false);
    EXPECT_EQ(markers.count(), 1);
    EXPECT_EQ(m_preferences.get(PreferenceKeys::kShowMarkers), "true");

    EXPECT_EQ(m_page->alwaysOpenLeftmostTab()->objectName(),
              QStringLiteral("OpenLeftmostDesignTab"));
    EXPECT_EQ(m_page->showDiscardConfirmation()->objectName(),
              QStringLiteral("IgnoreDiscardEditingWarning"));
    EXPECT_EQ(m_page->showMarkers()->objectName(), QStringLiteral("ShowMarkers"));
}

TEST_F(DesignPreferencesPanelTest, AStoredMachOutOfBoundsIsShownAsJavasGetterGivesIt)
{
    // PreferencesProbe: 0.95 and -0.1 are out of getChoice()'s range and show the default 0.300;
    // 0.05 is shown as it is.
    for (const auto& [stored, shown] :
         {std::pair{"0.95", "0.300"}, std::pair{"0.05", "0.050"}, std::pair{"-0.1", "0.300"}})
    {
        InMemoryPreferences preferences;
        preferences.put(PreferenceKeys::kDefaultMachNumber, stored);
        QWidget                      root;
        BindingScope                 scope(root, nullptr);
        const DesignPreferencesPanel page(preferences, scope, &root);
        EXPECT_EQ(page.defaultMachSpinner()->text(), QString::fromLatin1(shown)) << stored;
    }
}

TEST_F(DesignPreferencesPanelTest, NothingIsStoredOnceTheScopeIsReleased)
{
    ASSERT_TRUE(Gui::Test::showActive(*m_window));
    // A pending edit committed late (the focus leaving the spin box of a closing dialog).
    Gui::Test::typeInto(*m_page->defaultMachSpinner(), QStringLiteral("0.6"));
    m_scope->release();
    static_cast<void>(m_page->defaultMachSpinner()->commitEdit());
    EXPECT_EQ(storedMach(), std::nullopt);
}

}  // namespace

}  // namespace QtRocket::Gui
