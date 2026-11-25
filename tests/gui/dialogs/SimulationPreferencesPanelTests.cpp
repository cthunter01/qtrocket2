// The Simulation page of the preferences dialog (OpenRocket's SimulationPreferencesPanel) against
// what the preferences family's probe shows and does on OpenRocket's (PreferencesProbe.java,
// PreferencesPagePins.h: kSimulationPage*): its walk of the page of a cleared store and its steps,
// which these tests take in the same order (a text typed as the probe sets it on a spinner model:
// DoubleBinding::commitText(); a combo box item chosen as its setSelectedItem():
// Test::chooseItem(); a click as doClick(); the seed field's focus loss and prepareForSave() as
// the probe calls them by reflection). Where QtRocket differs on purpose, the pinned line is
// edited here, with the reason: the maxima of the user's decision U-M4-7 and the combo boxes'
// tooltips and the gravity row that follow their bindings after "Reset to default".

#include "QtRocket/gui/dialogs/SimulationPreferencesPanel.h"

#include <cstddef>
#include <format>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QCheckBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFocusEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/dialogs/PreferencesStore.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/FieldValidator.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/simulation/DefaultSimulationOptionFactory.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "dialogs/PreferencesPagePins.h"
#include "dialogs/PreferencesPageTranscript.h"
#include "dialogs/simulation/SimulationDialogWalk.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using SimulationPreferencesPanelTest = Gui::Test::GuiTest;
using Row                            = ConfigFields::NumberWidgets;
namespace Pins                       = Gui::Test::PreferencesPagePins;
namespace Keys                       = PreferenceKeys;
using Gui::Test::expectSameLines;
using Gui::Test::replacedIn;
using Gui::Test::storedDouble;
using Gui::Test::storedText;
using Gui::Test::transcriptOf;
using Gui::Test::without;

/// The page over a store just cleared (the probe's clearPreferences()), its bindings kept by a
/// scope released before the page goes.
class SimulationPage
{
public:
    SimulationPage()
    {
        PreferencesStore::clearPreferences(m_preferences);
        m_page = std::make_unique<SimulationPreferencesPanel>(m_preferences, m_scope, &m_root);
    }
    ~SimulationPage()
    {
        m_scope.release();
        m_page.reset();
    }
    SimulationPage(const SimulationPage&)            = delete;
    SimulationPage& operator=(const SimulationPage&) = delete;
    SimulationPage(SimulationPage&&)                 = delete;
    SimulationPage& operator=(SimulationPage&&)      = delete;

    [[nodiscard]] SimulationPreferencesPanel& page() const noexcept { return *m_page; }
    [[nodiscard]] InMemoryPreferences&        preferences() noexcept { return m_preferences; }

private:
    InMemoryPreferences                         m_preferences;
    QWidget                                     m_root;
    BindingScope                                m_scope{m_root, nullptr};
    std::unique_ptr<SimulationPreferencesPanel> m_page;
};

// ----------------------------------------------------------------------------- the transcript

/// "true" or "false", as Java prints a boolean (and std::format).
[[nodiscard]] std::string javaBool(bool value)
{
    return std::format("{}", value);
}

/// The probe's "[<text>]", " at <slider>", " hidden", " disabled" of a number row (Java's
/// isVisible() is the component's own flag: the page is not shown).
[[nodiscard]] std::string spinnerText(const Row& row)
{
    std::string state = "[" + QtStrings::toUtf8(row.spinBox->text()) + "]";
    if (row.slider != nullptr)
    {
        state += " at " + std::to_string(row.slider->value());
    }
    if (row.spinBox->isHidden())
    {
        state += " hidden";
    }
    if (!row.spinBox->isEnabled())
    {
        state += " disabled";
    }
    return state;
}

/// The probe's "<name>=[<shown item>] tip=<tooltip>" of a combo box.
[[nodiscard]] std::string comboState(std::string_view name, const EnumComboBox& combo)
{
    return std::string(name) + "=[" + QtStrings::toUtf8(combo.shownText()) +
           "] tip=" + QtStrings::toUtf8(combo.toolTip());
}

/// The probe's typeSpinner(): its spinner model's setValue(text).
void enter(const Row& row, std::string_view text)
{
    static_cast<void>(Gui::Test::enterText(*row.spinBox, text));
}

/// The probe's seedState(): the check box, the field (a seed drawn at random as "the stored
/// seed"), its outline, and the stored flag.
[[nodiscard]] std::string seedState(const SimulationPreferencesPanel& page,
                                    const Preferences&                preferences)
{
    const QString shown = page.randomSeedField().text();
    std::string   field;
    if (shown.isEmpty())
    {
        field = "[]";
    }
    else if (shown == QString::number(preferences.getRandomSeed()))
    {
        field = "[the stored seed]";
    }
    else
    {
        field = "[" + QtStrings::toUtf8(shown) + "]";
    }
    std::string state =
        "check=" + javaBool(page.randomSeedCheckBox().isChecked()) + " field=" + field;
    if (!page.randomSeedField().isEnabled())
    {
        state += " disabled";
    }
    if (FieldValidator::outlineOf(page.randomSeedField()) == FieldValidator::Outline::ERROR)
    {
        state += " outline=ERROR";
    }
    return state + " | " + storedText(preferences, Keys::kSimulationRandomSeedFixed, "false");
}

/// The probe's seedIs(): " seed==<expected>:<true|false>".
[[nodiscard]] std::string seedIs(const Preferences& preferences, int expected)
{
    return " seed==" + std::to_string(expected) + ":" +
           javaBool(preferences.getRandomSeed() == expected);
}

/// The item of @p strategy in the geodetic combo box (the strategies in declaration order).
[[nodiscard]] int geodeticIndex(GeodeticComputationStrategy strategy)
{
    // An index loop: an iterator of std::array is a class on MSVC.
    for (std::size_t index = 0; index < kAllGeodeticComputationStrategies.size(); ++index)
    {
        if (kAllGeodeticComputationStrategies.at(index) == strategy)
        {
            return static_cast<int>(index);
        }
    }
    ADD_FAILURE() << "no item of the strategy";
    return 0;
}

/// The probe's "STEPS SIMULATION": the two check boxes, the geodetic computations, the constant
/// gravity.
[[nodiscard]] std::vector<std::string> firstSteps(SimulationPage& page)
{
    const SimulationPreferencesPanel& panel       = page.page();
    const Preferences&                preferences = page.preferences();
    std::vector<std::string>          lines;
    for (QCheckBox* const box : {&panel.confirmDeletionCheckBox(), &panel.autoRunCheckBox()})
    {
        box->click();
        lines.push_back("click '" + QtStrings::toUtf8(box->text()) +
                        "' selected=" + javaBool(box->isChecked()) + " | " +
                        storedText(preferences, Keys::kConfirmDeleteSimulation, "true") + " " +
                        storedText(preferences, Keys::kAutoRunSimulations, "false"));
    }
    for (const GeodeticComputationStrategy strategy :
         {GeodeticComputationStrategy::FLAT, GeodeticComputationStrategy::WGS84,
          GeodeticComputationStrategy::SPHERICAL})
    {
        Gui::Test::chooseItem(panel.geodeticComboBox(), geodeticIndex(strategy));
        lines.push_back("geodetic " + std::string(name(strategy)) + " | " +
                        comboState("geodetic", panel.geodeticComboBox()) + " | " +
                        storedText(preferences, Keys::kGeodeticComputation, "SPHERICAL") +
                        " getter=" + preferences.getGeodeticComputationName());
    }
    Gui::Test::chooseItem(panel.gravityModelComboBox(), 1);
    lines.push_back("gravity CONSTANT | " + comboState("gravity", panel.gravityModelComboBox()) +
                    " | " + storedText(preferences, Keys::kGravityModel, "WGS") +
                    " | gravity value=" + spinnerText(panel.gravityValue()));
    return lines;
}

/// The probe's typed gravity values, the WGS model again, the time steps and maximum times.
[[nodiscard]] std::vector<std::string> numberSteps(SimulationPage& page)
{
    const SimulationPreferencesPanel& panel       = page.page();
    const Preferences&                preferences = page.preferences();
    std::vector<std::string>          lines;
    for (const std::string_view typed : {"-1", "0", "3.72", "30", "150", "9.807"})
    {
        enter(panel.gravityValue(), typed);
        lines.push_back("gravity typed=[" + std::string(typed) +
                        "] | gravity value=" + spinnerText(panel.gravityValue()) + " | " +
                        storedDouble(preferences, Keys::kConstantGravityValue, 9.807));
    }
    Gui::Test::chooseItem(panel.gravityModelComboBox(), 0);
    lines.push_back("gravity WGS | " + comboState("gravity", panel.gravityModelComboBox()) + " | " +
                    storedText(preferences, Keys::kGravityModel, "WGS") +
                    " | gravity value=" + spinnerText(panel.gravityValue()));
    for (const std::string_view typed : {"0.001", "0.01", "0.02", "0.2", "5", "0.05"})
    {
        enter(panel.timeStep(), typed);
        lines.push_back("time step typed=[" + std::string(typed) +
                        "] | time step=" + spinnerText(panel.timeStep()) + " | " +
                        storedDouble(preferences, Keys::kSimulationTimeStep, 0.05));
    }
    for (const std::string_view typed : {"0.5", "1", "600", "5000000", "1200"})
    {
        enter(panel.maximumTime(), typed);
        lines.push_back("maximum time typed=[" + std::string(typed) +
                        "] | maximum time=" + spinnerText(panel.maximumTime()) + " | " +
                        storedDouble(preferences, Keys::kSimulationMaxTime, 1200));
    }
    return lines;
}

/// The seed field's non-temporary focus loss (the probe: replaceInvalidRandomSeed() by
/// reflection).
void loseFocus(QLineEdit& field)
{
    QFocusEvent focusOut(QEvent::FocusOut, Qt::TabFocusReason);
    QCoreApplication::sendEvent(&field, &focusOut);
}

/// The probe's seed steps.
[[nodiscard]] std::vector<std::string> seedSteps(SimulationPage& page)
{
    SimulationPreferencesPanel& panel       = page.page();
    const Preferences&          preferences = page.preferences();
    QCheckBox&                  check       = panel.randomSeedCheckBox();
    QLineEdit&                  field       = panel.randomSeedField();
    std::vector<std::string>    lines;
    lines.push_back("seed initial | " + seedState(panel, preferences));
    check.click();
    lines.push_back("seed checked | " + seedState(panel, preferences));
    for (const char* const typed : {"12345", " -2147483648 ", "2147483648", "abc", ""})
    {
        field.setText(QString::fromLatin1(typed));
        lines.push_back("seed typed=[" + std::string(typed) + "] | " +
                        seedState(panel, preferences) + seedIs(preferences, 12345) +
                        seedIs(preferences, -2147483647 - 1));
    }
    field.setText(QStringLiteral("abc"));
    loseFocus(field);
    lines.push_back("seed focus lost with [abc] | " + seedState(panel, preferences));
    field.setText(QStringLiteral("4711"));
    loseFocus(field);
    lines.push_back("seed focus lost with [4711] | " + seedState(panel, preferences) +
                    seedIs(preferences, 4711));
    field.setText(QStringLiteral("  "));
    panel.prepareForSave();
    lines.push_back("seed prepareForSave with [  ] | " + seedState(panel, preferences));
    check.click();
    field.setText(QStringLiteral(" 99 "));
    panel.prepareForSave();
    lines.push_back("seed prepareForSave with [ 99 ] | " + seedState(panel, preferences) +
                    seedIs(preferences, 99));
    check.click();
    lines.push_back("seed unchecked | " + seedState(panel, preferences) + seedIs(preferences, 99));
    return lines;
}

/// The probe's "Reset to default" after edits of every value.
[[nodiscard]] std::string resetStep(SimulationPage& page)
{
    const SimulationPreferencesPanel& panel       = page.page();
    const Preferences&                preferences = page.preferences();
    enter(panel.timeStep(), "0.04");
    enter(panel.maximumTime(), "300");
    Gui::Test::chooseItem(panel.geodeticComboBox(),
                          geodeticIndex(GeodeticComputationStrategy::WGS84));
    Gui::Test::chooseItem(panel.gravityModelComboBox(), 1);
    enter(panel.gravityValue(), "5");
    panel.randomSeedCheckBox().click();
    panel.randomSeedField().setText(QStringLiteral("31"));
    panel.resetButton().click();
    return "reset | time step=" + spinnerText(panel.timeStep()) +
           " maximum time=" + spinnerText(panel.maximumTime()) + " geodetic=[" +
           QtStrings::toUtf8(panel.geodeticComboBox().shownText()) + "] gravity=[" +
           QtStrings::toUtf8(panel.gravityModelComboBox().shownText()) +
           "] gravity value=" + spinnerText(panel.gravityValue()) + " | " +
           seedState(panel, preferences) + seedIs(preferences, 31) + " | " +
           storedDouble(preferences, Keys::kSimulationTimeStep, 0.05) + " " +
           storedDouble(preferences, Keys::kSimulationMaxTime, 1200) + " " +
           storedText(preferences, Keys::kGeodeticComputation, "SPHERICAL") + " " +
           storedText(preferences, Keys::kGravityModel, "WGS") + " " +
           storedDouble(preferences, Keys::kConstantGravityValue, 9.807) + " " +
           storedText(preferences, Keys::kSimulationRandomSeedFixed, "false");
}

/// The pinned kSimulationPageSteps2 with QtRocket's maxima (the user's decision U-M4-7: the
/// constant gravity at most 100 m/s^2, the maximum time at most 1000000 s, where Java has none)
/// and the gravity row that "Reset to default" hides for WGS (Java's stays shown: its listener
/// does not hear the reset).
[[nodiscard]] std::vector<std::string> expectedSteps2()
{
    std::vector<std::string> lines = transcriptOf(Pins::kSimulationPageSteps2);
    lines                          = replacedIn(
        std::move(lines),
        "gravity typed=[150] | gravity value=[150] at 1000 | ConstantGravityValue=150.0",
        "gravity typed=[150] | gravity value=[100] at 1000 | ConstantGravityValue=100.0", 1);
    lines = replacedIn(std::move(lines),
                       "maximum time typed=[5000000] | maximum time=[5.00E6] | "
                       "SimulationMaxTime=5000000.0",
                       "maximum time typed=[5000000] | maximum time=[1000000] | "
                       "SimulationMaxTime=1000000.0",
                       1);
    return replacedIn(std::move(lines), "gravity=[WGS] gravity value=[9.81] at 490 |",
                      "gravity=[WGS] gravity value=[9.81] at 490 hidden |", 1);
}

/// The pinned walk after "Reset to default" with the tooltips and the gravity row that follow
/// the restored values (see expectedSteps2()).
[[nodiscard]] std::vector<std::string> expectedWalkAfterReset()
{
    std::vector<std::string> lines = transcriptOf(Pins::kSimulationPageWalkAfterReset);
    lines = replacedIn(std::move(lines),
                       "tip=<html>Perform geodetic computations on the WGS84 reference ellipsoid "
                       "using Vincenty's method. <br>Slower and unnecessary in most cases.",
                       "tip=<html>Perform geodetic computations assuming a spherical Earth. "
                       "<br>This is sufficiently accurate for almost all purposes.",
                       1);
    lines = replacedIn(std::move(lines),
                       "COMBO selected=WGS items=WGS|Constant tip=Constant gravity model (same "
                       "value everywhere)",
                       "COMBO selected=WGS items=WGS|Constant tip=WGS84 ellipsoid gravity model "
                       "(varies with latitude and altitude)",
                       1);
    for (const std::string_view hidden :
         {"  LABEL Gravity value: tip=Constant gravitational acceleration value",
          "  SPINNER 9.81 tip=Constant gravitational acceleration value",
          "  LABEL m/s\u00B2 tip=Constant gravitational acceleration value",
          "  SLIDER 0..1000 at 490 tip=Constant gravitational acceleration value"})
    {
        lines = without(std::move(lines), hidden);
    }
    return lines;
}

// ------------------------------------------------------------------------------------ the tests

TEST_F(SimulationPreferencesPanelTest, ShowsWhatOpenRocketsPageShows)
{
    const SimulationPage page;
    expectSameLines(Gui::Test::dialogWalk(page.page()), transcriptOf(Pins::kSimulationPageWalk));
    EXPECT_FALSE(page.page().gravityValue().spinBox->isVisibleTo(&page.page()));
}

TEST_F(SimulationPreferencesPanelTest, DoesWhatOpenRocketsPageDoes)
{
    SimulationPage page;
    expectSameLines(firstSteps(page), transcriptOf(Pins::kSimulationPageSteps));
    expectSameLines(Gui::Test::dialogWalk(page.page()),
                    transcriptOf(Pins::kSimulationPageWalkConstantGravity));
    std::vector<std::string> steps = numberSteps(page);
    for (std::string& line : seedSteps(page))
    {
        steps.push_back(std::move(line));
    }
    steps.push_back(resetStep(page));
    expectSameLines(steps, expectedSteps2());
    expectSameLines(Gui::Test::dialogWalk(page.page()), expectedWalkAfterReset());
}

TEST_F(SimulationPreferencesPanelTest, ResetToDefaultCommitsAPendingEditFirst)
{
    // A click on a button that takes no focus (macOS) leaves the spin box's edit pending; reset
    // commits it, then sets the defaults.
    SimulationPage page;
    QLineEdit&     editor = *Gui::Test::editorOf(*page.page().maximumTime().spinBox);
    editor.setText(QStringLiteral("300"));
    editor.setModified(true);
    page.page().resetButton().click();
    EXPECT_EQ(page.preferences().getMaxSimulationTime(), 1200.0);
    EXPECT_EQ(page.page().maximumTime().spinBox->text(), QStringLiteral("1200"));
}

TEST_F(SimulationPreferencesPanelTest, ShowsTheStoredValues)
{
    InMemoryPreferences preferences;
    preferences.setConfirmSimDeletion(false);
    preferences.setAutoRunSimulations(true);
    preferences.setGeodeticComputationName("WGS84");
    preferences.setGravityModelName("CONSTANT");
    preferences.setConstantGravityValue(3.72);
    preferences.setTimeStep(0.02);
    preferences.setMaxSimulationTime(600);
    preferences.setRandomSeed(4242);
    preferences.setRandomSeedFixed(true);
    QWidget                    root;
    BindingScope               scope(root, nullptr);
    SimulationPreferencesPanel page(preferences, scope, &root);
    EXPECT_FALSE(page.confirmDeletionCheckBox().isChecked());
    EXPECT_TRUE(page.autoRunCheckBox().isChecked());
    EXPECT_EQ(page.geodeticComboBox().shownText(), QStringLiteral("WGS84 ellipsoid"));
    EXPECT_EQ(page.gravityModelComboBox().shownText(), QStringLiteral("Constant"));
    EXPECT_TRUE(page.gravityValue().spinBox->isVisibleTo(&page));
    EXPECT_EQ(page.gravityValue().spinBox->text(), QStringLiteral("3.72"));
    EXPECT_EQ(page.timeStep().spinBox->text(), QStringLiteral("0.02"));
    EXPECT_EQ(page.maximumTime().spinBox->text(), QStringLiteral("600"));
    EXPECT_TRUE(page.randomSeedCheckBox().isChecked());
    EXPECT_TRUE(page.randomSeedField().isEnabled());
    EXPECT_EQ(page.randomSeedField().text(), QStringLiteral("4242"));
    scope.release();
}

TEST_F(SimulationPreferencesPanelTest, NewSimulationsTakeThePagesDefaults)
{
    // OpenRocket bug 38, fixed in the core (DefaultSimulationOptionFactory::getDefault()): the
    // page's geodetic computation reaches a new simulation, as its gravity and times do in Java.
    SimulationPage                    page;
    const SimulationPreferencesPanel& panel = page.page();
    Gui::Test::chooseItem(panel.geodeticComboBox(),
                          geodeticIndex(GeodeticComputationStrategy::WGS84));
    Gui::Test::chooseItem(panel.gravityModelComboBox(), 1);
    enter(panel.gravityValue(), "3.72");
    enter(panel.timeStep(), "0.02");
    enter(panel.maximumTime(), "600");
    const SimulationOptions defaults =
        DefaultSimulationOptionFactory(page.preferences()).getDefault();
    EXPECT_EQ(defaults.getGeodeticComputation(), GeodeticComputationStrategy::WGS84);
    EXPECT_EQ(defaults.getGravityModelType(), GravityModelType::CONSTANT);
    EXPECT_EQ(defaults.getConstantGravity(), 3.72);
    EXPECT_EQ(defaults.getTimeStep(), 0.02);
    EXPECT_EQ(defaults.getMaxSimulationTime(), 600.0);
}

TEST_F(SimulationPreferencesPanelTest, ATemporaryFocusLossKeepsAnInvalidSeed)
{
    // Java's FocusEvent.isTemporary(): another window or a popup took the focus for a while.
    SimulationPage page;
    page.page().randomSeedCheckBox().click();
    QLineEdit& field = page.page().randomSeedField();
    field.setText(QStringLiteral("abc"));
    for (const Qt::FocusReason reason :
         {Qt::ActiveWindowFocusReason, Qt::PopupFocusReason, Qt::MenuBarFocusReason})
    {
        QFocusEvent focusOut(QEvent::FocusOut, reason);
        QCoreApplication::sendEvent(&field, &focusOut);
        EXPECT_EQ(field.text(), QStringLiteral("abc"));
    }
    EXPECT_EQ(FieldValidator::outlineOf(field), FieldValidator::Outline::ERROR);
    loseFocus(field);
    EXPECT_EQ(field.text(), QString::number(page.preferences().getRandomSeed()));
}

}  // namespace

}  // namespace QtRocket::Gui
