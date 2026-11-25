// The random seed's controls of the simulation options tab (OpenRocket's SimulationOptionsPanel's
// fixedRandomSeedCheckBox and randomSeedField) over options of their own: the parse of a seed
// (Java's Integer.valueOf() of the trimmed text), the focus losses Java tells apart, and what
// QtRocket adds: the controls follow the options. What the probe
// tools/openrocket-probes/simulation-options pins of them on the tab is
// SimulationOptionsPanelTests.cpp's (and the ports of OpenRocket's SimulationOptionsPanelTest).

#include "QtRocket/gui/dialogs/simulation/RandomSeedControls.h"

#include <algorithm>
#include <array>
#include <climits>
#include <memory>
#include <optional>

#include <QChar>
#include <QCheckBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFocusEvent>
// FieldValidator.h's inline messagePopup() needs the class.
#include <QLabel>  // IWYU pragma: keep
#include <QLineEdit>
#include <QString>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/ObjectSelection.h"
#include "QtRocket/gui/widgets/FieldValidator.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

namespace
{

using RandomSeedControlsTest = Test::GuiTest;
using Seed                   = std::optional<int>;

/// The controls over options of their own, in a widget of their own.
class Controls
{
public:
    Controls() = default;
    ~Controls() { m_selection->release(); }
    Controls(const Controls&)            = delete;
    Controls& operator=(const Controls&) = delete;
    Controls(Controls&&)                 = delete;
    Controls& operator=(Controls&&)      = delete;

    [[nodiscard]] SimulationOptions&  options() noexcept { return m_options; }
    [[nodiscard]] RandomSeedControls& controls() noexcept { return m_controls; }
    [[nodiscard]] ObjectSelection&    selection() noexcept { return *m_selection; }

private:
    SimulationOptions                m_options;
    std::shared_ptr<ObjectSelection> m_selection =
        ObjectSelection::make<SimulationOptions>(m_options);
    QWidget            m_parent;
    RandomSeedControls m_controls{m_selection, &m_parent};
};

/// The field's focus leaving it for @p reason.
void loseFocus(QLineEdit& field, Qt::FocusReason reason)
{
    QFocusEvent event(QEvent::FocusOut, reason);
    QCoreApplication::sendEvent(&field, &event);
}

TEST_F(RandomSeedControlsTest, TheControlsShowTheOptionsSeed)
{
    Controls seed;
    EXPECT_FALSE(seed.controls().checkBox().isChecked());
    EXPECT_FALSE(seed.controls().field().isEnabled());
    EXPECT_EQ(seed.controls().field().text(), QString());
    EXPECT_EQ(seed.controls().checkBox().objectName(), QStringLiteral("fixedRandomSeedCheckBox"));
    EXPECT_EQ(seed.controls().field().objectName(), QStringLiteral("randomSeedField"));
    EXPECT_GE(seed.controls().field().minimumWidth(), RandomSeedControls::kFieldWidth);
    seed.options().setRandomSeed(-5);
    seed.options().setRandomSeedFixed(true);
    EXPECT_TRUE(seed.controls().checkBox().isChecked());
    EXPECT_EQ(seed.controls().field().text(), QStringLiteral("-5"));
    EXPECT_TRUE(seed.controls().field().isEnabled());
}

// Deviation: the controls follow the options (Java's are set once). The launch conditions tab's
// Reset to default takes the seed too (SimulationOptions::copyConditionsFrom()).
TEST_F(RandomSeedControlsTest, TheControlsFollowTheOptions)
{
    Controls          seed;
    SimulationOptions defaults;
    defaults.setRandomSeed(31337);
    defaults.setRandomSeedFixed(true);
    seed.options().copyConditionsFrom(defaults);
    const RandomSeedControls& controls = seed.controls();
    EXPECT_TRUE(controls.checkBox().isChecked());
    EXPECT_EQ(controls.field().text(), QStringLiteral("31337"));
    EXPECT_TRUE(controls.field().isEnabled());
    // A text that is no seed is the user's and stays while the options change elsewhere.
    controls.field().setText(QStringLiteral("x"));
    seed.options().setTimeStep(0.02);
    EXPECT_EQ(controls.field().text(), QStringLiteral("x"));
    EXPECT_EQ(FieldValidator::outlineOf(controls.field()), FieldValidator::Outline::ERROR);
    seed.options().setRandomSeedFixed(false);
    EXPECT_FALSE(controls.checkBox().isChecked());
    EXPECT_EQ(controls.field().text(), QString());
    EXPECT_FALSE(controls.field().isEnabled());
    EXPECT_EQ(FieldValidator::outlineOf(controls.field()), FieldValidator::Outline::NONE);
}

/// Whether @p controls keep their text through the temporary focus losses (another window, a
/// popup, the menu bar).
[[nodiscard]] bool keepsTheTextThroughTemporaryFocusLosses(const RandomSeedControls& controls)
{
    const QString                        text = controls.field().text();
    const std::array<Qt::FocusReason, 3> reasons{Qt::ActiveWindowFocusReason, Qt::PopupFocusReason,
                                                 Qt::MenuBarFocusReason};
    return std::ranges::all_of(reasons, [&controls, &text](Qt::FocusReason reason) {
        loseFocus(controls.field(), reason);
        return controls.field().text() == text;
    });
}

// Java replaces an invalid seed only when the focus leaves for good (FocusEvent.isTemporary():
// not for another window or a popup).
TEST_F(RandomSeedControlsTest, OnlyAFocusLossForGoodReplacesAnInvalidSeed)
{
    Controls                  seed;
    const RandomSeedControls& controls = seed.controls();
    controls.checkBox().click();
    controls.field().setText(QStringLiteral("abc"));
    EXPECT_TRUE(keepsTheTextThroughTemporaryFocusLosses(controls));
    loseFocus(controls.field(), Qt::MouseFocusReason);
    EXPECT_EQ(controls.field().text(), QString::number(seed.options().getRandomSeed()));
    EXPECT_EQ(FieldValidator::outlineOf(controls.field()), FieldValidator::Outline::NONE);
    // A seed is kept as it was typed.
    controls.field().setText(QStringLiteral(" 12 "));
    loseFocus(controls.field(), Qt::TabFocusReason);
    EXPECT_EQ(controls.field().text(), QStringLiteral(" 12 "));
    EXPECT_EQ(seed.options().getRandomSeed(), 12);
}

TEST_F(RandomSeedControlsTest, ParseRandomSeedIsJavasIntegerValueOfATrimmedText)
{
    EXPECT_EQ(RandomSeedControls::parseRandomSeed(QStringLiteral("\t 42\n")), Seed(42));
    EXPECT_EQ(RandomSeedControls::parseRandomSeed(QStringLiteral("+0")), Seed(0));
    EXPECT_EQ(RandomSeedControls::parseRandomSeed(QStringLiteral("2147483647")), Seed(INT_MAX));
    EXPECT_EQ(RandomSeedControls::parseRandomSeed(QStringLiteral("-2147483648")), Seed(INT_MIN));
    EXPECT_EQ(RandomSeedControls::parseRandomSeed(QStringLiteral("-2147483649")), Seed());
    EXPECT_EQ(RandomSeedControls::parseRandomSeed(QStringLiteral("+")), Seed());
    EXPECT_EQ(RandomSeedControls::parseRandomSeed(QStringLiteral("1 2")), Seed());
    EXPECT_EQ(RandomSeedControls::parseRandomSeed(QStringLiteral("--1")), Seed());
    EXPECT_EQ(RandomSeedControls::parseRandomSeed(QStringLiteral("1+")), Seed());
    EXPECT_EQ(RandomSeedControls::parseRandomSeed(QString()), Seed());
    // The fullwidth digits one and two (U+FF11, U+FF12) are decimal digits for Java too; a
    // superscript two (U+00B2) is none (Character.digit() gives -1).
    EXPECT_EQ(RandomSeedControls::parseRandomSeed(QString(QChar(0xFF11)) + QChar(0xFF12)),
              Seed(12));
    EXPECT_EQ(RandomSeedControls::parseRandomSeed(QString(QChar(0x00B2))), Seed());
}

TEST_F(RandomSeedControlsTest, AReleasedSelectionLeavesTheControlsInert)
{
    Controls seed;
    seed.selection().release();
    seed.controls().checkBox().click();
    seed.controls().field().setText(QStringLiteral("5"));
    seed.controls().prepareForSimulation();
    EXPECT_FALSE(seed.options().isRandomSeedFixed());
}

TEST_F(RandomSeedControlsTest, TheControlsNeedAParentAndTheSelectionOfOptions)
{
    SimulationOptions options;
    QWidget           parent;
    const auto        selection = ObjectSelection::make<SimulationOptions>(options);
    EXPECT_THROW(RandomSeedControls(nullptr, &parent), BugError);
    EXPECT_THROW(RandomSeedControls(selection, nullptr), BugError);
    struct Other
    {
        Signal<>                signal;
        [[nodiscard]] Signal<>& changed() noexcept { return signal; }
    };
    Other      other;
    const auto wrong = ObjectSelection::make<Other>(other);
    EXPECT_THROW(RandomSeedControls(wrong, &parent), BugError);
    selection->release();
    wrong->release();
}

}  // namespace

}  // namespace QtRocket::Gui
