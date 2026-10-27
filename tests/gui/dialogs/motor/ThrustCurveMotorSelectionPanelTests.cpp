// The motor chooser's panel (Java: ThrustCurveMotorSelectionPanel). OpenRocket has no JUnit test
// of it; these tests follow ThrustCurveMotorSelectionPanel.java's listeners over the eleven sets of
// smallMotorDatabase(), with the expectations of the probe
// tools/openrocket-probes/motor-selection-panel (its expected.txt: the COUNT, VERSION, ENABLED,
// DELAY, NAMECOLUMN, STALE, TABLEKEYS and RESELECT lines, which OpenRocket's real panel printed
// over the bundled database). The core's decisions (the curves, the delays, the preferred curve)
// are pinned against OpenRocket in tests/core/motor/chooser; these tests are about the widgets.

#include "QtRocket/gui/dialogs/motor/ThrustCurveMotorSelectionPanel.h"

#include <array>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QModelIndex>
#include <QPalette>
#include <QPoint>
#include <QPointer>
#include <QRadioButton>
#include <QRect>
#include <QSignalSpy>
#include <QString>
#include <QTabWidget>
#include <QTableView>
#include <QTest>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "MotorChooserTestSupport.h"
#include "QtRocket/gui/dialogs/motor/MotorFilterPanel.h"
#include "QtRocket/gui/dialogs/motor/MotorInformationPanel.h"
#include "QtRocket/gui/dialogs/motor/MotorTableModel.h"
#include "QtRocket/gui/plot/PlotColors.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorPreferences.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/motor/chooser/MotorRowFilter.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorSelection.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/util/BugError.h"
#include "ScopedApplicationPalette.h"
#include "TestGuiServices.h"
#include "motor/TestMotorDatabase.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using ThrustCurveMotorSelectionPanelTest = Gui::Test::GuiTest;
using Gui::Test::BodyTubeMount;
using Gui::Test::chooseComboItem;
using Gui::Test::doubleClickAt;
using Gui::Test::typeIntoComboBox;

/// The panel over smallMotorDatabase(), which it fills into @p services' motors first, in
/// @p window.
[[nodiscard]] ThrustCurveMotorSelectionPanel* makePanel(Gui::Test::TestGuiServices& services,
                                                        QWidget&                    window)
{
    services.motors() = QtRocket::Test::smallMotorDatabase();
    return makeChild<ThrustCurveMotorSelectionPanel>(&window, services.services());
}

/// The panel over smallMotorDatabase() in a window of its own, with fresh preferences.
class Chooser
{
public:
    Chooser() : m_panel(makePanel(m_services, m_window))
    {
        auto* layout = makeChild<QVBoxLayout>(&m_window);
        layout->addWidget(m_panel);
    }

    [[nodiscard]] ThrustCurveMotorSelectionPanel& panel() const noexcept { return *m_panel; }
    [[nodiscard]] QWidget&                        window() noexcept { return m_window; }
    [[nodiscard]] Gui::Test::TestGuiServices&     services() noexcept { return m_services; }
    [[nodiscard]] ThrustCurveMotorSelection&      selection() const { return m_panel->selection(); }

    /// The panel's child of the type T named @p name.
    template <class T>
    [[nodiscard]] T& child(const QString& name) const
    {
        T* found = m_panel->findChild<T*>(name);
        if (found == nullptr)
        {
            bug("The selection panel has no child " + QtStrings::toUtf8(name));
        }
        return *found;
    }

    [[nodiscard]] QComboBox& curves() const
    {
        return child<QComboBox>(QStringLiteral("curveSelectionBox"));
    }
    [[nodiscard]] QComboBox& delays() const { return child<QComboBox>(QStringLiteral("delayBox")); }
    [[nodiscard]] QTableView& table() const { return child<QTableView>(QStringLiteral("table")); }
    [[nodiscard]] QLineEdit&  search() const
    {
        return child<QLineEdit>(QStringLiteral("searchField"));
    }
    [[nodiscard]] UnitSpinBox& nozzle() const
    {
        return child<UnitSpinBox>(QStringLiteral("nozzleExitDiameterSpinner"));
    }
    [[nodiscard]] QLabel& label(const QString& name) const { return child<QLabel>(name); }
    [[nodiscard]] MotorInformationPanel& information() const
    {
        return child<MotorInformationPanel>(QStringLiteral("motorInformationPanel"));
    }

    /// The set of the database named so.
    [[nodiscard]] const ThrustCurveMotorSet& set(std::string_view manufacturer,
                                                 std::string_view designation)
    {
        return Gui::Test::findMotorSet(m_services.motors(), manufacturer, designation);
    }

    /// Selects the row of the set named so in the table, as a click on it does.
    void selectThroughTable(std::string_view manufacturer, std::string_view designation)
    {
        const int row = m_panel->tableModel().rowOf(set(manufacturer, designation));
        if (row < 0)
        {
            bug("the table does not show " + std::string(designation));
        }
        table().selectRow(row);
    }

    /// The set of the table's selected row, or null.
    [[nodiscard]] const ThrustCurveMotorSet* tableSet() const
    {
        const QModelIndexList rows = table().selectionModel()->selectedRows();
        return rows.isEmpty() ? nullptr : &m_panel->tableModel().motorSet(rows.front().row());
    }

private:
    Gui::Test::TestGuiServices      m_services;
    QWidget                         m_window;
    ThrustCurveMotorSelectionPanel* m_panel{nullptr};
};

/// The texts of @p box's items, separated by '|'.
[[nodiscard]] std::string itemTexts(const QComboBox& box)
{
    std::string texts;
    for (int index = 0; index < box.count(); ++index)
    {
        texts += (index == 0 ? "" : "|") + QtStrings::toUtf8(box.itemText(index));
    }
    return texts;
}

/// The set indices of @p curves in @p set, separated by ','.
[[nodiscard]] std::string curveIndices(
    const ThrustCurveMotorSet&                                  set,
    const std::vector<std::shared_ptr<const ThrustCurveMotor>>& curves)
{
    std::string indices;
    for (const std::shared_ptr<const ThrustCurveMotor>& curve : curves)
    {
        std::size_t index = 0;
        while (index < set.getMotors().size() && set.getMotors()[index] != curve)
        {
            ++index;
        }
        indices += std::format("{}{}", indices.empty() ? "" : ",", index);
    }
    return indices;
}

/// The enabled states of the left column's controls, as the probe's ENABLED lines print them.
[[nodiscard]] std::string enabledStates(const Chooser& chooser)
{
    return std::format(
        "curveLabel={} curveBox={} delayLabel={} delayBox={} nozzleLabel={} nozzleSpinner={} "
        "nozzleUnit={} curves={} delays={}",
        chooser.label(QStringLiteral("curveSelectionLabel")).isEnabled(),
        chooser.curves().isEnabled(),
        chooser.label(QStringLiteral("ejectionChargeDelayLabel")).isEnabled(),
        chooser.delays().isEnabled(),
        chooser.label(QStringLiteral("nozzleExitDiameterLabel")).isEnabled(),
        chooser.nozzle().isEnabled(),
        chooser.child<UnitSelector>(QStringLiteral("nozzleExitDiameterUnitSelector")).isEnabled(),
        chooser.curves().count(), chooser.delays().count());
}

// ------------------------------------------------------------------------------ the left column

TEST_F(ThrustCurveMotorSelectionPanelTest, StartsWithoutAMotorAndTheSelectionControlsDisabled)
{
    const Chooser chooser;
    EXPECT_EQ(chooser.panel().objectName(), QStringLiteral("selectionPanel"));
    EXPECT_EQ(chooser.panel().getSelectedMotor(), nullptr);
    EXPECT_EQ(chooser.panel().getSelectedDelay(), 0.0);
    EXPECT_EQ(chooser.panel().getSelectedNozzleExitDiameter(), 0.0);
    // ENABLED no motor
    EXPECT_EQ(enabledStates(chooser),
              "curveLabel=false curveBox=false delayLabel=false delayBox=false nozzleLabel=false "
              "nozzleSpinner=false nozzleUnit=false curves=0 delays=0");
    EXPECT_EQ(chooser.delays().currentText(), QString());
    EXPECT_EQ(chooser.table().selectionModel()->selectedRows().size(), 0);
    EXPECT_EQ(chooser.information().selectedMotor(), nullptr);
    EXPECT_EQ(&chooser.panel().getDefaultFocus(), &chooser.search());
}

TEST_F(ThrustCurveMotorSelectionPanelTest, ShowsJavasTextsInTheLeftColumn)
{
    const Chooser chooser;
    EXPECT_EQ(chooser.label(QStringLiteral("curveSelectionLabel")).text(),
              QStringLiteral("Select thrust curve:"));
    EXPECT_EQ(chooser.label(QStringLiteral("ejectionChargeDelayLabel")).text(),
              QStringLiteral("Ejection charge delay:"));
    EXPECT_EQ(chooser.label(QStringLiteral("nozzleExitDiameterLabel")).text(),
              QStringLiteral("Nozzle exit diameter:"));
    EXPECT_EQ(chooser.child<QCheckBox>(QStringLiteral("hideSimilarBox")).text(),
              QStringLiteral("Hide very similar thrust curves"));
    EXPECT_EQ(chooser.child<QCheckBox>(QStringLiteral("hideUnavailableBox")).text(),
              QStringLiteral("Hide motors which are not in regular production"));
    EXPECT_EQ(chooser.label(QStringLiteral("motorNameColumn")).text(),
              QStringLiteral("Motor name column:"));
    EXPECT_EQ(chooser.label(QStringLiteral("motorNameColumn")).toolTip(),
              QStringLiteral("Select which motor property to display in the motor name column."));
    EXPECT_EQ(chooser.child<QRadioButton>(QStringLiteral("commonName")).text(),
              QStringLiteral("Use common name"));
    EXPECT_EQ(chooser.child<QRadioButton>(QStringLiteral("designation")).text(),
              QStringLiteral("Use manufacturer's designation"));
    // VERSION and COUNT initial (the probe's database has 1458 rows, this one 11).
    EXPECT_EQ(chooser.label(QStringLiteral("motorDbVersionLabel")).text(),
              QStringLiteral("Motor DB version: Unknown"));
    EXPECT_EQ(chooser.label(QStringLiteral("motorDbVersionLabel")).toolTip(), QString());
    EXPECT_EQ(chooser.label(QStringLiteral("nrOfMotorsLabel")).text(),
              QStringLiteral("Number of motors: 11"));
    EXPECT_EQ(chooser.label(QStringLiteral("nrOfMotorsLabel")).toolTip(),
              QStringLiteral("Number of motors currently visible in the motor selection table"));
    EXPECT_TRUE(chooser.label(QStringLiteral("nrOfMotorsLabel")).font().italic());
    EXPECT_TRUE(chooser.label(QStringLiteral("motorDbVersionLabel")).font().italic());
}

TEST_F(ThrustCurveMotorSelectionPanelTest, TheTabsAreTheFilterAndTheDetails)
{
    const Chooser chooser;
    const auto&   tabs = chooser.child<QTabWidget>(QStringLiteral("rightSide"));
    ASSERT_EQ(tabs.count(), 2);
    EXPECT_EQ(tabs.tabText(0), QStringLiteral("Filter Motors"));
    EXPECT_EQ(tabs.tabText(1), QStringLiteral("Show Details"));
    EXPECT_EQ(tabs.widget(0), &chooser.child<MotorFilterPanel>(QStringLiteral("motorFilterPanel")));
    EXPECT_EQ(tabs.widget(1), &chooser.information());
    EXPECT_EQ(tabs.currentIndex(), 0);
}

TEST_F(ThrustCurveMotorSelectionPanelTest, CountsTheRowsTheTableShows)
{
    // COUNT search zzz / search estes c6 / search cleared.
    const Chooser chooser;
    chooser.search().setText(QStringLiteral("zzz"));
    EXPECT_EQ(chooser.label(QStringLiteral("nrOfMotorsLabel")).text(),
              QStringLiteral("Number of motors: None"));
    chooser.search().setText(QStringLiteral("estes c6"));
    EXPECT_EQ(chooser.label(QStringLiteral("nrOfMotorsLabel")).text(),
              QStringLiteral("Number of motors: 1"));
    chooser.search().setText(QString());
    EXPECT_EQ(chooser.label(QStringLiteral("nrOfMotorsLabel")).text(),
              QStringLiteral("Number of motors: 11"));
}

TEST_F(ThrustCurveMotorSelectionPanelTest, ShowsTheMotorDatabasesVersion)
{
    const Chooser chooser;
    chooser.panel().setMotorDatabaseVersion(20260418111610);
    EXPECT_EQ(chooser.label(QStringLiteral("motorDbVersionLabel")).text(),
              QStringLiteral("Motor DB version: 20260418111610"));
    // getLocalMotorDatabaseVersionText(): only a version above 0.
    chooser.panel().setMotorDatabaseVersion(0);
    EXPECT_EQ(chooser.label(QStringLiteral("motorDbVersionLabel")).text(),
              QStringLiteral("Motor DB version: Unknown"));
    chooser.panel().setMotorDatabaseVersion(std::nullopt);
    EXPECT_EQ(chooser.label(QStringLiteral("motorDbVersionLabel")).text(),
              QStringLiteral("Motor DB version: Unknown"));
}

TEST_F(ThrustCurveMotorSelectionPanelTest, EnablesTheCurveListForMoreThanOneCurve)
{
    // ENABLED AeroTech E15W / AeroTech E16W.
    Chooser chooser;
    chooser.selectThroughTable("AeroTech", "E15W");
    EXPECT_EQ(enabledStates(chooser),
              "curveLabel=true curveBox=true delayLabel=true delayBox=true nozzleLabel=true "
              "nozzleSpinner=true nozzleUnit=true curves=2 delays=7");
    chooser.selectThroughTable("AeroTech", "E16W");
    EXPECT_EQ(enabledStates(chooser),
              "curveLabel=false curveBox=false delayLabel=true delayBox=true nozzleLabel=true "
              "nozzleSpinner=true nozzleUnit=true curves=1 delays=5");
}

TEST_F(ThrustCurveMotorSelectionPanelTest, StoresBothHideBoxesWhenItIsMade)
{
    // The constructor runs both boxes' listeners, which store them as read.
    Chooser chooser;
    EXPECT_EQ(chooser.services().preferences().get(PreferenceKeys::kMotorHideSimilar), "true");
    EXPECT_EQ(chooser.services().preferences().get(PreferenceKeys::kMotorHideUnavailable), "true");
    EXPECT_TRUE(chooser.child<QCheckBox>(QStringLiteral("hideSimilarBox")).isChecked());
    EXPECT_TRUE(chooser.child<QCheckBox>(QStringLiteral("hideUnavailableBox")).isChecked());
}

TEST_F(ThrustCurveMotorSelectionPanelTest, StartsWithTheStoredBoxesAndNameColumn)
{
    Gui::Test::TestGuiServices services;
    services.motors() = QtRocket::Test::smallMotorDatabase();
    MotorPreferences::setHideSimilar(services.preferences(), false);
    MotorPreferences::setHideUnavailable(services.preferences(), false);
    services.preferences().setMotorNameColumn(false);
    QWidget window;
    auto*   panel = makeChild<ThrustCurveMotorSelectionPanel>(&window, services.services());
    EXPECT_FALSE(panel->findChild<QCheckBox*>(QStringLiteral("hideSimilarBox"))->isChecked());
    EXPECT_FALSE(panel->findChild<QCheckBox*>(QStringLiteral("hideUnavailableBox"))->isChecked());
    EXPECT_FALSE(panel->selection().isHideSimilar());
    EXPECT_FALSE(panel->selection().isHideUnavailable());
    EXPECT_TRUE(panel->findChild<QRadioButton*>(QStringLiteral("commonName"))->isChecked());
    EXPECT_FALSE(panel->findChild<QRadioButton*>(QStringLiteral("designation"))->isChecked());
    EXPECT_EQ(services.preferences().get(PreferenceKeys::kMotorHideSimilar), "false");
}

TEST_F(ThrustCurveMotorSelectionPanelTest, TheHideBoxesAreStoredAndApplied)
{
    Chooser chooser;
    chooser.selectThroughTable("AeroTech", "E15W");
    auto& hideSimilar = chooser.child<QCheckBox>(QStringLiteral("hideSimilarBox"));
    hideSimilar.click();
    EXPECT_EQ(chooser.services().preferences().get(PreferenceKeys::kMotorHideSimilar), "false");
    EXPECT_EQ(chooser.curves().count(), 3);
    EXPECT_EQ(chooser.information().curves().size(), 3U);
    hideSimilar.click();
    EXPECT_EQ(chooser.services().preferences().get(PreferenceKeys::kMotorHideSimilar), "true");
    EXPECT_EQ(chooser.curves().count(), 2);

    auto& hideUnavailable = chooser.child<QCheckBox>(QStringLiteral("hideUnavailableBox"));
    hideUnavailable.click();
    EXPECT_EQ(chooser.services().preferences().get(PreferenceKeys::kMotorHideUnavailable), "false");
    EXPECT_FALSE(chooser.selection().getRowFilter().isHideUnavailable());
    hideUnavailable.click();
    EXPECT_TRUE(chooser.selection().getRowFilter().isHideUnavailable());
}

// ------------------------------------------------------------------------------ the curve list

TEST_F(ThrustCurveMotorSelectionPanelTest, ChoosingACurveSelectsIt)
{
    Chooser                    chooser;
    const ThrustCurveMotorSet& e15w = chooser.set("AeroTech", "E15W");
    chooser.selectThroughTable("AeroTech", "E15W");
    // The best curve, with the curve 1 that is very similar to it hidden.
    EXPECT_EQ(chooser.panel().getSelectedMotor(), e15w.getMotors()[0]);
    EXPECT_EQ(curveIndices(e15w, chooser.information().curves()), "0,2");
    EXPECT_EQ(chooser.curves().currentIndex(), 0);
    EXPECT_EQ(itemTexts(chooser.curves()), "E15W|E15W");
    chooseComboItem(chooser.curves(), 1);
    // E15W curve item 1: curve 2, which is like neither of the others.
    EXPECT_EQ(chooser.panel().getSelectedMotor(), e15w.getMotors()[2]);
    EXPECT_EQ(curveIndices(e15w, chooser.information().curves()), "0,1,2");
    EXPECT_EQ(chooser.curves().currentIndex(), 2);
    EXPECT_EQ(chooser.information().selectedMotor(), e15w.getMotors()[2]);
    // The same set: the delay stays.
    EXPECT_EQ(chooser.delays().currentText(), QStringLiteral("0"));
}

TEST_F(ThrustCurveMotorSelectionPanelTest, TheCurveChosenStaysWhenHideSimilarIsTurnedOn)
{
    // Path A of the curve-box pins, AeroTech E15W curve 2 (bug 20): OpenRocket ends on curve 1
    // with the list 1,2; QtRocket keeps curve 2 with the list 0,1,2.
    Chooser                    chooser;
    const ThrustCurveMotorSet& e15w = chooser.set("AeroTech", "E15W");
    chooser.selectThroughTable("AeroTech", "E15W");
    auto& hideSimilar = chooser.child<QCheckBox>(QStringLiteral("hideSimilarBox"));
    hideSimilar.click();
    chooseComboItem(chooser.curves(), 2);
    EXPECT_EQ(chooser.panel().getSelectedMotor(), e15w.getMotors()[2]);
    hideSimilar.click();
    EXPECT_EQ(chooser.panel().getSelectedMotor(), e15w.getMotors()[2]);
    EXPECT_EQ(curveIndices(e15w, chooser.information().curves()), "0,1,2");
    EXPECT_EQ(chooser.curves().count(), 3);
    EXPECT_EQ(chooser.curves().currentIndex(), 2);
}

TEST_F(ThrustCurveMotorSelectionPanelTest, ThePreferredCurveStaysSelectedFromTheTable)
{
    // Path B (bug 20): curve 2 stored as the set's preferred one, then the set selected through
    // the table of a new panel.
    Gui::Test::TestGuiServices services;
    services.motors() = QtRocket::Test::smallMotorDatabase();
    const ThrustCurveMotorSet& e15w =
        Gui::Test::findMotorSet(services.motors(), "AeroTech", "E15W");
    {
        QWidget window;
        auto*   panel = makeChild<ThrustCurveMotorSelectionPanel>(&window, services.services());
        panel->selectedMotor(*e15w.getMotors()[2]);
    }
    QWidget     window;
    const auto* panel = makeChild<ThrustCurveMotorSelectionPanel>(&window, services.services());
    auto*       table = panel->findChild<QTableView*>(QStringLiteral("table"));
    table->selectRow(panel->tableModel().rowOf(e15w));
    EXPECT_EQ(panel->getSelectedMotor(), e15w.getMotors()[2]);
    const auto* information = panel->findChild<MotorInformationPanel*>();
    EXPECT_EQ(curveIndices(e15w, information->curves()), "0,1,2");
    EXPECT_EQ(panel->findChild<QComboBox*>(QStringLiteral("curveSelectionBox"))->currentIndex(), 2);
}

TEST_F(ThrustCurveMotorSelectionPanelTest, OpeningForAConfigurationKeepsItsCurve)
{
    // Path C (bug 20): opened for a configuration of curve 2.
    Chooser                     chooser;
    const ThrustCurveMotorSet&  e15w = chooser.set("AeroTech", "E15W");
    BodyTubeMount               mount;
    const FlightConfigurationId fcid = mount.configure(e15w.getMotors()[2], 4);
    chooser.panel().setMotorMountAndConfig(fcid, mount.mount());
    EXPECT_EQ(chooser.panel().getSelectedMotor(), e15w.getMotors()[2]);
    EXPECT_EQ(curveIndices(e15w, chooser.information().curves()), "0,1,2");
    EXPECT_EQ(chooser.curves().currentIndex(), 2);
    EXPECT_EQ(chooser.tableSet(), &e15w);
}

TEST_F(ThrustCurveMotorSelectionPanelTest, ColoursTheCurvesByTheirIndex)
{
    Chooser chooser;
    chooser.child<QCheckBox>(QStringLiteral("hideSimilarBox")).click();
    chooser.selectThroughTable("AeroTech", "M1850W");
    ASSERT_EQ(chooser.curves().count(), 3);
    const QPalette palette = chooser.panel().palette();
    for (int index = 0; index < 3; ++index)
    {
        EXPECT_EQ(chooser.curves().itemData(index, Qt::ForegroundRole).value<QColor>(),
                  PlotColors::curveColor(index, palette))
            << index;
    }
    // The closed box: the selected curve's colour.
    EXPECT_EQ(chooser.curves().palette().color(QPalette::Active, QPalette::ButtonText),
              PlotColors::curveColor(0, palette));
}

TEST_F(ThrustCurveMotorSelectionPanelTest, TheCurveColoursFollowThePalette)
{
    Chooser chooser;
    chooser.selectThroughTable("AeroTech", "E15W");
    {
        const Gui::Test::ScopedApplicationPalette dark(Gui::Test::ScopedApplicationPalette::dark());
        QCoreApplication::sendPostedEvents();
        EXPECT_EQ(chooser.curves().itemData(1, Qt::ForegroundRole).value<QColor>(),
                  PlotColors::curveColor(1, ThemeColors::Theme::DARK));
    }
    QCoreApplication::sendPostedEvents();
    EXPECT_EQ(chooser.curves().itemData(1, Qt::ForegroundRole).value<QColor>(),
              PlotColors::curveColor(1, chooser.panel().palette()));
}

TEST_F(ThrustCurveMotorSelectionPanelTest, TheWheelChangesNoListWithoutTheFocus)
{
    Chooser chooser;
    chooser.selectThroughTable("AeroTech", "E15W");
    ASSERT_TRUE(Gui::Test::showActive(chooser.window()));
    chooser.search().setFocus();
    EXPECT_FALSE(Gui::Test::turnWheel(chooser.curves(), -120));
    EXPECT_FALSE(Gui::Test::turnWheel(chooser.delays(), -120));
    EXPECT_EQ(chooser.curves().currentIndex(), 0);
    EXPECT_EQ(chooser.delays().currentText(), QStringLiteral("0"));
    EXPECT_EQ(chooser.panel().getSelectedMotor(), chooser.set("AeroTech", "E15W").getMotors()[0]);
}

// ------------------------------------------------------------------------------ the delay

/// What is done to the delay box in a step of the probe's DELAY lines.
enum class DelayAction
{
    /// Selects the set named in the step's text (manufacturer, then designation, after a space)
    /// in the table.
    SELECT,
    /// Types the text over the editor's text, then Return.
    TYPE_ENTER,
    /// Return on the editor's text.
    ENTER,
    /// Types the text over the editor's text, then moves the focus away.
    TYPE_FOCUS_LOST,
    /// Gives the box the focus and moves it away again.
    FOCUS_LOST,
    /// Chooses the step's item in the popup.
    CHOOSE,
};

/// A step of the probe's DELAY lines, and what follows: the delay, the box's text and its item.
struct DelayStep
{
    double           delay;
    std::string_view text;
    std::string_view shown;
    DelayAction      action;
    int              item;
    int              index;
};

/// The probe's DELAY lines, on Estes C6 (the delays 0, 3, 5, 7 and Plugged), in order. The one
/// difference: "NaN" keeps the delay (bug 11; OpenRocket takes NaN, shown "NaN").
constexpr double kPlugged = std::numeric_limits<double>::infinity();
// NOLINTBEGIN(modernize-use-designated-initializers): the probe's transcript, line by line
constexpr auto kDelaySteps = std::to_array<DelayStep>({
    {0.0, "Estes C6", "0", DelayAction::SELECT, 0, 0},
    {5.0, "5.04", "5", DelayAction::TYPE_ENTER, 0, 2},
    {5.0, "", "5", DelayAction::ENTER, 0, 2},
    {6.0, "6", "6", DelayAction::TYPE_FOCUS_LOST, 0, -1},
    {5.0, "5.04", "5", DelayAction::TYPE_ENTER, 0, 2},
    {5.0, "", "5", DelayAction::FOCUS_LOST, 0, 2},
    {5.0, "5", "5", DelayAction::TYPE_FOCUS_LOST, 0, 2},
    {5.0, "4.99", "5", DelayAction::TYPE_FOCUS_LOST, 0, 2},
    {5.0, "", "5", DelayAction::FOCUS_LOST, 0, 2},
    {3.0, "", "3", DelayAction::CHOOSE, 1, 1},
    {3.0, "", "3", DelayAction::CHOOSE, 1, 1},
    {kPlugged, "", "Plugged", DelayAction::CHOOSE, 4, 4},
    {kPlugged, "abc", "Plugged", DelayAction::TYPE_ENTER, 0, 4},
    {kPlugged, "abc", "Plugged", DelayAction::TYPE_FOCUS_LOST, 0, 4},
    {7.0, "7", "7", DelayAction::TYPE_FOCUS_LOST, 0, 3},
    {3.0, "3", "3", DelayAction::TYPE_FOCUS_LOST, 0, 1},
    {8.0, "0x1p3", "8", DelayAction::TYPE_ENTER, 0, -1},
    {kPlugged, "none", "Plugged", DelayAction::TYPE_ENTER, 0, 4},
    {5.0, " 5 ", "5", DelayAction::TYPE_ENTER, 0, 2},
    // OpenRocket: delay NaN, shown "NaN", no item.
    {5.0, "NaN", "5", DelayAction::TYPE_ENTER, 0, 2},
    {2.0, "2", "2", DelayAction::TYPE_ENTER, 0, -1},
    {2.0, "Estes 1/2A3", "2", DelayAction::SELECT, 0, 0},
});
// NOLINTEND(modernize-use-designated-initializers)

/// Does @p step to @p chooser's delay box.
void doDelayStep(Chooser& chooser, const DelayStep& step)
{
    QComboBox&    box  = chooser.delays();
    const QString text = QtStrings::fromUtf8(step.text);
    switch (step.action)
    {
        case DelayAction::SELECT:
        {
            const std::size_t space = step.text.find(' ');
            chooser.selectThroughTable(step.text.substr(0, space), step.text.substr(space + 1));
            break;
        }
        case DelayAction::TYPE_ENTER:
            typeIntoComboBox(box, text);
            QTest::keyClick(&box, Qt::Key_Return);
            break;
        case DelayAction::ENTER:
            box.setFocus();
            QTest::keyClick(&box, Qt::Key_Return);
            break;
        case DelayAction::TYPE_FOCUS_LOST:
            typeIntoComboBox(box, text);
            chooser.search().setFocus();
            break;
        case DelayAction::FOCUS_LOST:
            box.setFocus();
            chooser.search().setFocus();
            break;
        case DelayAction::CHOOSE:
            chooseComboItem(box, step.item);
            break;
    }
}

/// The steps whose delay box differs from the probe's, one line each.
[[nodiscard]] std::string delayStepDifferences(Chooser& chooser)
{
    std::string differences;
    for (const DelayStep& step : kDelaySteps)
    {
        doDelayStep(chooser, step);
        const QComboBox& box   = chooser.delays();
        const double     delay = chooser.panel().getSelectedDelay();
        if (delay != step.delay || box.currentText() != QtStrings::fromUtf8(step.shown) ||
            box.currentIndex() != step.index)
        {
            differences += std::format("[{}]: delay {} [{}] item {}, expected {} [{}] item {}\n",
                                       step.text, delay, QtStrings::toUtf8(box.currentText()),
                                       box.currentIndex(), step.delay, step.shown, step.index);
        }
    }
    return differences;
}

TEST_F(ThrustCurveMotorSelectionPanelTest, TheDelayBoxCommitsAsOpenRocketsComboBox)
{
    Chooser chooser;
    ASSERT_TRUE(Gui::Test::showActive(chooser.window()));
    EXPECT_EQ(delayStepDifferences(chooser), "");
}

TEST_F(ThrustCurveMotorSelectionPanelTest, TheDelayBoxListsTheSetsDelaysAndPlugged)
{
    Chooser chooser;
    chooser.selectThroughTable("Estes", "C6");
    EXPECT_EQ(itemTexts(chooser.delays()), "0|3|5|7|Plugged");
    EXPECT_TRUE(chooser.delays().isEditable());
    // G12ST: the plugged delay only ("P").
    chooser.selectThroughTable("AeroTech", "G12ST");
    EXPECT_EQ(itemTexts(chooser.delays()), "Plugged");
    EXPECT_EQ(chooser.delays().currentText(), QStringLiteral("Plugged"));
    EXPECT_EQ(chooser.panel().getSelectedDelay(), Motor::kPluggedDelay);
}

TEST_F(ThrustCurveMotorSelectionPanelTest, TheArrowKeysChooseTheNextDelay)
{
    Chooser chooser;
    ASSERT_TRUE(Gui::Test::showActive(chooser.window()));
    chooser.selectThroughTable("Estes", "C6");
    chooser.delays().setFocus();
    QTest::keyClick(&chooser.delays(), Qt::Key_Down);
    EXPECT_EQ(chooser.panel().getSelectedDelay(), 3.0);
    EXPECT_EQ(chooser.delays().currentText(), QStringLiteral("3"));
    QTest::keyClick(&chooser.delays(), Qt::Key_Up);
    EXPECT_EQ(chooser.panel().getSelectedDelay(), 0.0);
}

TEST_F(ThrustCurveMotorSelectionPanelTest, OpeningTheDelayListCommitsTheTypedText)
{
    // Qt's line edit loses the focus to the popup (Java's keeps the edit until a choice).
    Chooser chooser;
    ASSERT_TRUE(Gui::Test::showActive(chooser.window()));
    chooser.selectThroughTable("Estes", "C6");
    typeIntoComboBox(chooser.delays(), QStringLiteral("6"));
    {
        const Gui::Test::OffscreenPopupWarnings warnings;
        chooser.delays().showPopup();
        QCoreApplication::processEvents();
        EXPECT_EQ(chooser.panel().getSelectedDelay(), 6.0);
        chooser.delays().hidePopup();
    }
    chooseComboItem(chooser.delays(), 3);
    EXPECT_EQ(chooser.panel().getSelectedDelay(), 7.0);
}

TEST_F(ThrustCurveMotorSelectionPanelTest, TheDelayEditorCompletesNothing)
{
    // Java's editable JComboBox has no completion: "1" is not completed to an item.
    Chooser chooser;
    ASSERT_TRUE(Gui::Test::showActive(chooser.window()));
    chooser.selectThroughTable("AeroTech", "E15W");
    EXPECT_EQ(chooser.delays().completer(), nullptr);
    typeIntoComboBox(chooser.delays(), QStringLiteral("1"));
    EXPECT_EQ(chooser.delays().lineEdit()->text(), QStringLiteral("1"));
    QTest::keyClick(&chooser.delays(), Qt::Key_Return);
    EXPECT_EQ(chooser.panel().getSelectedDelay(), 1.0);
}

TEST_F(ThrustCurveMotorSelectionPanelTest, CommitEditsCommitsWhatIsBeingTyped)
{
    Chooser chooser;
    ASSERT_TRUE(Gui::Test::showActive(chooser.window()));
    chooser.selectThroughTable("Estes", "C6");
    typeIntoComboBox(chooser.delays(), QStringLiteral("4.96"));
    chooser.panel().commitEdits();
    // Both action events: 4.96, then its text "5" read back.
    EXPECT_EQ(chooser.panel().getSelectedDelay(), 5.0);
    EXPECT_EQ(chooser.delays().currentText(), QStringLiteral("5"));
    // Moving the focus on commits nothing more ("5" is the selected item).
    Gui::Test::typeInto(chooser.nozzle(), QStringLiteral("0.5"));
    EXPECT_EQ(chooser.panel().getSelectedDelay(), 5.0);
    EXPECT_EQ(chooser.panel().getSelectedNozzleExitDiameter(), 0.0);
    chooser.panel().commitEdits();
    EXPECT_DOUBLE_EQ(chooser.panel().getSelectedNozzleExitDiameter(), 0.005);
    // Committing again changes nothing.
    chooser.panel().commitEdits();
    EXPECT_EQ(chooser.panel().getSelectedDelay(), 5.0);
    EXPECT_DOUBLE_EQ(chooser.panel().getSelectedNozzleExitDiameter(), 0.005);
}

// ------------------------------------------------------------------------------ the nozzle

TEST_F(ThrustCurveMotorSelectionPanelTest, TheNozzleExitDiameterIsAtMostTheMotorsDiameter)
{
    Chooser chooser;
    ASSERT_TRUE(Gui::Test::showActive(chooser.window()));
    chooser.selectThroughTable("Estes", "C6");
    EXPECT_EQ(chooser.nozzle().text(), QStringLiteral("0"));
    // Length in cm: 0.4 cm, then 5 cm (above the 18 mm motor).
    Gui::Test::typeInto(chooser.nozzle(), QStringLiteral("0.4"));
    QTest::keyClick(&chooser.nozzle(), Qt::Key_Return);
    EXPECT_DOUBLE_EQ(chooser.panel().getSelectedNozzleExitDiameter(), 0.004);
    Gui::Test::typeInto(chooser.nozzle(), QStringLiteral("5"));
    QTest::keyClick(&chooser.nozzle(), Qt::Key_Return);
    EXPECT_EQ(chooser.panel().getSelectedNozzleExitDiameter(),
              chooser.set("Estes", "C6").getMotors()[0]->getDiameter());
    EXPECT_EQ(chooser.nozzle().text(), QStringLiteral("1.8"));
    // Below 0: 0.
    Gui::Test::typeInto(chooser.nozzle(), QStringLiteral("-1"));
    QTest::keyClick(&chooser.nozzle(), Qt::Key_Return);
    EXPECT_EQ(chooser.panel().getSelectedNozzleExitDiameter(), 0.0);
    // The step is 0.1 of the unit.
    EXPECT_EQ(chooser.nozzle().fixedStep(), 0.1);
}

TEST_F(ThrustCurveMotorSelectionPanelTest, AnotherCurveResetsTheNozzleExitDiameter)
{
    Chooser chooser;
    ASSERT_TRUE(Gui::Test::showActive(chooser.window()));
    chooser.selectThroughTable("Estes", "C6");
    Gui::Test::typeInto(chooser.nozzle(), QStringLiteral("0.4"));
    QTest::keyClick(&chooser.nozzle(), Qt::Key_Return);
    chooseComboItem(chooser.curves(), 1);
    EXPECT_EQ(chooser.panel().getSelectedNozzleExitDiameter(), 0.0);
    EXPECT_EQ(chooser.nozzle().text(), QStringLiteral("0"));
}

// ------------------------------------------------------------------------------ the table

TEST_F(ThrustCurveMotorSelectionPanelTest, TheTableSelectsTheSetsDefaultCurve)
{
    Chooser                    chooser;
    const ThrustCurveMotorSet& c6 = chooser.set("Estes", "C6");
    chooser.selectThroughTable("Estes", "C6");
    EXPECT_EQ(chooser.panel().getSelectedMotor(), c6.getMotors()[0]);
    EXPECT_EQ(chooser.selection().getSelectedMotorSet(), &c6);
    EXPECT_EQ(chooser.information().selectedMotor(), c6.getMotors()[0]);
    EXPECT_EQ(chooser.tableSet(), &c6);
    const QTableView& table = chooser.table();
    EXPECT_EQ(table.selectionBehavior(), QAbstractItemView::SelectRows);
    EXPECT_EQ(table.selectionMode(), QAbstractItemView::SingleSelection);
    EXPECT_FALSE(table.isSortingEnabled());
    EXPECT_FALSE(table.verticalHeader()->isVisible());
    EXPECT_EQ(table.horizontalHeader()->sectionSize(0), MotorTableModel::preferredWidth(0));
}

TEST_F(ThrustCurveMotorSelectionPanelTest, ASelectionFilteredOutStaysAndComesBack)
{
    // scrollSelectionVisible(): a set the search hides stays selected and is selected in the table
    // again when it is shown.
    Chooser                    chooser;
    const ThrustCurveMotorSet& c6 = chooser.set("Estes", "C6");
    chooser.selectThroughTable("Estes", "C6");
    chooser.search().setText(QStringLiteral("aerotech"));
    EXPECT_EQ(chooser.tableSet(), nullptr);
    EXPECT_EQ(chooser.selection().getSelectedMotorSet(), &c6);
    EXPECT_EQ(chooser.information().selectedMotor(), c6.getMotors()[0]);
    EXPECT_EQ(chooser.curves().count(), 2);
    chooser.search().setText(QStringLiteral("es"));
    EXPECT_EQ(chooser.tableSet(), &c6);
    EXPECT_EQ(chooser.selection().getSelectedMotorSet(), &c6);
}

TEST_F(ThrustCurveMotorSelectionPanelTest, TheNameColumnSwitchKeepsTheSelectedSet)
{
    // NAMECOLUMN, over the bundled database as the probe: sorted by Name, row 10 is Quest A3T;
    // switched to the common name, OpenRocket re-selects row 10, which is Estes A3 then. QtRocket
    // keeps Quest A3T (bug 10) and scrolls to its row.
    Gui::Test::TestGuiServices services;
    services.motors() = QtRocket::Test::bundledMotorDatabase();
    QWidget window;
    auto*   panel = makeChild<ThrustCurveMotorSelectionPanel>(&window, services.services());
    const ThrustCurveMotorSet& a3t   = Gui::Test::findMotorSet(services.motors(), "Quest", "A3T");
    const ThrustCurveMotorSet& a3    = Gui::Test::findMotorSet(services.motors(), "Estes", "A3");
    auto*                      table = panel->findChild<QTableView*>(QStringLiteral("table"));
    panel->tableModel().toggleSortOrder(1);
    table->selectRow(10);
    EXPECT_EQ(panel->selection().getSelectedMotorSet(), &a3t);
    panel->findChild<QRadioButton*>(QStringLiteral("commonName"))->click();
    EXPECT_FALSE(services.preferences().getMotorNameColumn());
    EXPECT_EQ(&panel->tableModel().motorSet(10), &a3);
    EXPECT_EQ(panel->selection().getSelectedMotorSet(), &a3t);
    ASSERT_EQ(table->selectionModel()->selectedRows().size(), 1);
    EXPECT_EQ(table->selectionModel()->selectedRows().front().row(),
              panel->tableModel().rowOf(a3t));
    EXPECT_EQ(panel->findChild<QComboBox*>(QStringLiteral("curveSelectionBox"))->currentText(),
              QStringLiteral("A3"));
    panel->findChild<QRadioButton*>(QStringLiteral("designation"))->click();
    EXPECT_TRUE(services.preferences().getMotorNameColumn());
    EXPECT_EQ(panel->selection().getSelectedMotorSet(), &a3t);
    EXPECT_EQ(table->selectionModel()->selectedRows().front().row(), 10);
    EXPECT_EQ(panel->findChild<QComboBox*>(QStringLiteral("curveSelectionBox"))->currentText(),
              QStringLiteral("A3T"));
}

TEST_F(ThrustCurveMotorSelectionPanelTest, TheCurveListShowsTheNameColumnsName)
{
    Chooser chooser;
    chooser.selectThroughTable("Quest", "B6W");
    EXPECT_EQ(itemTexts(chooser.curves()), "B6W");
    chooser.child<QRadioButton>(QStringLiteral("commonName")).click();
    EXPECT_EQ(itemTexts(chooser.curves()), "B6");
    EXPECT_EQ(chooser.selection().getSelectedMotorSet(), &chooser.set("Quest", "B6W"));
}

/// The table's selected row, its current row (Java's lead) and the current cell's column on the
/// screen, as the probe's TABLEKEYS lines print them (-1 for none).
[[nodiscard]] std::string tableCursor(const Chooser& chooser)
{
    const QTableView&     table   = chooser.table();
    const QModelIndex     current = table.currentIndex();
    const QModelIndexList rows    = table.selectionModel()->selectedRows();
    return std::format(
        "row={} lead={} column={}", rows.isEmpty() ? -1 : rows.front().row(),
        current.isValid() ? current.row() : -1,
        current.isValid() ? table.horizontalHeader()->visualIndex(current.column()) : -1);
}

TEST_F(ThrustCurveMotorSelectionPanelTest, EnterInTheTableSelectsTheNextRowAsAJTable)
{
    // TABLEKEYS (selectNextRowCell, selectPreviousRowCell), over the eleven rows of the small
    // database (the probe's table has 1458: its last row is 1457 where this one's is 10).
    Chooser chooser;
    ASSERT_TRUE(Gui::Test::showActive(chooser.window()));
    QTableView& table = chooser.table();
    EXPECT_EQ(tableCursor(chooser), "row=-1 lead=-1 column=-1");
    // The focus makes no row current (Qt's item views make the first cell current).
    table.setFocus();
    ASSERT_TRUE(table.hasFocus());
    EXPECT_EQ(tableCursor(chooser), "row=-1 lead=-1 column=-1");
    QTest::keyClick(&table, Qt::Key_Return);
    EXPECT_EQ(tableCursor(chooser), "row=0 lead=0 column=0");
    EXPECT_EQ(chooser.selection().getSelectedMotorSet(), &chooser.panel().tableModel().motorSet(0));
    QTest::keyClick(&table, Qt::Key_Enter, Qt::KeypadModifier);
    EXPECT_EQ(tableCursor(chooser), "row=1 lead=1 column=0");
    EXPECT_EQ(chooser.selection().getSelectedMotorSet(), &chooser.panel().tableModel().motorSet(1));
    QTest::keyClick(&table, Qt::Key_Return, Qt::ShiftModifier);
    EXPECT_EQ(tableCursor(chooser), "row=0 lead=0 column=0");
    // Shift+Enter on the first row: the last row, the column before (the last).
    QTest::keyClick(&table, Qt::Key_Return, Qt::ShiftModifier);
    EXPECT_EQ(tableCursor(chooser), "row=10 lead=10 column=5");
    EXPECT_EQ(chooser.selection().getSelectedMotorSet(),
              &chooser.panel().tableModel().motorSet(10));
    // Enter on the last row: the first row, the next column (the first).
    QTest::keyClick(&table, Qt::Key_Return);
    EXPECT_EQ(tableCursor(chooser), "row=0 lead=0 column=0");
    table.clearSelection();
    EXPECT_EQ(tableCursor(chooser), "row=-1 lead=0 column=0");
    QTest::keyClick(&table, Qt::Key_Return);
    EXPECT_EQ(tableCursor(chooser), "row=1 lead=1 column=0");
    table.selectRow(10);
    EXPECT_EQ(tableCursor(chooser), "row=10 lead=10 column=0");
    QTest::keyClick(&table, Qt::Key_Return);
    EXPECT_EQ(tableCursor(chooser), "row=0 lead=0 column=1");
    // An empty table: nothing. (Java keeps its column lead, 1, without a row; Qt's current cell
    // goes with its row, so that the selection shown again starts from the first column.)
    chooser.search().setText(QStringLiteral("zzz"));
    EXPECT_EQ(tableCursor(chooser), "row=-1 lead=-1 column=-1");
    QTest::keyClick(&table, Qt::Key_Return);
    EXPECT_EQ(tableCursor(chooser), "row=-1 lead=-1 column=-1");
    chooser.search().setText(QString());
    EXPECT_EQ(tableCursor(chooser), "row=0 lead=0 column=0");
}

TEST_F(ThrustCurveMotorSelectionPanelTest, TheTableHasNoTypeAhead)
{
    // TABLEKEYS binding [typed a]: no action (a JTable's typed key only starts an edit, which the
    // motor table's cells refuse); Qt's item views select the next row whose text starts so.
    Chooser chooser;
    ASSERT_TRUE(Gui::Test::showActive(chooser.window()));
    QTableView& table = chooser.table();
    table.setFocus();
    table.selectRow(0);
    const ThrustCurveMotorSet* const first = chooser.selection().getSelectedMotorSet();
    ASSERT_NE(first, nullptr);
    QTest::keyClicks(&table, QStringLiteral("qe"));
    EXPECT_EQ(tableCursor(chooser), "row=0 lead=0 column=0");
    EXPECT_EQ(chooser.selection().getSelectedMotorSet(), first);
}

TEST_F(ThrustCurveMotorSelectionPanelTest, OnlyTheLeftButtonSelectsInTheTable)
{
    // TABLEKEYS right button on row 5, middle button on row 6: the selection stays (Swing's
    // BasicTableUI ignores the other buttons).
    Chooser chooser;
    ASSERT_TRUE(Gui::Test::showActive(chooser.window()));
    QTableView& table = chooser.table();
    table.selectRow(3);
    const ThrustCurveMotorSet* const selected = chooser.selection().getSelectedMotorSet();
    const auto                       center   = [&table](int row) {
        return table.visualRect(table.model()->index(row, 1)).center();
    };
    QTest::mouseClick(table.viewport(), Qt::RightButton, {}, center(5));
    EXPECT_EQ(chooser.tableSet(), selected);
    QTest::mouseClick(table.viewport(), Qt::MiddleButton, {}, center(6));
    EXPECT_EQ(chooser.tableSet(), selected);
    doubleClickAt(*table.viewport(), center(7), Qt::RightButton);
    EXPECT_EQ(chooser.tableSet(), selected);
    EXPECT_EQ(chooser.selection().getSelectedMotorSet(), selected);
    QTest::mouseClick(table.viewport(), Qt::LeftButton, {}, center(5));
    EXPECT_EQ(chooser.tableSet(), &chooser.panel().tableModel().motorSet(5));
}

TEST_F(ThrustCurveMotorSelectionPanelTest, TheTablesColumnsCanBeMoved)
{
    // TABLEKEYS reorderingAllowed=true; Enter goes on to the next column on the screen.
    Chooser chooser;
    ASSERT_TRUE(Gui::Test::showActive(chooser.window()));
    QTableView&  table  = chooser.table();
    QHeaderView& header = *table.horizontalHeader();
    EXPECT_TRUE(header.sectionsMovable());
    header.moveSection(0, 5);
    EXPECT_EQ(header.logicalIndex(0), 1);
    table.setFocus();
    table.selectionModel()->setCurrentIndex(
        table.model()->index(10, 0),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    EXPECT_EQ(tableCursor(chooser), "row=10 lead=10 column=5");
    QTest::keyClick(&table, Qt::Key_Return);
    EXPECT_EQ(tableCursor(chooser), "row=0 lead=0 column=0");
    EXPECT_EQ(table.currentIndex().column(), 1);
    // The rows are the model's still: the moved column sorts as its own.
    EXPECT_EQ(chooser.panel().tableModel().headerData(0, Qt::Horizontal, Qt::DisplayRole),
              header.model()->headerData(0, Qt::Horizontal, Qt::DisplayRole));
}

TEST_F(ThrustCurveMotorSelectionPanelTest, ADoubleClickAnywhereOnTheTableAsksForOk)
{
    Chooser chooser;
    ASSERT_TRUE(Gui::Test::showActive(chooser.window()));
    const QSignalSpy  ok(&chooser.panel(), &ThrustCurveMotorSelectionPanel::okRequested);
    const QTableView& table = chooser.table();
    const QPoint      cell  = table.visualRect(table.model()->index(0, 0)).center();
    doubleClickAt(*table.viewport(), cell);
    EXPECT_EQ(ok.count(), 1);
    EXPECT_EQ(chooser.tableSet(), &chooser.panel().tableModel().motorSet(0));
    // Below the rows (fillsViewportHeight).
    chooser.search().setText(QStringLiteral("estes c6"));
    doubleClickAt(*table.viewport(), QPoint(cell.x(), table.viewport()->height() - 2));
    EXPECT_EQ(ok.count(), 2);
    // Another button, a single click, or a release away from the double click: nothing.
    doubleClickAt(*table.viewport(), cell, Qt::RightButton);
    QTest::mouseClick(table.viewport(), Qt::LeftButton, {}, cell);
    QTest::mouseClick(table.viewport(), Qt::LeftButton, {}, cell);
    QTest::mousePress(table.viewport(), Qt::LeftButton, {}, cell);
    QTest::mouseDClick(table.viewport(), Qt::LeftButton, {}, cell);
    QTest::mouseRelease(table.viewport(), Qt::LeftButton, {}, cell + QPoint(0, 20));
    EXPECT_EQ(ok.count(), 2);
}

// ------------------------------------------------------------------------------ the mount

TEST_F(ThrustCurveMotorSelectionPanelTest, OpensForAConfigurationWithItsMotorDelayAndNozzle)
{
    Chooser                     chooser;
    const ThrustCurveMotorSet&  c6 = chooser.set("Estes", "C6");
    BodyTubeMount               mount;
    const FlightConfigurationId fcid = mount.configure(c6.getMotors()[0], 6, 0.01);
    chooser.panel().setMotorMountAndConfig(fcid, mount.mount());
    EXPECT_EQ(chooser.panel().getSelectedMotor(), c6.getMotors()[0]);
    // Bug 8: the configured delay, not the closest standard one (OpenRocket: 7).
    EXPECT_EQ(chooser.panel().getSelectedDelay(), 6.0);
    EXPECT_EQ(chooser.delays().currentText(), QStringLiteral("6"));
    EXPECT_EQ(chooser.delays().currentIndex(), -1);
    EXPECT_EQ(itemTexts(chooser.delays()), "0|3|5|7|Plugged");
    EXPECT_EQ(chooser.panel().getSelectedNozzleExitDiameter(), 0.01);
    EXPECT_EQ(chooser.nozzle().text(), QStringLiteral("1"));
    EXPECT_EQ(chooser.tableSet(), &c6);
    EXPECT_EQ(chooser.information().selectedMotor(), c6.getMotors()[0]);
}

/// The panel's state as the probe's STALE lines print it (without the nozzle's and delay's
/// values in s and m, which the tests compare themselves).
[[nodiscard]] std::string staleState(const Chooser& chooser)
{
    const std::shared_ptr<const ThrustCurveMotor> motor = chooser.panel().getSelectedMotor();
    const ThrustCurveMotorSet*                    row   = chooser.tableSet();
    return std::format(
        "motor={} rowSet={} curves=[{}] curveBox={} delays=[{}] delayText=[{}] delayBox={} "
        "nozzleText=[{}] info=[{}] series={}",
        motor == nullptr ? "none" : motor->getDesignation(),
        row == nullptr ? "none" : row->getDesignation(), itemTexts(chooser.curves()),
        chooser.curves().isEnabled(), itemTexts(chooser.delays()),
        QtStrings::toUtf8(chooser.delays().currentText()), chooser.delays().isEnabled(),
        QtStrings::toUtf8(chooser.nozzle().text()),
        QtStrings::toUtf8(
            chooser.information().findChild<QLabel*>(QStringLiteral("designationLabel"))->text()),
        chooser.information().curves().size());
}

TEST_F(ThrustCurveMotorSelectionPanelTest, AnEmptyConfigurationLeavesTheLastMotorShown)
{
    // STALE: the widgets keep the motor of before until open()'s selectMotorFromTable().
    Chooser                     chooser;
    const ThrustCurveMotorSet&  c6 = chooser.set("Estes", "C6");
    BodyTubeMount               mount;
    const FlightConfigurationId withMotor = mount.configure(c6.getMotors()[0], 5, 0.004);
    const FlightConfigurationId empty     = mount.addEmptyConfiguration();
    chooser.panel().setMotorMountAndConfig(empty, mount.mount());
    EXPECT_EQ(staleState(chooser),
              "motor=none rowSet=none curves=[] curveBox=false delays=[] "
              "delayText=[] delayBox=false nozzleText=[0] info=[] series=0");
    chooser.panel().selectMotorFromTable();
    EXPECT_EQ(chooser.panel().getSelectedMotor(), nullptr);

    chooser.panel().setMotorMountAndConfig(withMotor, mount.mount());
    EXPECT_EQ(staleState(chooser),
              "motor=C6 rowSet=C6 curves=[C6|C6] curveBox=true delays=[0|3|5|7|Plugged] "
              "delayText=[5] delayBox=true nozzleText=[0.4] info=[C6] series=2");
    EXPECT_EQ(chooser.panel().getSelectedDelay(), 5.0);

    chooser.panel().setMotorMountAndConfig(empty, mount.mount());
    EXPECT_EQ(staleState(chooser),
              "motor=none rowSet=C6 curves=[C6|C6] curveBox=true delays=[0|3|5|7|Plugged] "
              "delayText=[5] delayBox=true nozzleText=[0] info=[C6] series=2");
    EXPECT_EQ(chooser.panel().getSelectedDelay(), 0.0);
    EXPECT_EQ(chooser.panel().getSelectedNozzleExitDiameter(), 0.0);

    chooser.panel().selectMotorFromTable();
    EXPECT_EQ(staleState(chooser),
              "motor=C6 rowSet=C6 curves=[C6|C6] curveBox=true delays=[0|3|5|7|Plugged] "
              "delayText=[0] delayBox=true nozzleText=[0] info=[C6] series=2");
    EXPECT_EQ(chooser.panel().getSelectedDelay(), 0.0);
}

/// The panel opened for @p mount in @p fcid as the chooser dialog opens it: the state after
/// setMotorMountAndConfig() and after selectMotorFromTable() (open()), as staleState() writes them,
/// separated by " | ".
[[nodiscard]] std::string openAs(const Chooser& chooser, const FlightConfigurationId& fcid,
                                 const InnerTube& mount)
{
    chooser.panel().setMotorMountAndConfig(fcid, mount);
    const std::string afterMount = staleState(chooser);
    chooser.panel().selectMotorFromTable();
    return afterMount + " | " + staleState(chooser);
}

TEST_F(ThrustCurveMotorSelectionPanelTest, AnEmptyConfigurationSelectsTheSetAFilterHidAgain)
{
    // RESELECT: AeroTech E15W (24 mm) selected for the 18 mm mount, then hidden by "Limit motor
    // diameter to mount diameter"; opened for the 29 mm mount's empty configuration, the panel
    // selects its row again and with it its default curve (OpenRocket's first
    // motorFilterPanel.setMotorMount() scrolls to the set of before, before the reset); opened for
    // the 18 mm mount again, where the limit hides it, nothing.
    Chooser                             chooser;
    const Gui::Test::TwoInnerTubeMounts mounts;
    ThrustCurveMotorSelectionPanel&     panel = chooser.panel();
    const std::string                   none =
        "motor=none rowSet=none curves=[] curveBox=false delays=[] "
        "delayText=[] delayBox=false nozzleText=[0] info=[] series=0";
    EXPECT_EQ(openAs(chooser, mounts.fcid(), mounts.mount18()), none + " | " + none);
    chooser.selectThroughTable("AeroTech", "E15W");
    const std::string e15w =
        "curves=[E15W|E15W] curveBox=true delays=[0|4|6|7|8|10|Plugged] "
        "delayText=[0] delayBox=true nozzleText=[0] info=[E15W] series=2";
    EXPECT_EQ(staleState(chooser), "motor=E15W rowSet=E15W " + e15w);
    chooser.child<QCheckBox>(QStringLiteral("limitDiameterCheckBox")).click();
    EXPECT_EQ(staleState(chooser), "motor=E15W rowSet=none " + e15w);
    const std::shared_ptr<const ThrustCurveMotor> chosen = panel.getSelectedMotor();
    ASSERT_NE(chosen, nullptr);
    panel.selectedMotor(*chosen);

    EXPECT_EQ(openAs(chooser, mounts.fcid(), mounts.mount29()),
              "motor=E15W rowSet=E15W " + e15w + " | motor=E15W rowSet=E15W " + e15w);
    EXPECT_EQ(panel.getSelectedMotor(), chosen);
    EXPECT_EQ(panel.getSelectedDelay(), 0.0);
    EXPECT_EQ(panel.getSelectedNozzleExitDiameter(), 0.0);
    panel.selectedMotor(*chosen);

    EXPECT_EQ(openAs(chooser, mounts.fcid(), mounts.mount18()),
              "motor=none rowSet=none " + e15w + " | motor=none rowSet=none " + e15w);
    EXPECT_EQ(panel.getSelectedDelay(), 0.0);
}

TEST_F(ThrustCurveMotorSelectionPanelTest, OpensForAMotorOfNoSetWithASetOfItsOwn)
{
    const Chooser                                 chooser;
    const std::shared_ptr<const ThrustCurveMotor> custom = QtRocket::Test::estesA8Motor();
    BodyTubeMount                                 mount;
    const FlightConfigurationId                   fcid = mount.configure(custom, 3);
    chooser.panel().setMotorMountAndConfig(fcid, mount.mount());
    EXPECT_EQ(chooser.panel().getSelectedMotor(), custom);
    EXPECT_EQ(chooser.panel().tableModel().rowCount(), 12);
    EXPECT_EQ(chooser.label(QStringLiteral("nrOfMotorsLabel")).text(),
              QStringLiteral("Number of motors: 12"));
    ASSERT_NE(chooser.tableSet(), nullptr);
    EXPECT_EQ(chooser.tableSet()->getMotors().front(), custom);
}

TEST_F(ThrustCurveMotorSelectionPanelTest, TheFilterPanelsChangesSortTheTable)
{
    Chooser                     chooser;
    BodyTubeMount               mount;
    const FlightConfigurationId fcid =
        mount.configure(chooser.set("Estes", "C6").getMotors()[0], 5);
    chooser.panel().setMotorMountAndConfig(fcid, mount.mount());
    auto& hideUsed = chooser.child<QCheckBox>(QStringLiteral("hideUsedBox"));
    hideUsed.click();
    EXPECT_EQ(chooser.label(QStringLiteral("nrOfMotorsLabel")).text(),
              QStringLiteral("Number of motors: 10"));
    EXPECT_EQ(chooser.tableSet(), nullptr);
    hideUsed.click();
    EXPECT_EQ(chooser.label(QStringLiteral("nrOfMotorsLabel")).text(),
              QStringLiteral("Number of motors: 11"));
    EXPECT_EQ(chooser.tableSet(), &chooser.set("Estes", "C6"));
}

// ------------------------------------------------------------------------------ OK and lifetime

TEST_F(ThrustCurveMotorSelectionPanelTest, StoresThePreferredCurveOfItsSet)
{
    Chooser                    chooser;
    const ThrustCurveMotorSet& c6 = chooser.set("Estes", "C6");
    chooser.panel().selectedMotor(*c6.getMotors()[1]);
    EXPECT_TRUE(MotorPreferences::isPreferredThrustCurve(chooser.services().preferences(),
                                                         c6.getMotors()[1]->getDigest()));
    EXPECT_FALSE(MotorPreferences::isPreferredThrustCurve(chooser.services().preferences(),
                                                          c6.getMotors()[0]->getDigest()));
}

TEST_F(ThrustCurveMotorSelectionPanelTest, GoesSafelyWhileTheDelayIsBeingTyped)
{
    // The window goes with the delay box focused and edited: its focus loss reaches no panel
    // (ASan proves it).
    auto services      = std::make_unique<Gui::Test::TestGuiServices>();
    services->motors() = QtRocket::Test::smallMotorDatabase();
    auto  window       = std::make_unique<QWidget>();
    auto* panel  = makeChild<ThrustCurveMotorSelectionPanel>(window.get(), services->services());
    auto* layout = makeChild<QVBoxLayout>(window.get());
    layout->addWidget(panel);
    ASSERT_TRUE(Gui::Test::showActive(*window));
    auto* table = panel->findChild<QTableView*>(QStringLiteral("table"));
    table->selectRow(0);
    auto* delays = panel->findChild<QComboBox*>(QStringLiteral("delayBox"));
    typeIntoComboBox(*delays, QStringLiteral("7"));
    const QPointer<ThrustCurveMotorSelectionPanel> alive(panel);
    window.reset();
    EXPECT_TRUE(alive.isNull());
}

TEST_F(ThrustCurveMotorSelectionPanelTest, GoesSafelyWithItsCurveListOpen)
{
    auto services      = std::make_unique<Gui::Test::TestGuiServices>();
    services->motors() = QtRocket::Test::smallMotorDatabase();
    auto  window       = std::make_unique<QWidget>();
    auto* panel  = makeChild<ThrustCurveMotorSelectionPanel>(window.get(), services->services());
    auto* layout = makeChild<QVBoxLayout>(window.get());
    layout->addWidget(panel);
    ASSERT_TRUE(Gui::Test::showActive(*window));
    panel->findChild<QTableView*>(QStringLiteral("table"))->selectRow(1);
    const Gui::Test::OffscreenPopupWarnings warnings;
    panel->findChild<QComboBox*>(QStringLiteral("curveSelectionBox"))->showPopup();
    window.reset();
    QCoreApplication::processEvents();
}

}  // namespace

}  // namespace QtRocket::Gui
