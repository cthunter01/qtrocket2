// The "Simulation options" tab (OpenRocket's SimulationOptionsPanel) over the first simulation
// of "A simple model rocket": the port of OpenRocket's SimulationOptionsPanelTest (the seed
// field's applyRandomSeedInput(), here RandomSeedControls'), and what the probe
// tools/openrocket-probes/simulation-options shows and does (SimulationOptionsPins.h): its walk
// of the tab and its steps, which these tests take in the same order, a text typed as the probe
// sets it on a spinner model (DoubleBinding::commitText()), an item chosen as its
// setSelectedItem() (Test::chooseItem()), a click as doClick(). Where QtRocket differs on purpose,
// the pinned line is edited here, with the reason: the maxima of the user's decision U-M4-7, the
// combo boxes that follow the options (the scout's decision D16), the Add extension button of
// Milestone 7 (U-M4-3) and the stability columns' order.

#include "QtRocket/gui/dialogs/simulation/SimulationOptionsPanel.h"

#include <climits>
#include <cstddef>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QCheckBox>
#include <QCoreApplication>
#include <QFocusEvent>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QRect>
#include <QSignalSpy>
#include <QString>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/aero/lookup/CsvMachAoALookup.h"
#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/dialogs/simulation/AerodynamicLookupDialog.h"
#include "QtRocket/gui/dialogs/simulation/RandomSeedControls.h"
#include "QtRocket/gui/dialogs/simulation/SimulationExtensionList.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/FieldValidator.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/Strings.h"
#include "TestGuiServices.h"
#include "TestPaths.h"
#include "dialogs/PreferencesPageTranscript.h"
#include "dialogs/simulation/SimulationDialogWalk.h"
#include "dialogs/simulation/SimulationOptionsPins.h"
#include "file/DesignFileEnvironment.h"
#include "motor/TestMotorDatabase.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using SimulationOptionsPanelTest = Test::GuiTest;
using Row                        = ConfigFields::NumberWidgets;
namespace Pins                   = Test::SimulationOptionsPins;

// ------------------------------------------------------------------------------------ the tab

/// The tab over the first simulation ("Simulation 1") of the example "A simple model rocket"
/// (data/examples), as the probe loads it; the services are the tests' (a fresh preference
/// store, as the probe's), so that "Reset to default" and "Save as default" use their defaults.
/// The window holds the panel; the scope its bindings, released before the window goes.
class OptionsTab
{
public:
    OptionsTab()
    {
        Result<LoadedDocument> loaded =
            GeneralRocketLoader(m_environment.context())
                .load(QtRocket::Test::dataDir() / "examples" / "A simple model rocket.ork");
        if (loaded.has_value())
        {
            m_document   = std::move(loaded->document);
            m_simulation = m_document->getSimulation(0);
        }
        else
        {
            ADD_FAILURE() << loaded.error().message;
            m_document   = OpenRocketDocumentFactory::createEmptyRocket();
            m_simulation = std::make_shared<Simulation>(m_document.get(), m_document->getRocket());
        }
        m_panel = makeChild<SimulationOptionsPanel>(&m_window, m_simulation, m_services.services(),
                                                    m_scope);
    }
    ~OptionsTab() { m_scope.release(); }
    OptionsTab(const OptionsTab&)            = delete;
    OptionsTab& operator=(const OptionsTab&) = delete;
    OptionsTab(OptionsTab&&)                 = delete;
    OptionsTab& operator=(OptionsTab&&)      = delete;

    [[nodiscard]] SimulationOptionsPanel& panel() const noexcept { return *m_panel; }
    [[nodiscard]] SimulationOptions& options() const noexcept { return m_simulation->getOptions(); }
    [[nodiscard]] Simulation&        simulation() const noexcept { return *m_simulation; }
    [[nodiscard]] Test::TestGuiServices& services() noexcept { return m_services; }
    [[nodiscard]] QWidget&               window() noexcept { return m_window; }
    [[nodiscard]] BindingScope&          scope() noexcept { return m_scope; }
    /// The preferences the example's options were read from, which the stepper's setter writes.
    [[nodiscard]] Preferences& designPreferences() noexcept { return m_environment.preferences(); }

private:
    Test::TestGuiServices                 m_services;
    ThrustCurveMotorSetDatabase           m_motors = QtRocket::Test::estesA8Database();
    QtRocket::Test::DesignFileEnvironment m_environment{
        QtRocket::Test::DesignFileEnvironment::Presets::NONE, m_motors};
    std::unique_ptr<OpenRocketDocument> m_document;
    std::shared_ptr<Simulation>         m_simulation;
    QWidget                             m_window;
    BindingScope                        m_scope{m_window, nullptr};
    SimulationOptionsPanel*             m_panel{nullptr};
};

// ----------------------------------------------------------------------------- the transcript

/// "true" or "false", as Java prints a boolean.
[[nodiscard]] std::string javaBool(bool value)
{
    return std::format("{}", value);
}

/// @p text with its line breaks written as "\n" (the probe's clean()).
[[nodiscard]] std::string clean(const QString& text)
{
    QString cleaned = text;
    return QtStrings::toUtf8(cleaned.replace(u'\n', QStringLiteral("\\n")));
}

/// The probe's combo(): "<name>=[<shown item>] tip=<tooltip>".
[[nodiscard]] std::string comboState(std::string_view name, const EnumComboBox& combo)
{
    return std::string(name) + "=[" + QtStrings::toUtf8(combo.shownText()) +
           "] tip=" + clean(combo.toolTip());
}

/// The probe's spinner(): "<name>=[<text>]", " at <slider>", " hidden", " disabled" (Java's
/// isVisible() is the component's own flag: the panel is not shown).
[[nodiscard]] std::string spinnerState(std::string_view name, const Row& row)
{
    std::string state = std::string(name) + "=[" + QtStrings::toUtf8(row.spinBox->text()) + "]";
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

/// The probe's typeSpinner(): its spinner model's setValue(text).
void enter(const Row& row, std::string_view text)
{
    static_cast<void>(row.spinBox->binding()->commitText(text));
}

/// The probe's seed(): the check box, the field (a seed drawn at random as "the options' seed"),
/// its outline and tooltip, and whether the options' seed is fixed.
[[nodiscard]] std::string seedState(const RandomSeedControls& controls,
                                    const SimulationOptions&  options)
{
    const QString shown = controls.field().text();
    std::string   field;
    if (shown.isEmpty())
    {
        field = "[]";
    }
    else if (shown == QString::number(options.getRandomSeed()))
    {
        field = "[the options' seed]";
    }
    else
    {
        field = "[" + QtStrings::toUtf8(shown) + "]";
    }
    std::string state = "check=" + javaBool(controls.checkBox().isChecked()) + " field=" + field;
    if (!controls.field().isEnabled())
    {
        state += " disabled";
    }
    if (FieldValidator::outlineOf(controls.field()) == FieldValidator::Outline::ERROR)
    {
        state += " outline=ERROR";
    }
    return state + " tip=" + clean(controls.field().toolTip()) +
           " | options fixed=" + javaBool(options.isRandomSeedFixed());
}

/// The probe's seedValue(): " seed==<expected>:<true|false>".
[[nodiscard]] std::string seedIs(const SimulationOptions& options, int expected)
{
    return " seed==" + std::to_string(expected) + ":" +
           javaBool(options.getRandomSeed() == expected);
}

/// Java's Double.toString().
[[nodiscard]] std::string javaDouble(double value)
{
    return Strings::javaDoubleToString(value);
}

/// The walk of @p widget, each line with the probe's prefix "WALK ".
void appendWalk(std::vector<std::string>& lines, const QWidget& widget)
{
    for (const std::string& line : Gui::Test::dialogWalk(widget))
    {
        lines.push_back("WALK " + line);
    }
}

/// The pinned lines of @p section without its MODEL and BOX lines (pinned apart).
[[nodiscard]] std::vector<std::string> pinnedSteps(std::span<const std::string_view> section)
{
    std::vector<std::string> lines;
    for (const std::string_view line : section)
    {
        if (!line.starts_with("MODEL ") && !line.starts_with("BOX "))
        {
            lines.emplace_back(line);
        }
    }
    return lines;
}

// --------------------------------------------------------------------------------- the steps

/// The probe's "the stepper": RK6, then RK4, with the preference the options' setter writes.
[[nodiscard]] std::vector<std::string> stepperSteps(OptionsTab& tab)
{
    std::vector<std::string> lines;
    for (const SimulationStepperMethod method :
         {SimulationStepperMethod::RK6, SimulationStepperMethod::RK4})
    {
        Gui::Test::chooseItem(tab.panel().stepperComboBox(),
                              method == SimulationStepperMethod::RK6 ? 1 : 0);
        lines.push_back(
            "STEP stepper " + std::string(simulationStepperMethodName(method)) + " | " +
            comboState("stepper", tab.panel().stepperComboBox()) + " | options=" +
            std::string(
                simulationStepperMethodName(tab.options().getSimulationStepperMethodChoice())) +
            " preference=" +
            tab.designPreferences().getString(PreferenceKeys::kSimulationStepperMethod, "(none)"));
    }
    return lines;
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

/// The probe's "the geodetic computation": Spherical, WGS84, then Flat Earth.
[[nodiscard]] std::vector<std::string> geodeticSteps(const OptionsTab& tab)
{
    std::vector<std::string> lines;
    for (const GeodeticComputationStrategy strategy :
         {GeodeticComputationStrategy::SPHERICAL, GeodeticComputationStrategy::WGS84,
          GeodeticComputationStrategy::FLAT})
    {
        Gui::Test::chooseItem(tab.panel().geodeticComboBox(), geodeticIndex(strategy));
        lines.push_back("STEP geodetic " + std::string(name(strategy)) + " | " +
                        comboState("geodetic", tab.panel().geodeticComboBox()) +
                        " | options=" + std::string(name(tab.options().getGeodeticComputation())));
    }
    return lines;
}

/// The probe's gravity line.
[[nodiscard]] std::string gravityLine(const OptionsTab& tab, std::string_view step)
{
    const SimulationOptionsPanel& panel = tab.panel();
    return "STEP gravity " + std::string(step) + " | " +
           comboState("gravity", panel.gravityModelComboBox()) +
           " | options=" + std::string(gravityModelTypeName(tab.options().getGravityModelType())) +
           " | label visible=" + javaBool(!panel.gravityValue().label->isHidden()) + " " +
           spinnerState("gravity", panel.gravityValue());
}

/// The probe's "the gravity model": Constant, its walk, typed values, then WGS.
[[nodiscard]] std::vector<std::string> gravitySteps(const OptionsTab& tab)
{
    std::vector<std::string>      lines;
    const SimulationOptionsPanel& panel = tab.panel();
    Gui::Test::chooseItem(panel.gravityModelComboBox(), 1);
    lines.push_back(gravityLine(tab, "CONSTANT"));
    lines.emplace_back("== tab with the constant gravity");
    appendWalk(lines, panel);
    for (const std::string_view text : {"-1", "0", "3.72", "30", "150"})
    {
        enter(panel.gravityValue(), text);
        lines.push_back("STEP gravity typed=[" + std::string(text) + "] | " +
                        spinnerState("gravity", panel.gravityValue()) +
                        " | options=" + javaDouble(tab.options().getConstantGravity()));
    }
    Gui::Test::chooseItem(panel.gravityModelComboBox(), 0);
    lines.push_back(gravityLine(tab, "WGS"));
    return lines;
}

/// The probe's "the time step and the maximum time".
[[nodiscard]] std::vector<std::string> timeSteps(const OptionsTab& tab)
{
    std::vector<std::string>      lines;
    const SimulationOptionsPanel& panel = tab.panel();
    for (const std::string_view text : {"0.001", "0.01", "0.02", "0.2", "5", "0.05"})
    {
        enter(panel.timeStep(), text);
        lines.push_back("STEP time step typed=[" + std::string(text) + "] | " +
                        spinnerState("time step", panel.timeStep()) +
                        " | options=" + javaDouble(tab.options().getTimeStep()));
    }
    for (const std::string_view text : {"0.5", "1", "5000000", "1200"})
    {
        enter(panel.maximumTime(), text);
        lines.push_back("STEP maximum time typed=[" + std::string(text) + "] | " +
                        spinnerState("maximum time", panel.maximumTime()) +
                        " | options=" + javaDouble(tab.options().getMaxSimulationTime()));
    }
    return lines;
}

/// The table of the CSV @p rows with the value columns @p columns, which must parse.
[[nodiscard]] std::shared_ptr<const MachAoALookup> tableOf(const std::vector<std::string>& rows,
                                                           const std::vector<std::string>& columns)
{
    Result<MachAoALookup> table = CsvMachAoALookup::parse(rows, columns, ',');
    EXPECT_TRUE(table.has_value());
    return table.has_value() ? std::make_shared<const MachAoALookup>(std::move(*table)) : nullptr;
}

/// The probe's "the lookup summary": a drag table, a stability table, then none.
[[nodiscard]] std::vector<std::string> lookupSummarySteps(const OptionsTab& tab)
{
    std::vector<std::string>       lines;
    SimulationOptions&             options = tab.options();
    const QLabel&                  icon    = tab.panel().lookupSummaryIcon();
    const std::vector<std::string> drag{"Mach,Cd", "0,0.5", "0.3333333,0.55", "2.5,0.6"};
    options.setDragLookup(pathFromUtf8("/data/aero/drag & <lift>.csv"), tableOf(drag, {"cd"}),
                          drag);
    lines.push_back("STEP drag table | tip=" + clean(icon.toolTip()));
    const std::vector<std::string> stability{
        "Mach,AoA,Cn,Cm,Cp",        "0.1,-0.0004,1,1,1", "1.25,10,2,2,2",
        "1234567.891,10,3,3,3",     "0.1,10,1,1,1",      "1.25,-0.0004,2,2,2",
        "1234567.891,-0.0004,3,3,3"};
    options.setStabilityLookup(pathFromUtf8("stability.csv"),
                               tableOf(stability, {"cn", "cm", "cp"}), stability);
    lines.push_back("STEP stability table | tip=" + clean(icon.toolTip()));
    options.clearDragLookup();
    options.clearStabilityLookup();
    lines.push_back("STEP no tables | tip=" + clean(icon.toolTip()));
    return lines;
}

/// The field's focus leaving it for good (a Tab), as the probe's replaceInvalidRandomSeed().
void focusOut(QLineEdit& field)
{
    QFocusEvent event(QEvent::FocusOut, Qt::TabFocusReason);
    QCoreApplication::sendEvent(&field, &event);
}

/// The probe's "the random seed".
[[nodiscard]] std::vector<std::string> seedSteps(const OptionsTab& tab)
{
    std::vector<std::string>  lines;
    const RandomSeedControls& seed    = tab.panel().randomSeed();
    const SimulationOptions&  options = tab.options();
    lines.push_back("STEP initial | " + seedState(seed, options));
    seed.checkBox().click();
    lines.push_back("STEP checked | " + seedState(seed, options));
    for (const std::string_view text : {"12345", " -2147483648 ", "2147483648", "abc", ""})
    {
        seed.field().setText(QtStrings::fromUtf8(text));
        lines.push_back("STEP typed=[" + std::string(text) + "] | " + seedState(seed, options) +
                        seedIs(options, 12345) + seedIs(options, INT_MIN));
    }
    seed.field().setText(QStringLiteral("abc"));
    focusOut(seed.field());
    lines.push_back("STEP focus lost with [abc] | " + seedState(seed, options));
    // The Arabic-Indic digits one, two and three (U+0661...U+0663), which Java's
    // Character.digit() reads as 123.
    for (const QString& text :
         {QStringLiteral("+77"), QString(QChar(0x0661)) + QChar(0x0662) + QChar(0x0663),
          QStringLiteral("0x10"), QStringLiteral("1.0"), QStringLiteral("-0")})
    {
        seed.field().setText(text);
        lines.push_back("STEP typed=[" + QtStrings::toUtf8(text) + "] | " +
                        seedState(seed, options) + seedIs(options, 77) + seedIs(options, 123) +
                        seedIs(options, 0));
    }
    seed.field().setText(QStringLiteral("4711"));
    focusOut(seed.field());
    lines.push_back("STEP focus lost with [4711] | " + seedState(seed, options) +
                    seedIs(options, 4711));
    seed.field().setText(QStringLiteral("  "));
    tab.panel().prepareForSimulation();
    lines.push_back("STEP prepareForSimulation with [  ] | " + seedState(seed, options));
    seed.checkBox().click();
    seed.field().setText(QStringLiteral(" 99 "));
    tab.panel().prepareForSimulation();
    lines.push_back("STEP prepareForSimulation with [ 99 ] | " + seedState(seed, options) +
                    seedIs(options, 99));
    seed.checkBox().click();
    lines.push_back("STEP unchecked | " + seedState(seed, options) + seedIs(options, 99));
    return lines;
}

/// The options' time step, maximum time and geodetic computation as the probe's reset lines
/// print them.
[[nodiscard]] std::string resetOptions(const SimulationOptions& options)
{
    return "time step=" + javaDouble(options.getTimeStep()) +
           " max time=" + javaDouble(options.getMaxSimulationTime()) +
           " geodetic=" + std::string(name(options.getGeodeticComputation()));
}

/// The probe's "reset to default".
[[nodiscard]] std::vector<std::string> resetSteps(OptionsTab& tab)
{
    std::vector<std::string>      lines;
    const SimulationOptionsPanel& panel   = tab.panel();
    const SimulationOptions&      options = tab.options();
    Gui::Test::chooseItem(panel.stepperComboBox(), 1);
    Gui::Test::chooseItem(panel.gravityModelComboBox(), 1);
    enter(panel.gravityValue(), "5");
    enter(panel.timeStep(), "0.04");
    enter(panel.maximumTime(), "300");
    panel.resetButton().click();
    lines.push_back(
        "STEP reset, fresh preferences | " + resetOptions(options) + " stepper=" +
        std::string(simulationStepperMethodName(options.getSimulationStepperMethodChoice())) +
        " gravity=" + std::string(gravityModelTypeName(options.getGravityModelType())) + " " +
        javaDouble(options.getConstantGravity()) + " | " +
        spinnerState("time step", panel.timeStep()) + " " +
        spinnerState("maximum time", panel.maximumTime()) + " | " +
        comboState("geodetic", panel.geodeticComboBox()) + " | " +
        seedState(panel.randomSeed(), options));
    Preferences& preferences = tab.services().preferences();
    preferences.setTimeStep(0.02);
    preferences.setMaxSimulationTime(600);
    preferences.setGeodeticComputationName("WGS84");
    preferences.setRandomSeed(777);
    preferences.setRandomSeedFixed(true);
    panel.resetButton().click();
    lines.push_back("STEP reset, stored preferences | " + resetOptions(options) + " | " +
                    spinnerState("time step", panel.timeStep()) + " " +
                    spinnerState("maximum time", panel.maximumTime()) + " | " +
                    comboState("geodetic", panel.geodeticComboBox()) + " | " +
                    seedState(panel.randomSeed(), options) + seedIs(options, 777));
    preferences.setRandomSeedFixed(false);
    panel.resetButton().click();
    lines.push_back("STEP reset, preferences without a fixed seed | " +
                    seedState(panel.randomSeed(), options));
    return lines;
}

/// The probe's "save as default".
[[nodiscard]] std::vector<std::string> saveSteps(OptionsTab& tab)
{
    std::vector<std::string>      lines;
    const SimulationOptionsPanel& panel       = tab.panel();
    const SimulationOptions&      options     = tab.options();
    const Preferences&            preferences = tab.services().preferences();
    const RandomSeedControls&     seed        = panel.randomSeed();
    enter(panel.timeStep(), "0.03");
    enter(panel.maximumTime(), "900");
    Gui::Test::chooseItem(panel.geodeticComboBox(),
                          geodeticIndex(GeodeticComputationStrategy::FLAT));
    seed.checkBox().click();
    seed.field().setText(QStringLiteral("4242"));
    panel.saveButton().click();
    lines.push_back(
        "STEP save with [4242] | preferences time step=" + javaDouble(preferences.getTimeStep()) +
        " max time=" + javaDouble(preferences.getMaxSimulationTime()) +
        " geodetic=" + preferences.getGeodeticComputationName() +
        " fixed=" + javaBool(preferences.isRandomSeedFixed()) +
        " seed=" + std::to_string(preferences.getRandomSeed()) + " | " + seedState(seed, options));
    seed.checkBox().click();
    seed.checkBox().click();
    seed.field().setText(QStringLiteral("x"));
    panel.saveButton().click();
    lines.push_back(
        "STEP save with [x] | preferences fixed=" + javaBool(preferences.isRandomSeedFixed()) +
        " seed=" + std::to_string(preferences.getRandomSeed()) + " | " + seedState(seed, options));
    return lines;
}

// ---------------------------------------------------------------------------- the expectations

/// The probe's walk of the tab: Java's "Add extension" comes with Milestone 7 (U-M4-3).
[[nodiscard]] std::vector<std::string> tabWalkExpectation()
{
    return Gui::Test::without(Gui::Test::linesWithPrefix(Pins::kTab, "WALK "),
                              "  BUTTON Add extension");
}

/// The probe's gravity steps: 150 m/s^2 is beyond SimulationInputRanges' 100 (U-M4-7; Java has
/// no maximum); the walk with the constant gravity lacks "Add extension" (U-M4-3).
[[nodiscard]] std::vector<std::string> gravityExpectation()
{
    std::vector<std::string> lines =
        Gui::Test::without(pinnedSteps(Pins::kGravity), "WALK   BUTTON Add extension");
    lines = Gui::Test::replacedIn(std::move(lines),
                                  "typed=[150] | gravity=[150] at 1000 | options=150.0",
                                  "typed=[150] | gravity=[100] at 1000 | options=100.0", 1);
    return Gui::Test::replacedIn(std::move(lines), "label visible=false gravity=[150]",
                                 "label visible=false gravity=[100]", 1);
}

/// The probe's time steps: 5000000 s is beyond SimulationInputRanges' 1000000 (U-M4-7; Java
/// has no maximum).
[[nodiscard]] std::vector<std::string> timesExpectation()
{
    return Gui::Test::replacedIn(pinnedSteps(Pins::kTimes),
                                 "maximum time=[5.00E6] | options=5000000.0",
                                 "maximum time=[1000000] | options=1000000.0", 1);
}

/// The probe's lookup summaries: the stability table's columns in their order in the table
/// (Java's order changes from run to run, "{CN, CM, CP}" in the probe).
[[nodiscard]] std::vector<std::string> lookupSummaryExpectation()
{
    return Gui::Test::replacedIn(pinnedSteps(Pins::kLookupSummary), "columns: {CN, CM, CP}",
                                 "columns: CN, CM, CP", 1);
}

/// The probe's resets: QtRocket's geodetic combo box shows the computation the reset restored
/// (D16: the binding follows the options); Java's still shows the last item chosen, Flat Earth.
[[nodiscard]] std::vector<std::string> resetExpectation()
{
    const std::string_view flat =
        "geodetic=[Flat Earth] tip=Perform computations with a flat Earth approximation.  "
        "Sufficient for low-altitude flights.";
    std::vector<std::string> lines = Gui::Test::replacedIn(
        pinnedSteps(Pins::kResetToDefault),
        std::string("max time=1200.0 geodetic=SPHERICAL") +
            " stepper=RK6 gravity=CONSTANT 5.0 | time step=[0.05] at 210 maximum time=[1200] | " +
            std::string(flat),
        std::string("max time=1200.0 geodetic=SPHERICAL") +
            " stepper=RK6 gravity=CONSTANT 5.0 | time step=[0.05] at 210 maximum time=[1200] | " +
            "geodetic=[Spherical approximation] tip=<html>Perform geodetic computations assuming "
            "a spherical Earth. <br>This is sufficiently accurate for almost all purposes.",
        1);
    return Gui::Test::replacedIn(
        std::move(lines), std::string("maximum time=[600] | ") + std::string(flat),
        "maximum time=[600] | geodetic=[WGS84 ellipsoid] tip=<html>Perform geodetic "
        "computations on the WGS84 reference ellipsoid using Vincenty's method. <br>Slower and "
        "unnecessary in most cases.",
        1);
}

/// The probe's saves: Java's geodetic combo box still showed Flat Earth after the reset to
/// WGS84 (above), so choosing Flat Earth there changed nothing and WGS84 was saved; QtRocket's
/// shows WGS84, and Flat Earth chosen is saved (D16).
[[nodiscard]] std::vector<std::string> saveExpectation()
{
    return Gui::Test::replacedIn(pinnedSteps(Pins::kSaveAsDefault), "geodetic=WGS84 fixed=true",
                                 "geodetic=FLAT fixed=true", 1);
}

// ------------------------------------------------------------------------------------ geometry

/// @p widget's rectangle in @p panel's coordinates.
[[nodiscard]] QRect rectIn(const QWidget& widget, const QWidget& panel)
{
    return {widget.mapTo(&panel, QPoint(0, 0)), widget.size()};
}

/// A relation of the layout and whether it holds.
struct Relation
{
    std::string_view name;
    bool             holds;
};

/// The names of the relations of @p relations that do not hold, each followed by "; ".
[[nodiscard]] std::string unmet(std::initializer_list<Relation> relations)
{
    std::string names;
    for (const Relation& relation : relations)
    {
        if (!relation.holds)
        {
            names += std::string(relation.name) + "; ";
        }
    }
    return names;
}

/// The probe's BOX lines as relations: the options' box left of the extensions' box, both at
/// the top ("BORDER Simulator options x=6 y=6", "BORDER Simulation extensions x=451 y=6"); the
/// labels in one column on the left (x=17), the combo boxes, the Configure button, the spin
/// boxes and the seed's field in the next (x=223), the help icon right of Configure (x=340), a
/// unit right of its spin box and the slider right of the unit ("SPINNER 0.05 x=223", "LABEL s
/// x=295", "SLIDER x=334"), the rows from top to bottom in Java's order, and Reset left of Save
/// below them ("BUTTON Reset to default x=17 y=328", "Save as default x=158 y=328").
[[nodiscard]] std::string unmetTabRelations(const SimulationOptionsPanel& panel)
{
    const QRect options    = rectIn(panel.simulatorOptionsBox(), panel);
    const QRect extensions = rectIn(panel.extensionsBox(), panel);
    const QRect method     = rectIn(panel.calculationMethodLabel(), panel);
    const QRect stepper    = rectIn(panel.stepperComboBox(), panel);
    const QRect configure  = rectIn(panel.configureLookupButton(), panel);
    const QRect icon       = rectIn(panel.lookupSummaryIcon(), panel);
    const QRect geodetic   = rectIn(panel.geodeticComboBox(), panel);
    const QRect gravity    = rectIn(panel.gravityModelComboBox(), panel);
    const QRect step       = rectIn(*panel.timeStep().spinBox, panel);
    const QRect stepUnit   = rectIn(*panel.timeStep().unitSelector, panel);
    const QRect stepSlider = rectIn(*panel.timeStep().slider, panel);
    const QRect maximum    = rectIn(*panel.maximumTime().spinBox, panel);
    const QRect seed       = rectIn(panel.randomSeed().field(), panel);
    const QRect check      = rectIn(panel.randomSeed().checkBox(), panel);
    const QRect reset      = rectIn(panel.resetButton(), panel);
    const QRect save       = rectIn(panel.saveButton(), panel);
    return unmet({
        {.name  = "the extensions right of the options",
         .holds = extensions.left() > options.right()},
        {.name = "the boxes level", .holds = extensions.top() == options.top()},
        {.name  = "the fields in one column",
         .holds = stepper.left() == configure.left() && stepper.left() == geodetic.left() &&
                  stepper.left() == gravity.left() && stepper.left() == step.left() &&
                  stepper.left() == maximum.left() && stepper.left() == seed.left()},
        {.name = "the labels left of the fields", .holds = method.right() < stepper.left()},
        {.name = "the labels in one column", .holds = check.left() == method.left()},
        {.name = "the icon right of Configure", .holds = icon.left() > configure.right()},
        {.name = "the unit right of the spin box", .holds = stepUnit.left() > step.right()},
        {.name = "the slider right of the unit", .holds = stepSlider.left() > stepUnit.right()},
        {.name  = "the rows from top to bottom",
         .holds = method.bottom() < stepper.top() && stepper.bottom() < configure.top() &&
                  configure.bottom() < geodetic.top() && geodetic.bottom() < gravity.top() &&
                  gravity.bottom() < step.top() && step.bottom() < maximum.top() &&
                  maximum.bottom() < seed.top()},
        {.name = "the buttons below the grid", .holds = reset.top() > seed.bottom()},
        {.name = "the buttons on one line", .holds = reset.top() == save.top()},
        {.name = "Reset left of Save", .holds = reset.right() < save.left()},
        {.name = "the buttons on the left", .holds = reset.left() == method.left()},
    });
}

// -------------------------------------------------------------------------------------- tests

/// SimulationOptionsPanelTest.fixedOptions(): new options with the fixed seed 12345.
[[nodiscard]] std::unique_ptr<SimulationOptions> fixedOptions()
{
    auto options = std::make_unique<SimulationOptions>();
    options->setRandomSeed(12345);
    options->setRandomSeedFixed(true);
    return options;
}

// Port of SimulationOptionsPanelTest.testValidSignedIntegerEnablesFixedSeed (Java's
// SimulationOptionsPanel.applyRandomSeedInput() is RandomSeedControls'.)
TEST_F(SimulationOptionsPanelTest, TestValidSignedIntegerEnablesFixedSeed)
{
    SimulationOptions options;

    const bool fixed =
        RandomSeedControls::applyRandomSeedInput(options, true, QStringLiteral(" -2147483648 "));

    EXPECT_TRUE(fixed);
    EXPECT_TRUE(options.isRandomSeedFixed());
    EXPECT_EQ(INT_MIN, options.getRandomSeed());
}

// Port of SimulationOptionsPanelTest.testBlankSeedFallsBackToRandomRuns
TEST_F(SimulationOptionsPanelTest, TestBlankSeedFallsBackToRandomRuns)
{
    const std::unique_ptr<SimulationOptions> options = fixedOptions();

    const bool fixed =
        RandomSeedControls::applyRandomSeedInput(*options, true, QStringLiteral("  "));

    EXPECT_FALSE(fixed);
    EXPECT_FALSE(options->isRandomSeedFixed());
}

// Port of SimulationOptionsPanelTest.testOutOfRangeSeedFallsBackToRandomRuns
TEST_F(SimulationOptionsPanelTest, TestOutOfRangeSeedFallsBackToRandomRuns)
{
    const std::unique_ptr<SimulationOptions> options = fixedOptions();

    const bool fixed =
        RandomSeedControls::applyRandomSeedInput(*options, true, QStringLiteral("2147483648"));

    EXPECT_FALSE(fixed);
    EXPECT_FALSE(options->isRandomSeedFixed());
}

// Port of SimulationOptionsPanelTest.testUncheckedSeedUsesRandomRuns
TEST_F(SimulationOptionsPanelTest, TestUncheckedSeedUsesRandomRuns)
{
    const std::unique_ptr<SimulationOptions> options = fixedOptions();

    const bool fixed =
        RandomSeedControls::applyRandomSeedInput(*options, false, QStringLiteral("12345"));

    EXPECT_FALSE(fixed);
    EXPECT_FALSE(options->isRandomSeedFixed());
}

TEST_F(SimulationOptionsPanelTest, TheTabShowsWhatOpenRocketsTabShows)
{
    const OptionsTab tab;
    EXPECT_EQ(Gui::Test::dialogWalk(tab.panel()), tabWalkExpectation());
}

TEST_F(SimulationOptionsPanelTest, TheFieldsHaveJavasUnitsBoundsAndSliders)
{
    const OptionsTab              tab;
    const SimulationOptionsPanel& panel = tab.panel();
    // "MODEL TimeStep ... units=UNITS_TIME_STEP min=0.01 max=1.0 ... slider=linear min=0.01
    // mid=0.2 max=0.2 linearPosition=1.0".
    const DoubleBinding& step = *panel.timeStep().spinBox->binding();
    EXPECT_EQ(&step.getUnitGroup(), &unitGroup(UnitGroupId::TIME_STEP));
    EXPECT_EQ(step.minimum(), 0.01);
    EXPECT_EQ(step.maximum(), 1.0);
    EXPECT_TRUE(panel.timeStep().slider->mapping().isLinear());
    EXPECT_EQ(panel.timeStep().slider->mapping().getMin(), 0.01);
    EXPECT_EQ(panel.timeStep().slider->mapping().getMax(), 0.2);
    // "MODEL MaxSimulationTime ... units=UNITS_LONG_TIME min=1.0 max=Infinity ... slider=none"
    // (U-M4-7: SimulationInputRanges' 1000000 s).
    const DoubleBinding& maximum = *panel.maximumTime().spinBox->binding();
    EXPECT_EQ(&maximum.getUnitGroup(), &unitGroup(UnitGroupId::LONG_TIME));
    EXPECT_EQ(maximum.minimum(), 1.0);
    EXPECT_EQ(maximum.maximum(), 1000000.0);
    EXPECT_EQ(panel.maximumTime().slider, nullptr);
    // "MODEL ConstantGravity ... units=UNITS_ACCELERATION min=0.0 max=Infinity ...
    // slider=linear min=0.0 mid=20.0 max=20.0" (U-M4-7: 100 m/s^2).
    const DoubleBinding& gravity = *panel.gravityValue().spinBox->binding();
    EXPECT_EQ(&gravity.getUnitGroup(), &unitGroup(UnitGroupId::ACCELERATION));
    EXPECT_EQ(gravity.minimum(), 0.0);
    EXPECT_EQ(gravity.maximum(), 100.0);
    EXPECT_EQ(panel.gravityValue().slider->mapping().getMin(), 0.0);
    EXPECT_EQ(panel.gravityValue().slider->mapping().getMax(), 20.0);
    // The widgets carry the Java properties' names.
    EXPECT_EQ(panel.timeStep().spinBox->objectName(), QStringLiteral("TimeStep"));
    EXPECT_EQ(panel.maximumTime().spinBox->objectName(), QStringLiteral("MaxSimulationTime"));
    EXPECT_EQ(panel.gravityValue().spinBox->objectName(), QStringLiteral("ConstantGravity"));
    EXPECT_EQ(panel.stepperComboBox().objectName(),
              QStringLiteral("SimulationStepperMethodChoice"));
    EXPECT_EQ(panel.geodeticComboBox().objectName(), QStringLiteral("GeodeticComputation"));
    EXPECT_EQ(panel.gravityModelComboBox().objectName(), QStringLiteral("GravityModelType"));
}

TEST_F(SimulationOptionsPanelTest, TheBoxesAndRowsAreLaidOutAsJavas)
{
    OptionsTab tab;
    ASSERT_TRUE(Gui::Test::showActiveWith(tab.window(), {&tab.panel()}));
    EXPECT_EQ(unmetTabRelations(tab.panel()), "");
}

TEST_F(SimulationOptionsPanelTest, TheStepperComboBoxSetsTheStepperAndItsPreference)
{
    OptionsTab tab;
    EXPECT_EQ(stepperSteps(tab), pinnedSteps(Pins::kStepper));
}

TEST_F(SimulationOptionsPanelTest, TheGeodeticComboBoxShowsTheStrategysDescription)
{
    const OptionsTab tab;
    EXPECT_EQ(geodeticSteps(tab), pinnedSteps(Pins::kGeodetic));
}

TEST_F(SimulationOptionsPanelTest, TheConstantGravityRowFollowsTheGravityModel)
{
    const OptionsTab tab;
    EXPECT_EQ(gravitySteps(tab), gravityExpectation());
}

TEST_F(SimulationOptionsPanelTest, TheTimeStepAndTheMaximumTimeKeepTheirBounds)
{
    const OptionsTab tab;
    EXPECT_EQ(timeSteps(tab), timesExpectation());
}

TEST_F(SimulationOptionsPanelTest, TheLookupSummaryFollowsTheOptionsTables)
{
    const OptionsTab tab;
    EXPECT_EQ(lookupSummarySteps(tab), lookupSummaryExpectation());
}

TEST_F(SimulationOptionsPanelTest, TheSeedControlsFixAndValidateTheSeed)
{
    const OptionsTab tab;
    EXPECT_EQ(seedSteps(tab), pinnedSteps(Pins::kRandomSeed));
}

TEST_F(SimulationOptionsPanelTest, ResetToDefaultTakesTheTimesTheGeodeticMethodAndTheSeed)
{
    // The example's geodetic computation is Flat Earth, as the probe's earlier steps left it,
    // and the seed is not fixed.
    OptionsTab tab;
    EXPECT_EQ(resetSteps(tab), resetExpectation());
}

TEST_F(SimulationOptionsPanelTest, SaveAsDefaultStoresTheTimesTheGeodeticMethodAndTheSeed)
{
    OptionsTab tab;
    // The state the probe's resets left: WGS84, preferences without a fixed seed.
    Gui::Test::chooseItem(tab.panel().geodeticComboBox(),
                          geodeticIndex(GeodeticComputationStrategy::WGS84));
    tab.services().preferences().setGeodeticComputationName("WGS84");
    tab.services().preferences().setRandomSeed(777);
    EXPECT_EQ(saveSteps(tab), saveExpectation());
}

/// What the test of "Configure..." does with the lookup dialog: checks it (over the panel's
/// window, modal for the application, showing the options' drag table), clears the drag table
/// and clicks OK.
struct LookupDialogAnswer
{
    const QWidget* window = nullptr;
    bool*          shown  = nullptr;

    void operator()(QWidget& dialog) const
    {
        auto* const lookup = qobject_cast<AerodynamicLookupDialog*>(&dialog);
        ASSERT_NE(lookup, nullptr);
        *shown = true;
        EXPECT_EQ(lookup->parentWidget(), window);
        EXPECT_EQ(lookup->windowModality(), Qt::ApplicationModal);
        EXPECT_NE(lookup->lookupTable(AerodynamicLookupDialog::Table::DRAG), nullptr);
        lookup->clearButton(AerodynamicLookupDialog::Table::DRAG).click();
        lookup->okButton().click();
    }
};

TEST_F(SimulationOptionsPanelTest, ConfigureOpensTheLookupDialogOfTheOptions)
{
    OptionsTab                     tab;
    const std::vector<std::string> rows{"Mach,Cd", "0,0.5", "1,0.6"};
    tab.options().setDragLookup(pathFromUtf8("drag.csv"), tableOf(rows, {"cd"}), rows);
    EXPECT_TRUE(tab.panel().lookupSummaryIcon().toolTip().contains(QStringLiteral("drag.csv")));
    bool                             shown = false;
    const Gui::Test::NextModalDialog answer(
        LookupDialogAnswer{.window = &tab.window(), .shown = &shown});
    tab.panel().configureLookupButton().click();
    EXPECT_TRUE(answer.acted());
    EXPECT_TRUE(shown);
    // OK gave the options the cleared table; the summary follows.
    EXPECT_FALSE(tab.options().hasDragLookup());
    EXPECT_EQ(tab.panel().lookupSummaryIcon().toolTip(),
              QStringLiteral("<html>Drag: Using built-in Barrowman data<br>Stability: Using "
                             "built-in Barrowman data</html>"));
}

TEST_F(SimulationOptionsPanelTest, PrepareForSimulationResolvesTheSeedControls)
{
    const OptionsTab          tab;
    const RandomSeedControls& seed = tab.panel().randomSeed();
    seed.checkBox().click();
    seed.field().setText(QStringLiteral("  -17 "));
    tab.panel().prepareForSimulation();
    EXPECT_TRUE(tab.options().isRandomSeedFixed());
    EXPECT_EQ(tab.options().getRandomSeed(), -17);
    EXPECT_TRUE(seed.checkBox().isChecked());
}

TEST_F(SimulationOptionsPanelTest, TheExtensionsBoxListsTheSimulationsExtensions)
{
    const OptionsTab tab;
    EXPECT_EQ(tab.panel().extensionList().simulation().get(), &tab.simulation());
    EXPECT_EQ(tab.panel().extensionList().extensionCount(), 0U);
    EXPECT_EQ(tab.panel().extensionsDescription().description(),
              QStringLiteral("<html><i>Simulation extensions</i> enable advanced features and "
                             "custom functionality during flight simulations.  You can for "
                             "example do hardware-in-the-loop testing with them."));
}

TEST_F(SimulationOptionsPanelTest, ThePanelNeedsASimulation)
{
    const Gui::Test::TestGuiServices services;
    QWidget                          window;
    BindingScope                     scope(window, nullptr);
    EXPECT_THROW(SimulationOptionsPanel(nullptr, services.services(), scope, nullptr), BugError);
    scope.release();
}

TEST_F(SimulationOptionsPanelTest, AReleasedScopeEndsTheFieldsWrites)
{
    OptionsTab tab;
    tab.scope().release();
    EXPECT_FALSE(tab.panel().timeStep().spinBox->binding()->commitText("0.02"));
    tab.panel().resetButton().click();
    tab.panel().saveButton().click();
    tab.panel().randomSeed().checkBox().click();
    tab.panel().prepareForSimulation();
    EXPECT_DOUBLE_EQ(tab.options().getTimeStep(), 0.05);
    EXPECT_FALSE(tab.options().isRandomSeedFixed());
    EXPECT_FALSE(tab.services().preferences().isRandomSeedFixed());
}

}  // namespace

}  // namespace QtRocket::Gui
