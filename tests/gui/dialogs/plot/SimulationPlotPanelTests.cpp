// A simulation's "Plot data" tab (OpenRocket's SimulationPlotPanel): the presets, the rows'
// colours and looks, the events table, the marker style, doPlot() and the configuration kept for
// the next tab, over the stored flight of "A simple model rocket" #1 (and of "Three stage low power
// rocket" #1) and over SimulationChartTest's small flight.
//
// Expectations marked "probe" are OpenRocket's own, printed by tools/openrocket-gui-pins'
// plot-dialog probe PlotTabProbe (OpenRocket 5f164fd0e, JDK 17, FlatLaf Light under xvfb; its
// transcript is the end of plot-dialog/hand-pinned.txt, which generate.sh checks): OpenRocket's
// SimulationPlotPanel.create() of those flights, its controls set as Swing sets them
// (setSelectedItem(), setSelectedIndex(), doClick()), and the panel's state printed in the format
// of Gui::Test::stateText(), configurationText(), rowsText(), eventsText() and extrasText()
// (dialogs/plot/PlotPanelTestSupport.h). simulation_plot_panel_golden_tests.cpp compares every
// preset of every stored example simulation with the core's plot goldens.

#include "QtRocket/gui/dialogs/plot/SimulationPlotPanel.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QAbstractItemModelTester>
#include <QColor>
#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QModelIndex>
#include <QPoint>
#include <QPushButton>
#include <QRadioButton>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QTableView>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>
#include <rocketplot/LineSeries.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/dialogs/plot/FlightEventTableModel.h"
#include "QtRocket/gui/dialogs/plot/PlotTypeSelector.h"
#include "QtRocket/gui/dialogs/plot/SimulationPlotTypeSelector.h"
#include "QtRocket/gui/plot/SimulationPlot.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/ColorChooserButton.h"
#include "QtRocket/gui/widgets/DataTypeComboBox.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"
#include "QtRocket/gui/widgets/LineStyleComboBox.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/plot/PlotPreferences.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/plot/SimulationPlotModel.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/PlotAppearance.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/LineStyle.h"
#include "RecordingSimulationDialogs.h"
#include "ScriptedUserPrompts.h"
#include "SimulationRunTestSupport.h"
#include "TestGuiServices.h"
#include "dialogs/plot/PlotPanelTestSupport.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using SimulationPlotPanelTest = Gui::Test::GuiTest;
using Gui::Test::ExampleDesign;
using Id = FlightDataTypeId;

[[nodiscard]] const FlightDataType& type(Id id)
{
    return FlightDataType::builtin(id);
}

/// The tab of the first stored simulation of an example, with the test's services, in a window
/// of its own (never shown unless the test asks: show()).
class StoredPanel
{
public:
    explicit StoredPanel(const char* example = ExampleDesign::kSimpleModelRocket)
      : StoredPanel(nullptr, example)
    {
    }
    StoredPanel(Gui::Test::TestGuiServices& services, const char* example)
      : StoredPanel(&services, example)
    {
    }

    [[nodiscard]] Gui::Test::TestGuiServices& services() const noexcept { return *m_services; }
    [[nodiscard]] Simulation&                 simulation() const { return *m_design.simulation(0); }
    [[nodiscard]] SimulationPlotPanel&        panel() const noexcept { return *m_panel; }

    /// Shows the window, laid out: whether it became active.
    [[nodiscard]] bool show()
    {
        m_layout->activate();
        return Gui::Test::showActive(m_window);
    }

    /// A new tab of the same simulation, as the settings dialog makes it when it opens again (the
    /// old one goes later).
    void reopen()
    {
        m_panel->hide();
        m_layout->removeWidget(m_panel);
        m_panel->deleteLater();
        m_panel = makePanel();
    }

    /// Chooses the preset @p index in the preset box, as the user does.
    void choosePreset(int index) const { m_panel->configurationSelector().setCurrentIndex(index); }

    [[nodiscard]] std::string state() const { return Gui::Test::stateText(*m_panel); }
    [[nodiscard]] std::string configuration() const
    {
        return Gui::Test::configurationText(m_panel->getConfiguration());
    }
    [[nodiscard]] std::string rows() const { return Gui::Test::rowsText(*m_panel); }
    [[nodiscard]] std::string extras() const { return Gui::Test::extrasText(*m_panel); }
    [[nodiscard]] std::string events() const
    {
        return Gui::Test::eventsText(m_panel->eventTableModel());
    }

private:
    StoredPanel(Gui::Test::TestGuiServices* shared, const char* example)
      : m_services(shared != nullptr ? shared : &m_ownServices),
        m_design(example),
        m_layout(makeChild<QVBoxLayout>(&m_window)),
        m_panel(makePanel())
    {
    }

    [[nodiscard]] SimulationPlotPanel* makePanel()
    {
        auto* panel = makeChild<SimulationPlotPanel>(&m_window, m_services->services(),
                                                     m_design.simulation(0));
        m_layout->addWidget(panel);
        return panel;
    }

    Gui::Test::TestGuiServices  m_ownServices;
    Gui::Test::TestGuiServices* m_services;
    ExampleDesign               m_design;
    QWidget                     m_window;
    QVBoxLayout*                m_layout;
    SimulationPlotPanel*        m_panel;
};

/// The events of the presets but the drag coefficients', the ground track's and the roll
/// characteristics'.
constexpr const char* kPresetEvents =
    " events=[Motor ignition, Motor burnout, Stage separation, Apogee, Recovery device "
    "deployment, Ground hit, Tumbling, Warning, Simulation abort, Exception]";

/// The marker style over the time, and over another type (extrasText() without the description).
constexpr const char* kMarkerStyleOverTime =
    " marker style enabled=true tooltip='Style of the flight event marker (how it's drawn in the "
    "simulation plot)' Vertical line enabled=true selected=true tooltip=none Icon enabled=true "
    "selected=false tooltip=none";
constexpr const char* kMarkerStyleOverAnother =
    " marker style enabled=false tooltip='Only available for time domain, other domains only "
    "support icon markers' Vertical line enabled=false selected=true tooltip='Only available for "
    "time domain, other domains only support icon markers' Icon enabled=false selected=false "
    "tooltip='Only available for time domain, other domains only support icon markers'";

/// A preset of "A simple model rocket" #1 as the probe printed it.
struct PresetPins
{
    const char* state;
    const char* configuration;
    const char* rows;
};

// probe: "== presets of 'A simple model rocket' #1", preset 0 to 8.
constexpr std::array<PresetPins, 9> kPresetPins{{
    {.state = "preset box=0 'Vertical motion vs. time' X box='Time' X unit=s",
     .configuration =
         "name='Vertical motion vs. time' X=Time [s] rows=3: Altitude [m] @0 color=none Solid; "
         "Vertical velocity [m/s] @-1 color=none Solid; Vertical acceleration [m/s\u00b2] @-1 "
         "color=none Solid",
     .rows = "0 Altitude [m] Left #0072bd Solid setDefault=off reset=off | 1 Vertical velocity "
             "[m/s] Auto "
             "#d95319 Solid setDefault=off reset=off | 2 Vertical acceleration [m/s\u00b2] Auto "
             "#edb120 "
             "Solid setDefault=off reset=off"},
    {.state = "preset box=1 'Total motion vs. time' X box='Time' X unit=s",
     .configuration =
         "name='Total motion vs. time' X=Time [s] rows=3: Altitude [m] @0 color=none Solid; Total "
         "velocity [m/s] @-1 color=none Solid; Total acceleration [m/s\u00b2] @-1 color=none Solid",
     .rows =
         "0 Altitude [m] Left #0072bd Solid setDefault=off reset=off | 1 Total velocity [m/s] Auto "
         "#d95319 Solid setDefault=off reset=off | 2 Total acceleration [m/s\u00b2] Auto #edb120 "
         "Solid "
         "setDefault=off reset=off"},
    {.state = "preset box=2 'Flight side profile' X box='Position East of launch' X unit=m",
     .configuration =
         "name='Flight side profile' X=Position East of launch [m] rows=1: Altitude [m] @-1 "
         "color=none Solid",
     .rows = "0 Altitude [m] Auto #0072bd Solid setDefault=off reset=off"},
    {.state         = "preset box=3 'Ground track' X box='Position East of launch' X unit=m",
     .configuration = "name='Ground track' X=Position East of launch [m] rows=2: Position North of "
                      "launch [m] @0 "
                      "color=none Solid; Altitude [m] @1 color=none Solid",
     .rows = "0 Position North of launch [m] Left #0072bd Solid setDefault=off reset=off | 1 "
             "Altitude [m] "
             "Right #d95319 Solid setDefault=off reset=off"},
    {.state = "preset box=4 'Stability vs. time' X box='Time' X unit=s",
     .configuration =
         "name='Stability vs. time' X=Time [s] rows=3: Stability margin calibers [\u200b] @0 "
         "color=none Solid; CP location [cm] @1 color=none Solid; CG location [cm] @1 color=none "
         "Solid",
     .rows =
         "0 Stability margin calibers [\u200b] Left #0072bd Solid setDefault=off reset=off | 1 CP "
         "location [cm] Right #d95319 Solid setDefault=off reset=off | 2 CG location [cm] Right "
         "#edb120 Solid setDefault=off reset=off"},
    {.state = "preset box=5 'Drag coefficients vs. Mach number' X box='Mach number' X unit=\u200b",
     .configuration =
         "name='Drag coefficients vs. Mach number' X=Mach number [\u200b] rows=4: Drag coefficient "
         "(CD) [\u200b] @0 color=none Solid; Friction drag coefficient (CD_friction) [\u200b] @0 "
         "color=none Solid; Base drag coefficient (CD_base) [\u200b] @0 color=none Solid; Pressure "
         "drag coefficient (CD_pressure) [\u200b] @0 color=none Solid",
     .rows = "0 Drag coefficient (CD) [\u200b] Left #0072bd Solid setDefault=off reset=off | 1 "
             "Friction "
             "drag coefficient (CD_friction) [\u200b] Left #d95319 Solid setDefault=off reset=off "
             "| 2 Base "
             "drag coefficient (CD_base) [\u200b] Left #edb120 Solid setDefault=off reset=off | 3 "
             "Pressure "
             "drag coefficient (CD_pressure) [\u200b] Left #7e318e Solid setDefault=off reset=off"},
    {.state = "preset box=6 'Roll characteristics' X box='Time' X unit=s",
     .configuration =
         "name='Roll characteristics' X=Time [s] rows=4: Roll rate (Z) [\u00b0/s] @0 color=none "
         "Solid; Roll moment coefficient [\u200b] @1 color=none Solid; Roll forcing coefficient "
         "[\u200b] @1 color=none Solid; Roll damping coefficient [\u200b] @1 color=none Solid",
     .rows =
         "0 Roll rate (Z) [\u00b0/s] Left #0072bd Solid setDefault=off reset=off | 1 Roll moment "
         "coefficient [\u200b] Right #d95319 Solid setDefault=off reset=off | 2 Roll forcing "
         "coefficient [\u200b] Right #edb120 Solid setDefault=off reset=off | 3 Roll damping "
         "coefficient [\u200b] Right #7e318e Solid setDefault=off reset=off"},
    {.state = "preset box=7 'Angle of attack and orientation vs. time' X box='Time' X unit=s",
     .configuration =
         "name='Angle of attack and orientation vs. time' X=Time [s] rows=3: Angle of attack "
         "[\u00b0] @0 color=none Solid; Lateral orientation (azimuth) [\u00b0] @-1 color=none "
         "Solid; "
         "Vertical orientation (zenith) [\u00b0] @-1 color=none Solid",
     .rows =
         "0 Angle of attack [\u00b0] Left #0072bd Solid setDefault=off reset=off | 1 Lateral "
         "orientation (azimuth) [\u00b0] Auto #d95319 Solid setDefault=off reset=off | 2 Vertical "
         "orientation (zenith) [\u00b0] Auto #edb120 Solid setDefault=off reset=off"},
    {.state = "preset box=8 'Simulation time step and computation time' X box='Time' X unit=s",
     .configuration =
         "name='Simulation time step and computation time' X=Time [s] rows=2: Simulation time step "
         "[s] @-1 color=none Solid; Computation time [s] @-1 color=none Solid",
     .rows = "0 Simulation time step [s] Auto #0072bd Solid setDefault=off reset=off | 1 "
             "Computation time "
             "[s] Auto #d95319 Solid setDefault=off reset=off"},
}};

/// The events of preset @p index (probe: the presets' "events=[...]").
[[nodiscard]] std::string presetEvents(std::size_t index)
{
    switch (index)
    {
        case 3:
            return " events=[Motor ignition, Motor burnout, Apogee, Recovery device deployment, "
                   "Ground hit, Warning, Simulation abort, Exception]";
        case 5:
            return " events=[Warning, Simulation abort, Exception]";
        case 6:
            return " events=[Motor ignition, Launch rod clearance, Motor burnout, Stage "
                   "separation, "
                   "Apogee, Recovery device deployment, Ground hit, Tumbling, Warning, Simulation "
                   "abort, Exception]";
        default:
            return kPresetEvents;
    }
}

/// Checks that @p panel shows preset @p index as the probe printed it.
void expectPreset(const StoredPanel& panel, std::size_t index)
{
    const PresetPins& pins = kPresetPins.at(index);
    EXPECT_EQ(panel.state(), pins.state);
    EXPECT_EQ(panel.configuration(), pins.configuration + presetEvents(index));
    EXPECT_EQ(panel.rows(), pins.rows);
}

TEST_F(SimulationPlotPanelTest, TheTabOfAStoredFlightStartsWithTheFirstPreset)
{
    // probe: "== plot tab simple: 'Simulation 1' branches 1": the preset box's items, "initial"
    // and its "events: 10: ...".
    const StoredPanel panel;
    const QComboBox&  presets = panel.panel().configurationSelector();
    QStringList       items;
    for (int i = 0; i < presets.count(); ++i)
    {
        items.append(presets.itemText(i));
    }
    EXPECT_EQ(
        items,
        (QStringList{QStringLiteral("Vertical motion vs. time"),
                     QStringLiteral("Total motion vs. time"), QStringLiteral("Flight side profile"),
                     QStringLiteral("Ground track"), QStringLiteral("Stability vs. time"),
                     QStringLiteral("Drag coefficients vs. Mach number"),
                     QStringLiteral("Roll characteristics"),
                     QStringLiteral("Angle of attack and orientation vs. time"),
                     QStringLiteral("Simulation time step and computation time"),
                     QStringLiteral("Custom")}));
    expectPreset(panel, 0);
    EXPECT_EQ(panel.extras(), std::string("description visible=false") + kMarkerStyleOverTime);
    EXPECT_EQ(panel.events(),
              "10: Launch=false; Motor ignition=true; Lift-off=false; Launch rod clearance=false; "
              "Motor burnout=true; Ejection charge=false; Apogee=true; Recovery device "
              "deployment=true; Ground hit=true; Simulation end=false");
    // The types of the first stage, as both the X and the Y types (probe: "types 58: Time |
    // Altitude | ...").
    EXPECT_EQ(panel.panel().typesX().size(), 58U);
    EXPECT_EQ(panel.panel().typesX(), panel.panel().typesY());
    EXPECT_EQ(panel.panel().typesX().front(), &type(Id::TYPE_TIME));
}

TEST_F(SimulationPlotPanelTest, EveryPresetGivesItsRows)
{
    // probe: "== presets of 'A simple model rocket' #1": each preset a copy in the default units,
    // the rows' colours those the plot draws (SimulationPlotModel::computeDefaultColorIndices()).
    const StoredPanel panel;
    for (std::size_t index = 0; index < kPresetPins.size(); ++index)
    {
        panel.choosePreset(static_cast<int>(index));
        expectPreset(panel, index);
    }
    // "Custom chosen (the configuration keeps its name)": nothing changes but the box.
    panel.choosePreset(9);
    EXPECT_EQ(panel.state(), "preset box=9 'Custom' X box='Time' X unit=s");
    EXPECT_EQ(panel.configuration(), kPresetPins.at(8).configuration + presetEvents(8));
    EXPECT_EQ(panel.rows(), kPresetPins.at(8).rows);
}

TEST_F(SimulationPlotPanelTest, ThePresetsXTypeLeavesTheDescriptionAndTheMarkerStyle)
{
    // Kept as Java (probe: "preset 2 again: the X type is the position, the description and the
    // marker style do not follow"; "X axis -> Mach number", then "preset 0 after it: the
    // description and the marker style stay"): only the user's choice of an X type shows the
    // description and disables the marker style.
    const StoredPanel panel;
    panel.choosePreset(2);
    expectPreset(panel, 2);
    EXPECT_EQ(panel.extras(), std::string("description visible=false") + kMarkerStyleOverTime);

    panel.choosePreset(0);
    panel.panel().domainTypeSelector().setSelectedType(&type(Id::TYPE_MACH_NUMBER));
    EXPECT_EQ(panel.state(), "preset box=9 'Custom' X box='Mach number' X unit=\u200b");
    EXPECT_EQ(panel.rows(), kPresetPins.at(0).rows);
    EXPECT_EQ(panel.extras(), std::string("description visible=true") + kMarkerStyleOverAnother);
    EXPECT_EQ(panel.panel().descriptionArea().description(),
              QStringLiteral(
                  "The data will be plotted in time order even if the X axis type is not time."));

    panel.choosePreset(0);
    expectPreset(panel, 0);
    EXPECT_EQ(panel.extras(), std::string("description visible=true") + kMarkerStyleOverAnother);

    // The user's own choice of the time hides it.
    panel.panel().domainTypeSelector().setSelectedType(&type(Id::TYPE_MACH_NUMBER));
    panel.panel().domainTypeSelector().setSelectedType(&type(Id::TYPE_TIME));
    EXPECT_EQ(panel.extras(), std::string("description visible=false") + kMarkerStyleOverTime);
    EXPECT_TRUE(panel.panel().descriptionArea().description().isEmpty());
}

TEST_F(SimulationPlotPanelTest, UnitsAndAxesKeepThePresetATypeMakesItCustom)
{
    // probe: "== edits of 'A simple model rocket' #1" from "X unit -> min (no Custom)" to "row 2
    // type -> Mass": the colours computed before the axis or the type changes (kept as Java).
    const StoredPanel panel;
    const Unit*       minutes = type(Id::TYPE_TIME).getUnitGroup().getUnit("min");
    ASSERT_NE(minutes, nullptr);
    panel.panel().domainUnitSelector().setSelectedUnit(*minutes);
    EXPECT_EQ(panel.state(), "preset box=0 'Vertical motion vs. time' X box='Time' X unit=min");

    panel.choosePreset(1);
    panel.choosePreset(0);
    const Unit* feet = type(Id::TYPE_ALTITUDE).getUnitGroup().getUnit("ft");
    ASSERT_NE(feet, nullptr);
    panel.panel().plotTypeSelector(0).unitSelector().setSelectedUnit(*feet);
    EXPECT_EQ(panel.state(), "preset box=0 'Vertical motion vs. time' X box='Time' X unit=s");
    EXPECT_EQ(panel.rows(),
              "0 Altitude [ft] Left #0072bd Solid setDefault=off reset=off | 1 Vertical velocity "
              "[m/s] Auto #d95319 Solid setDefault=off reset=off | 2 Vertical acceleration "
              "[m/s\u00b2] Auto #edb120 Solid setDefault=off reset=off");

    panel.panel().plotTypeSelector(1).axisSelector().setCurrentIndex(2);
    EXPECT_EQ(panel.state(), "preset box=0 'Vertical motion vs. time' X box='Time' X unit=s");
    EXPECT_EQ(panel.configuration(),
              "name='Vertical motion vs. time' X=Time [s] rows=3: Altitude [ft] @0 color=none "
              "Solid; Vertical velocity [m/s] @1 color=none Solid; Vertical acceleration "
              "[m/s\u00b2] @-1 color=none Solid" +
                  std::string(kPresetEvents));
    EXPECT_EQ(panel.rows(),
              "0 Altitude [ft] Left #0072bd Solid setDefault=off reset=off | 1 Vertical velocity "
              "[m/s] Right #edb120 Solid setDefault=off reset=off | 2 Vertical acceleration "
              "[m/s\u00b2] Auto #d95319 Solid setDefault=off reset=off");

    panel.panel().plotTypeSelector(2).typeSelector().setSelectedType(&type(Id::TYPE_MASS));
    EXPECT_EQ(panel.state(), "preset box=9 'Custom' X box='Time' X unit=s");
    EXPECT_EQ(panel.rows(),
              "0 Altitude [ft] Left #0072bd Solid setDefault=off reset=off | 1 Vertical velocity "
              "[m/s] Right #edb120 Solid setDefault=off reset=off | 2 Mass [g] Auto #d95319 Solid "
              "setDefault=off reset=off");
}

TEST_F(SimulationPlotPanelTest, NewRowsTakeTheFirstTypesNotInUseAndRemovedOnesGo)
{
    // probe: "== edits of 'A simple model rocket' #1" from "New Y axis plot type" to "the last row
    // removed" (OpenRocket: "IndexOutOfBoundsException: Index 0 out of bounds for length 0").
    const StoredPanel panel;
    const Unit*       feet = type(Id::TYPE_ALTITUDE).getUnitGroup().getUnit("ft");
    ASSERT_NE(feet, nullptr);
    panel.panel().plotTypeSelector(0).unitSelector().setSelectedUnit(*feet);
    panel.panel().plotTypeSelector(1).axisSelector().setCurrentIndex(2);
    panel.panel().updatePlots();
    panel.panel().plotTypeSelector(2).typeSelector().setSelectedType(&type(Id::TYPE_MASS));
    QTest::mouseClick(&panel.panel().newYAxisButton(), Qt::LeftButton);
    EXPECT_EQ(panel.rows(),
              "0 Altitude [ft] Left #0072bd Solid setDefault=off reset=off | 1 Vertical velocity "
              "[m/s] Right #7e318e Solid setDefault=off reset=off | 2 Mass [g] Auto #d95319 Solid "
              "setDefault=off reset=off | 3 Altitude above sea level [m] Auto #edb120 Solid "
              "setDefault=off reset=off");
    panel.panel().newYAxisPlotType();
    panel.panel().newYAxisPlotType();
    EXPECT_EQ(panel.rows(),
              "0 Altitude [ft] Left #0072bd Solid setDefault=off reset=off | 1 Vertical velocity "
              "[m/s] Right #7e318e Solid setDefault=off reset=off | 2 Mass [g] Auto #d95319 Solid "
              "setDefault=off reset=off | 3 Altitude above sea level [m] Auto #77ac30 Solid "
              "setDefault=off reset=off | 4 Total velocity [m/s] Auto #4dbeee Solid setDefault=off "
              "reset=off | 5 Vertical acceleration [m/s\u00b2] Auto #edb120 Solid setDefault=off "
              "reset=off");
    QTest::mouseClick(&panel.panel().plotTypeSelector(1).removeButton(), Qt::LeftButton);
    EXPECT_EQ(panel.rows(),
              "0 Altitude [ft] Left #0072bd Solid setDefault=off reset=off | 1 Mass [g] Auto "
              "#d95319 Solid setDefault=off reset=off | 2 Altitude above sea level [m] Auto "
              "#7e318e Solid setDefault=off reset=off | 3 Total velocity [m/s] Auto #77ac30 Solid "
              "setDefault=off reset=off | 4 Vertical acceleration [m/s\u00b2] Auto #edb120 Solid "
              "setDefault=off reset=off");

    panel.choosePreset(8);
    QTest::mouseClick(&panel.panel().plotTypeSelector(1).removeButton(), Qt::LeftButton);
    EXPECT_EQ(panel.rows(),
              "0 Simulation time step [s] Auto #0072bd Solid setDefault=off reset=off");
    QTest::mouseClick(&panel.panel().plotTypeSelector(0).removeButton(), Qt::LeftButton);
    EXPECT_EQ(panel.configuration(),
              "name='Custom' X=Time [s] rows=0:" + std::string(kPresetEvents));
    EXPECT_EQ(panel.panel().selectorCount(), 0U);
}

TEST_F(SimulationPlotPanelTest, FifteenRowsAndNoMore)
{
    // probe: "12 times New Y axis plot type (the next would show 'A maximum of 15 plots is
    // allowed.')": the colours of fifteen rows over one stage.
    const StoredPanel panel;
    for (int i = 0; i < 12; ++i)
    {
        panel.panel().newYAxisPlotType();
    }
    EXPECT_EQ(
        panel.rows(),
        "0 Altitude [m] Left #0072bd Solid setDefault=off reset=off | 1 Vertical velocity "
        "[m/s] Auto #d95319 Solid setDefault=off reset=off | 2 Vertical acceleration "
        "[m/s\u00b2] Auto #0072bd Solid setDefault=off reset=off | 3 Altitude above sea level "
        "[m] Auto #edb120 Solid setDefault=off reset=off | 4 Total velocity [m/s] Auto "
        "#7e318e Solid setDefault=off reset=off | 5 Total acceleration [m/s\u00b2] Auto "
        "#d95319 Solid setDefault=off reset=off | 6 Position East of launch [m] Auto #edb120 "
        "Solid setDefault=off reset=off | 7 Position North of launch [m] Auto #77ac30 Solid "
        "setDefault=off reset=off | 8 Lateral distance [m] Auto #4dbeee Solid "
        "setDefault=off reset=off | 9 Lateral direction [\u00b0] Auto #7e318e Solid "
        "setDefault=off reset=off | 10 Lateral velocity [m/s] Auto #a2142f Solid "
        "setDefault=off reset=off | 11 Lateral acceleration [m/s\u00b2] Auto #c56a7a Solid "
        "setDefault=off reset=off | 12 Latitude [\u00b0 N] Auto #ff7f50 Solid "
        "setDefault=off reset=off | 13 Longitude [\u00b0 E] Auto #556b2f Solid "
        "setDefault=off reset=off | 14 Angle of attack [\u00b0] Auto #77ac30 Solid "
        "setDefault=off reset=off");
    EXPECT_TRUE(panel.services().prompts().prompts().empty());
    QTest::mouseClick(&panel.panel().newYAxisButton(), Qt::LeftButton);
    EXPECT_EQ(panel.panel().selectorCount(), 15U);
    ASSERT_EQ(panel.services().prompts().prompts().size(), 1U);
    EXPECT_EQ(panel.services().prompts().prompts().front().title,
              QStringLiteral("Cannot add plot"));
}

TEST_F(SimulationPlotPanelTest, PickedLooksBecomeTheSimulationsAndSetAsDefaultTheUsers)
{
    // probe: "== appearances of 'A simple model rocket' #1".
    StoredPanel                       panel;
    const SimulationPlotTypeSelector& row = panel.panel().plotTypeSelector(0);
    panel.services().prompts().answerColor(QColor(200, 100, 50));
    QTest::mouseClick(&row.colorButton(), Qt::LeftButton);
    EXPECT_EQ(panel.rows(),
              "0 Altitude [m] Left #c86432 Solid setDefault=on reset=on | 1 Vertical velocity "
              "[m/s] Auto #d95319 Solid setDefault=off reset=off | 2 Vertical acceleration "
              "[m/s\u00b2] Auto #edb120 Solid setDefault=off reset=off");
    // "simulation's Altitude appearance: color=200,100,50,255 lineStyle=none".
    EXPECT_EQ(panel.simulation().getPlotAppearance(type(Id::TYPE_ALTITUDE)),
              PlotAppearance(Color(200, 100, 50), std::nullopt));

    row.lineStyleSelector().setSelectedLineStyle(LineStyle::DASHED);
    EXPECT_EQ(Gui::Test::rowText(row), "0 Altitude [m] Left #c86432 Dashed setDefault=on reset=on");
    EXPECT_EQ(panel.simulation().getPlotAppearance(type(Id::TYPE_ALTITUDE)),
              PlotAppearance(Color(200, 100, 50), LineStyle::DASHED));

    // "default of Altitude: color=200,100,50,255 lineStyle=DASHED stored keys:
    // [Altitude.color=200,100,50, Altitude.lineStyle=DASHED]".
    QTest::mouseClick(&row.setDefaultButton(), Qt::LeftButton);
    EXPECT_EQ(Gui::Test::rowText(row),
              "0 Altitude [m] Left #c86432 Dashed setDefault=off reset=on");
    EXPECT_EQ(PlotPreferences::getDefaultPlotAppearance(panel.services().preferences(),
                                                        type(Id::TYPE_ALTITUDE)),
              PlotAppearance(Color(200, 100, 50), LineStyle::DASHED));

    QTest::mouseClick(&row.resetToFactoryButton(), Qt::LeftButton);
    EXPECT_EQ(Gui::Test::rowText(row), "0 Altitude [m] Left #0072bd Solid setDefault=on reset=off");
    EXPECT_EQ(panel.simulation().getPlotAppearance(type(Id::TYPE_ALTITUDE)), std::nullopt);

    // A row's new type starts with the type's look: "row 1 type -> Altitude (its default
    // appearance)", then "row 2 line style -> DOTTED".
    panel.panel().plotTypeSelector(1).typeSelector().setSelectedType(&type(Id::TYPE_ALTITUDE));
    panel.panel().plotTypeSelector(2).lineStyleSelector().setSelectedLineStyle(LineStyle::DOTTED);
    EXPECT_EQ(panel.rows(),
              "0 Altitude [m] Left #0072bd Solid setDefault=on reset=off | 1 Altitude [m] Auto "
              "#c86432 Dashed setDefault=off reset=on | 2 Vertical acceleration [m/s\u00b2] Auto "
              "#edb120 Dotted setDefault=on reset=on");
    EXPECT_EQ(panel.configuration(),
              "name='Custom' X=Time [s] rows=3: Altitude [m] @0 color=none Solid; Altitude [m] @-1 "
              "color=#c86432 Dashed; Vertical acceleration [m/s\u00b2] @-1 color=none Dotted" +
                  std::string(kPresetEvents));

    // "a new panel: the simulation's appearance of Vertical acceleration and the default of
    // Altitude".
    panel.reopen();
    EXPECT_EQ(panel.rows(),
              "0 Altitude [m] Left #c86432 Dashed setDefault=off reset=on | 1 Vertical velocity "
              "[m/s] Auto #d95319 Solid setDefault=off reset=off | 2 Vertical acceleration "
              "[m/s\u00b2] Auto #edb120 Dotted setDefault=on reset=on");
    ASSERT_EQ(panel.services().prompts().colorQuestions().size(), 1U);
    EXPECT_EQ(panel.services().prompts().colorQuestions().front().initial, QColor(0, 0x72, 0xbd));
}

TEST_F(SimulationPlotPanelTest, TheEventsTableMarksTheEventsAllAndNoneEveryType)
{
    // probe: "== events of 'A simple model rocket' #1".
    StoredPanel panel;
    ASSERT_TRUE(panel.show());
    const QAbstractItemModelTester tester(&panel.panel().eventTableModel(),
                                          QAbstractItemModelTester::FailureReportingMode::Warning);

    QTest::mouseClick(&panel.panel().noneButton(), Qt::LeftButton);
    EXPECT_EQ(panel.events(),
              "10: Launch=false; Motor ignition=false; Lift-off=false; Launch rod clearance=false; "
              "Motor burnout=false; Ejection charge=false; Apogee=false; Recovery device "
              "deployment=false; Ground hit=false; Simulation end=false");
    EXPECT_TRUE(panel.panel().getConfiguration().getActiveEvents().empty());
    QTest::mouseClick(&panel.panel().allButton(), Qt::LeftButton);
    EXPECT_EQ(panel.events(),
              "10: Launch=true; Motor ignition=true; Lift-off=true; Launch rod clearance=true; "
              "Motor burnout=true; Ejection charge=true; Apogee=true; Recovery device "
              "deployment=true; Ground hit=true; Simulation end=true");
    // Every type, also those the table does not list.
    EXPECT_EQ(panel.panel().getConfiguration().getActiveEvents().size(),
              FlightEvent::kAllTypes.size());

    FlightEventTableModel& model = panel.panel().eventTableModel();
    EXPECT_TRUE(model.setData(model.index(1, FlightEventTableModel::kSelectedColumn), Qt::Unchecked,
                              Qt::CheckStateRole));
    // "a click on row 0's name".
    const QTableView& table = panel.panel().eventTable();
    const QPoint      name =
        table.visualRect(model.index(0, FlightEventTableModel::kNameColumn)).center();
    QTest::mouseClick(table.viewport(), Qt::LeftButton, Qt::NoModifier, name);
    EXPECT_EQ(panel.events(),
              "10: Launch=false; Motor ignition=false; Lift-off=true; Launch rod clearance=true; "
              "Motor burnout=true; Ejection charge=true; Apogee=true; Recovery device "
              "deployment=true; Ground hit=true; Simulation end=true");
}

TEST_F(SimulationPlotPanelTest, TheMarkerStyleIsThePreference)
{
    // probe: "Icon clicked: pref MarkerStyleIcon=true", "Vertical line clicked: pref
    // MarkerStyleIcon=false", "a new panel with MarkerStyleIcon=true: ... Icon enabled=true
    // selected=true".
    StoredPanel panel;
    panel.panel().iconMarkerButton().click();
    EXPECT_TRUE(PlotPreferences::isMarkerStyleIcon(panel.services().preferences()));
    panel.panel().verticalMarkerButton().click();
    EXPECT_FALSE(PlotPreferences::isMarkerStyleIcon(panel.services().preferences()));
    PlotPreferences::setMarkerStyleIcon(panel.services().preferences(), true);
    panel.reopen();
    EXPECT_EQ(panel.extras(),
              "description visible=false marker style enabled=true tooltip='Style of the flight "
              "event marker (how it's drawn in the simulation plot)' Vertical line enabled=true "
              "selected=false tooltip=none Icon enabled=true selected=true tooltip=none");
}

TEST_F(SimulationPlotPanelTest, DoPlotKeepsTheConfigurationForTheNextTabWithoutItsLooks)
{
    // probe: "== doPlot": the Ground track with row 0's colour picked, the X unit in ft and a new
    // row; the configuration kept without its colours; the next tab of another simulation starts
    // from it (its description hidden, as Java's), and of the same simulation with the colour the
    // simulation keeps.
    Gui::Test::TestGuiServices services;
    StoredPanel                simple(services, ExampleDesign::kSimpleModelRocket);
    simple.choosePreset(3);
    simple.panel().plotTypeSelector(0).setSelectedColor(QColor(10, 20, 30));
    const Unit* feet = type(Id::TYPE_POSITION_X).getUnitGroup().getUnit("ft");
    ASSERT_NE(feet, nullptr);
    simple.panel().domainUnitSelector().setSelectedUnit(*feet);
    simple.panel().newYAxisPlotType();
    QWidget parent;
    EXPECT_TRUE(simple.panel().doPlot(&parent));

    constexpr const char* kGroundTrackEvents =
        " events=[Motor ignition, Motor burnout, Apogee, Recovery device deployment, Ground hit, "
        "Warning, Simulation abort, Exception]";
    EXPECT_EQ(simple.configuration(),
              "name='Custom' X=Position East of launch [ft] rows=3: Position North of launch [m] "
              "@0 color=#0a141e Solid; Altitude [m] @1 color=none Solid; Time [s] @-1 color=none "
              "Solid" +
                  std::string(kGroundTrackEvents));
    const std::shared_ptr<const SimulationPlotConfiguration> kept =
        services.plotDefaults().lastPlotted();
    ASSERT_NE(kept, nullptr);
    EXPECT_EQ(Gui::Test::configurationText(*kept),
              "name='Custom' X=Position East of launch [ft] rows=3: Position North of launch [m] "
              "@0 color=none Solid; Altitude [m] @1 color=none Solid; Time [s] @-1 color=none "
              "Solid" +
                  std::string(kGroundTrackEvents));
    const std::vector<Gui::Test::RecordingSimulationDialogs::Request>& requests =
        services.simulationDialogs().requests();
    ASSERT_EQ(requests.size(), 1U);
    EXPECT_EQ(requests.front().kind, Gui::Test::RecordingSimulationDialogs::Kind::PLOT);
    EXPECT_EQ(requests.front().parent, &parent);
    EXPECT_EQ(requests.front().simulationNames, QStringList{QStringLiteral("Simulation 1")});
    ASSERT_EQ(services.simulationDialogs().plotConfigurations().size(), 1U);
    EXPECT_EQ(
        Gui::Test::configurationText(services.simulationDialogs().plotConfigurations().front()),
        simple.configuration());

    const StoredPanel three(services, ExampleDesign::kThreeStageLowPowerRocket);
    EXPECT_EQ(three.state(), "preset box=9 'Custom' X box='Position East of launch' X unit=ft");
    EXPECT_EQ(three.rows(),
              "0 Position North of launch [m] Left #0072bd Solid setDefault=off reset=off | 1 "
              "Altitude [m] Right #7e318e Solid setDefault=off reset=off | 2 Time [s] Auto #77ac30 "
              "Solid setDefault=off reset=off");
    EXPECT_EQ(three.extras(), std::string("description visible=false") + kMarkerStyleOverAnother);

    simple.reopen();
    EXPECT_EQ(simple.rows(),
              "0 Position North of launch [m] Left #0a141e Solid setDefault=on reset=on | 1 "
              "Altitude [m] Right #d95319 Solid setDefault=off reset=off | 2 Time [s] Auto #edb120 "
              "Solid setDefault=off reset=off");
}

TEST_F(SimulationPlotPanelTest, WithoutARowThereIsNothingToPlot)
{
    const StoredPanel panel;
    panel.choosePreset(8);
    QTest::mouseClick(&panel.panel().plotTypeSelector(1).removeButton(), Qt::LeftButton);
    QTest::mouseClick(&panel.panel().plotTypeSelector(0).removeButton(), Qt::LeftButton);
    EXPECT_FALSE(panel.panel().doPlot(nullptr));
    const std::vector<Gui::Test::ScriptedUserPrompts::Prompt>& shown =
        panel.services().prompts().prompts();
    ASSERT_EQ(shown.size(), 1U);
    EXPECT_EQ(shown.front().kind, Gui::Test::ScriptedUserPrompts::Kind::MESSAGE);
    EXPECT_EQ(shown.front().title, QStringLiteral("Nothing to plot"));
    EXPECT_EQ(
        shown.front().texts,
        (QStringList{QStringLiteral("ERROR"),
                     QStringLiteral("Please add one or more variables to plot on the Y-axis.")}));
    EXPECT_TRUE(panel.services().simulationDialogs().requests().empty());
    EXPECT_EQ(panel.services().plotDefaults().lastPlotted(), nullptr);
}

TEST_F(SimulationPlotPanelTest, TheTabOfAFlightOfThreeStages)
{
    // probe: "== plot tab three-stage": the right axis' colours after the left axis' rows of
    // every stage; the events of every stage.
    const StoredPanel panel(ExampleDesign::kThreeStageLowPowerRocket);
    EXPECT_EQ(panel.rows(),
              "0 Altitude [m] Left #0072bd Solid setDefault=off reset=off | 1 Vertical velocity "
              "[m/s] Auto #a2142f Solid setDefault=off reset=off | 2 Vertical acceleration "
              "[m/s\u00b2] Auto #d95319 Solid setDefault=off reset=off");
    EXPECT_EQ(panel.events(),
              "12: Launch=false; Motor ignition=true; Lift-off=false; Launch rod clearance=false; "
              "Motor burnout=true; Ejection charge=false; Stage separation=true; Apogee=true; "
              "Recovery device deployment=true; Ground hit=true; Simulation end=false; "
              "Tumbling=true");
}

/// The colour SimulationPlot draws the series of row @p row of its configuration in (Java:
/// SimulationChartTest.findRenderedColor()).
[[nodiscard]] QColor renderedColorOf(const SimulationPlot& plot, std::size_t row)
{
    const std::vector<SimulationPlotModel::Series>& series = plot.model().getSeries();
    for (std::size_t i = 0; i < series.size(); ++i)
    {
        if (series[i].row == row && plot.series(i) != nullptr)
        {
            return plot.series(i)->color();
        }
    }
    ADD_FAILURE() << "No rendered series for data index " << row;
    return {};
}

TEST_F(SimulationPlotPanelTest, AutomaticColorsRefreshWhenLaterVariableChangesAxisAssignment)
{
    // Port of SimulationChartTest.automaticColorsRefreshWhenLaterVariableChangesAxisAssignment:
    // changing one auto-axis variable can alter the palette position of another variable. Every
    // automatic color shown in the configuration panel must be refreshed to match the final
    // rendered plot.
    const Gui::Test::TestGuiServices services;
    const Gui::Test::TestFlight      flight(Gui::Test::positionFlight());
    SimulationPlotPanel              panel(services.services(), flight.simulation());

    SimulationPlotConfiguration configuration("Test", type(Id::TYPE_TIME));
    configuration.addPlotDataType(type(Id::TYPE_ALTITUDE), 1);
    configuration.addPlotDataType(type(Id::TYPE_VELOCITY_Z));
    configuration.addPlotDataType(type(Id::TYPE_ACCELERATION_Z));
    panel.setConfiguration(configuration);
    panel.updatePlots();

    panel.plotTypeSelector(1).typeSelector().setSelectedType(&type(Id::TYPE_POSITION_X));
    const QColor colorBeforeThirdChange = panel.plotTypeSelector(1).selectedColor();
    panel.plotTypeSelector(2).typeSelector().setSelectedType(&type(Id::TYPE_POSITION_Y));

    const QColor         displayedColor = panel.plotTypeSelector(1).selectedColor();
    const SimulationPlot plot(services.services(), *flight.simulation(), panel.getConfiguration(),
                              false);
    const QColor         renderedColor = renderedColorOf(plot, 1);

    EXPECT_NE(colorBeforeThirdChange, displayedColor)
        << "The later variable must cause this auto-assigned palette position to change";
    EXPECT_EQ(renderedColor, displayedColor);
}

TEST_F(SimulationPlotPanelTest, TheColoursShownFollowATypeOrAnAxisOneChangeLate)
{
    // Kept as Java (probe: "== SimulationChartTest's colour test"): Swing calls
    // SimulationPlotPanel's listeners before PlotPanel's, so the colours a row's type or axis
    // change shows are those of the configuration before it; the rows made again show the
    // configuration's own.
    const Gui::Test::TestGuiServices services;
    const Gui::Test::TestFlight      flight(Gui::Test::positionFlight());
    SimulationPlotPanel              panel(services.services(), flight.simulation());
    EXPECT_EQ(Gui::Test::rowsText(panel),
              "0 Altitude [m] Left #0072bd Solid setDefault=off reset=off | 1 Vertical velocity "
              "[m/s] Auto #d95319 Solid setDefault=off reset=off | 2 Vertical acceleration "
              "[m/s\u00b2] Auto #edb120 Solid setDefault=off reset=off");
    SimulationPlotConfiguration configuration("Test", type(Id::TYPE_TIME));
    configuration.addPlotDataType(type(Id::TYPE_ALTITUDE), 1);
    configuration.addPlotDataType(type(Id::TYPE_VELOCITY_Z));
    configuration.addPlotDataType(type(Id::TYPE_ACCELERATION_Z));
    panel.setConfiguration(configuration);
    panel.updatePlots();
    EXPECT_EQ(Gui::Test::stateText(panel),
              "preset box=0 'Vertical motion vs. time' X box='Time' X unit=s");
    EXPECT_EQ(Gui::Test::rowsText(panel),
              "0 Altitude [m] Right #d95319 Solid setDefault=off reset=off | 1 Vertical velocity "
              "[m/s] Auto #edb120 Solid setDefault=off reset=off | 2 Vertical acceleration "
              "[m/s\u00b2] Auto #0072bd Solid setDefault=off reset=off");
    panel.plotTypeSelector(1).typeSelector().setSelectedType(&type(Id::TYPE_POSITION_X));
    EXPECT_EQ(Gui::Test::rowsText(panel),
              "0 Altitude [m] Right #d95319 Solid setDefault=off reset=off | 1 Position East of "
              "launch [m] Auto #edb120 Solid setDefault=off reset=off | 2 Vertical acceleration "
              "[m/s\u00b2] Auto #0072bd Solid setDefault=off reset=off");
    panel.plotTypeSelector(2).typeSelector().setSelectedType(&type(Id::TYPE_POSITION_Y));
    EXPECT_EQ(Gui::Test::rowsText(panel),
              "0 Altitude [m] Right #d95319 Solid setDefault=off reset=off | 1 Position East of "
              "launch [m] Auto #0072bd Solid setDefault=off reset=off | 2 Position North of launch "
              "[m] Auto #edb120 Solid setDefault=off reset=off");
    panel.updatePlots();
    EXPECT_EQ(Gui::Test::rowsText(panel),
              "0 Altitude [m] Right #edb120 Solid setDefault=off reset=off | 1 Position East of "
              "launch [m] Auto #0072bd Solid setDefault=off reset=off | 2 Position North of launch "
              "[m] Auto #d95319 Solid setDefault=off reset=off");

    // "again; row 1 axis -> Left".
    panel.setConfiguration(configuration);
    panel.updatePlots();
    panel.plotTypeSelector(1).axisSelector().setCurrentIndex(1);
    EXPECT_EQ(Gui::Test::rowsText(panel),
              "0 Altitude [m] Right #d95319 Solid setDefault=off reset=off | 1 Vertical velocity "
              "[m/s] Left #edb120 Solid setDefault=off reset=off | 2 Vertical acceleration "
              "[m/s\u00b2] Auto #0072bd Solid setDefault=off reset=off");
    panel.updatePlots();
    EXPECT_EQ(Gui::Test::rowsText(panel),
              "0 Altitude [m] Right #d95319 Solid setDefault=off reset=off | 1 Vertical velocity "
              "[m/s] Left #0072bd Solid setDefault=off reset=off | 2 Vertical acceleration "
              "[m/s\u00b2] Auto #edb120 Solid setDefault=off reset=off");
}

TEST_F(SimulationPlotPanelTest, ThePartsAreLaidOutAsJavasPanel)
{
    // probe: "events table: header=false showVerticalLines=false showHorizontalLines=false
    // rowSelectionAllowed=false columnSelectionAllowed=false column 0 width=22 = row height + 2:
    // true scroll preferred height=150 column 1 class=String editable 0/1=true/false"; "events
    // panel": the table "width 200lp, grow 1", All and None "gapleft para, gapright para, growx,
    // sizegroup buttons", the marker style "split 3, growx"; the description "growx, wrap"
    // after the X unit.
    StoredPanel panel;
    ASSERT_TRUE(panel.show());
    const QTableView& table = panel.panel().eventTable();
    EXPECT_EQ(table.objectName(), QStringLiteral("flightEventTable"));
    EXPECT_TRUE(table.horizontalHeader()->isHidden());
    EXPECT_TRUE(table.verticalHeader()->isHidden());
    EXPECT_FALSE(table.showGrid());
    EXPECT_EQ(table.selectionMode(), QAbstractItemView::NoSelection);
    EXPECT_EQ(table.columnWidth(FlightEventTableModel::kSelectedColumn),
              table.verticalHeader()->defaultSectionSize() + 2);
    EXPECT_EQ(table.sizeHint(),
              QSize(SimulationPlotPanel::kEventTableWidth, SimulationPlotPanel::kEventTableHeight));
    // One width ("sizegroup buttons"; the layout's rounding may give one a pixel more).
    EXPECT_EQ(panel.panel().allButton().minimumWidth(), panel.panel().noneButton().minimumWidth());
    EXPECT_LE(std::abs(panel.panel().allButton().width() - panel.panel().noneButton().width()), 1);
    EXPECT_EQ(panel.panel().allButton().mapTo(&panel.panel(), QPoint(0, 0)).x() -
                  table.mapTo(&panel.panel(), QPoint(0, 0)).x(),
              PlotTypeSelector::kParagraphGap);
    EXPECT_EQ(panel.panel().descriptionArea().objectName(), QStringLiteral("simPlotPanelDesc"));
    // Hidden, the description keeps its place (MigLayout's hidemode 0).
    EXPECT_TRUE(panel.panel().descriptionArea().sizePolicy().retainSizeWhenHidden());
    EXPECT_EQ(panel.panel().descriptionArea().parentWidget(),
              panel.panel().domainTypeSelector().parentWidget());
    // probe: "x axis geometry: type width=200 gap type-'Unit:'=12 gap 'Unit:'-unit=6 unit
    // width=40 gap unit-description=12" (one split cell).
    const QWidget& unit = panel.panel().domainUnitSelector();
    EXPECT_EQ(panel.panel().descriptionArea().x() - (unit.x() + unit.width()),
              PlotTypeSelector::kUnrelatedGap);
    EXPECT_GE(unit.width(), PlotTypeSelector::kUnitSelectorWidth);
}

TEST_F(SimulationPlotPanelTest, ASimulationWithoutDataHasNoTab)
{
    // Java's create(): IllegalArgumentException "Simulation contains no data.".
    const Gui::Test::TestGuiServices services;
    const Gui::Test::TestFlight      flight(nullptr);
    EXPECT_THROW(SimulationPlotPanel(services.services(), flight.simulation()), BugError);
    EXPECT_THROW(SimulationPlotPanel(services.services(), nullptr), BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
