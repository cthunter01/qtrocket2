// The plot configuration panel (OpenRocket's PlotPanel) over a few types and two presets of its
// own: the preset box, the X axis, the rows, New Y axis plot type and the remove buttons, as the
// generic panel has them (SimulationPlotPanelTests.cpp has the simulations' tab, with OpenRocket's
// own values of a stored flight). PlotPanel's own behaviour, as PlotPanel.java has it:
// setConfiguration() drops the types the panel does not offer, every edit but a unit or an axis
// makes the configuration "Custom", and choosing "Custom" changes nothing.

#include "QtRocket/gui/dialogs/plot/PlotPanel.h"

#include <cstddef>
#include <string>
#include <vector>

#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QPoint>
#include <QPushButton>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/dialogs/plot/PlotTypeSelector.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/DataTypeComboBox.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"
#include "dialogs/plot/PlotPanelTestSupport.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using PlotPanelTest = Gui::Test::GuiTest;
using Id            = FlightDataTypeId;

[[nodiscard]] const FlightDataType& type(Id id)
{
    return FlightDataType::builtin(id);
}

/// The panel's types: time, altitude, vertical velocity and the Mach number.
[[nodiscard]] PlotPanel::DataTypes fourTypes()
{
    return {&type(Id::TYPE_TIME), &type(Id::TYPE_ALTITUDE), &type(Id::TYPE_VELOCITY_Z),
            &type(Id::TYPE_MACH_NUMBER)};
}

/// The presets: "Altitude" (altitude over time, on the left) and "Velocity" (the vertical velocity
/// over the Mach number, on the right, in km/h: the panel chooses it in the default units), then
/// "Custom".
[[nodiscard]] PlotPanel::Configurations twoPresets()
{
    SimulationPlotConfiguration altitude("Altitude", type(Id::TYPE_TIME));
    altitude.addPlotDataType(type(Id::TYPE_ALTITUDE), 0);
    SimulationPlotConfiguration velocity("Velocity", type(Id::TYPE_MACH_NUMBER));
    velocity.addPlotDataType(type(Id::TYPE_VELOCITY_Z), 1);
    const Unit* kmh = type(Id::TYPE_VELOCITY_Z).getUnitGroup().getUnit("km/h");
    QTROCKET_ASSERT(kmh != nullptr);
    velocity.setPlotDataUnit(0, *kmh);
    return {altitude, velocity, SimulationPlotConfiguration::customConfiguration()};
}

/// A panel over fourTypes() and twoPresets(), starting from @p start, its rows shown.
class Panel
{
public:
    explicit Panel(const SimulationPlotConfiguration& start)
      : m_panel(m_services.services(), fourTypes(), fourTypes(), twoPresets(), start)
    {
        m_panel.updatePlots();
    }
    Panel() : Panel(twoPresets().front()) { }

    [[nodiscard]] Gui::Test::TestGuiServices& services() noexcept { return m_services; }
    [[nodiscard]] PlotPanel&                  panel() noexcept { return m_panel; }

    /// The configuration's name and its rows, "<type> [<unit>] @<axis>" each.
    [[nodiscard]] std::string configuration() const
    {
        const SimulationPlotConfiguration& configuration = m_panel.getConfiguration();
        std::string text = configuration.getName() + " over " +
                           configuration.getDomainAxisType().getName() + " [" +
                           configuration.getDomainAxisUnit().getUnit() + "]:";
        for (std::size_t i = 0; i < configuration.getDataCount(); ++i)
        {
            text += " " + configuration.getType(i).getName() + " [" +
                    configuration.getUnit(i).getUnit() + "] @" +
                    std::to_string(configuration.getAxis(i));
        }
        return text;
    }

    /// The rows shown, "<type> [<unit>] <axis box's text>" each.
    [[nodiscard]] std::string rows() const
    {
        std::string text;
        for (std::size_t i = 0; i < m_panel.selectorCount(); ++i)
        {
            const PlotTypeSelector& row = m_panel.selector(i);
            text += (i == 0 ? "" : " | ") + row.selectedType()->getName() + " [" +
                    row.selectedUnit().getUnit() + "] " +
                    row.axisSelector().currentText().toStdString();
        }
        return text;
    }

private:
    Gui::Test::TestGuiServices m_services;
    PlotPanel                  m_panel;
};

TEST_F(PlotPanelTest, ThePresetBoxStartsAtThePresetOfTheConfigurationsName)
{
    Panel            panel;
    const QComboBox& presets = panel.panel().configurationSelector();
    EXPECT_EQ(presets.objectName(), QStringLiteral("configurationSelector"));
    ASSERT_EQ(presets.count(), 3);
    EXPECT_EQ(presets.itemText(0), QStringLiteral("Altitude"));
    EXPECT_EQ(presets.itemText(1), QStringLiteral("Velocity"));
    EXPECT_EQ(presets.itemText(2), QStringLiteral("Custom"));
    EXPECT_EQ(panel.panel().customIndex(), 2U);
    EXPECT_EQ(presets.currentIndex(), 0);
    EXPECT_EQ(panel.configuration(), "Altitude over Time [s]: Altitude [m] @0");
    EXPECT_EQ(panel.rows(), "Altitude [m] Left");

    // Another name: its preset; none of them: the first (DefaultComboBoxModel's first item).
    Panel velocity(twoPresets().at(1));
    EXPECT_EQ(velocity.panel().configurationSelector().currentIndex(), 1);
    // The configuration given keeps its units (only a choice in the box resets them).
    EXPECT_EQ(velocity.configuration(),
              "Velocity over Mach number [\u200b]: Vertical velocity [km/h] @1");
    const SimulationPlotConfiguration other("Something else", type(Id::TYPE_TIME));
    Panel                             unnamed(other);
    EXPECT_EQ(unnamed.panel().configurationSelector().currentIndex(), 0);
    EXPECT_EQ(unnamed.configuration(), "Something else over Time [s]:");
    Panel custom(SimulationPlotConfiguration::customConfiguration());
    EXPECT_EQ(custom.panel().configurationSelector().currentIndex(), 2);
}

TEST_F(PlotPanelTest, ThePanelDropsTheTypesItDoesNotOffer)
{
    // setConfiguration(): the domain becomes the first X type, rows of other types go, and the
    // name becomes "Custom"; the preset box keeps its choice.
    SimulationPlotConfiguration foreign("Altitude", type(Id::TYPE_POSITION_X));
    foreign.addPlotDataType(type(Id::TYPE_ALTITUDE), 0);
    foreign.addPlotDataType(type(Id::TYPE_THRUST_FORCE), 1);
    foreign.addPlotDataType(type(Id::TYPE_VELOCITY_Z), -1);
    Panel panel(foreign);
    EXPECT_EQ(panel.configuration(),
              "Custom over Time [s]: Altitude [m] @0 Vertical velocity [m/s] @-1");
    EXPECT_EQ(panel.panel().getConfiguration().getNameKey(), "simplotpanel.CUSTOM");
    EXPECT_EQ(panel.panel().configurationSelector().currentIndex(), 2);

    const QSignalSpy changed(&panel.panel(), &PlotPanel::configurationChanged);
    panel.panel().setConfiguration(foreign);
    EXPECT_EQ(changed.count(), 1);
    panel.panel().setConfiguration(twoPresets().at(1));
    EXPECT_EQ(changed.count(), 2);
    // Kept as Java: the preset box shows what it showed; the rows come with updatePlots().
    EXPECT_EQ(panel.panel().configurationSelector().currentIndex(), 2);
    EXPECT_EQ(panel.rows(), "Altitude [m] Left | Vertical velocity [m/s] Auto");
    panel.panel().updatePlots();
    EXPECT_EQ(panel.rows(), "Vertical velocity [km/h] Right");
    EXPECT_EQ(panel.panel().domainTypeSelector().selectedType(), &type(Id::TYPE_MACH_NUMBER));
}

TEST_F(PlotPanelTest, ChoosingAPresetCopiesItInTheDefaultUnits)
{
    Panel panel;
    panel.panel().configurationSelector().setCurrentIndex(1);
    EXPECT_EQ(panel.configuration(),
              "Velocity over Mach number [\u200b]: Vertical velocity [m/s] @1");
    EXPECT_EQ(panel.rows(), "Vertical velocity [m/s] Right");
    EXPECT_EQ(panel.panel().domainTypeSelector().selectedType(), &type(Id::TYPE_MACH_NUMBER));
    EXPECT_EQ(&panel.panel().domainUnitSelector().getUnitGroup(),
              &type(Id::TYPE_MACH_NUMBER).getUnitGroup());

    // "Custom" changes nothing, not even the name.
    panel.panel().configurationSelector().setCurrentIndex(2);
    EXPECT_EQ(panel.configuration(),
              "Velocity over Mach number [\u200b]: Vertical velocity [m/s] @1");
    panel.panel().configurationSelector().setCurrentIndex(0);
    EXPECT_EQ(panel.configuration(), "Altitude over Time [s]: Altitude [m] @0");
    EXPECT_EQ(panel.rows(), "Altitude [m] Left");
}

TEST_F(PlotPanelTest, NewYAxisPlotTypeAddsTheFirstTypeNotInUse)
{
    Panel panel;
    QTest::mouseClick(&panel.panel().newYAxisButton(), Qt::LeftButton);
    // Time is the X type and Altitude a row's: the vertical velocity, on an automatic axis.
    EXPECT_EQ(panel.configuration(),
              "Custom over Time [s]: Altitude [m] @0 Vertical velocity [m/s] @-1");
    EXPECT_EQ(panel.panel().configurationSelector().currentIndex(), 2);
    EXPECT_EQ(panel.rows(), "Altitude [m] Left | Vertical velocity [m/s] Auto");
    panel.panel().newYAxisPlotType();
    panel.panel().newYAxisPlotType();
    // Every type in use: the first.
    EXPECT_EQ(panel.configuration(),
              "Custom over Time [s]: Altitude [m] @0 Vertical velocity "
              "[m/s] @-1 Mach number [\u200b] @-1 Time [s] @-1");
    EXPECT_EQ(panel.panel().selectorCount(), 4U);
    EXPECT_TRUE(panel.services().prompts().prompts().empty());
}

/// Adds @p count rows to @p panel (New Y axis plot type).
void addRows(PlotPanel& panel, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i)
    {
        panel.newYAxisPlotType();
    }
}

TEST_F(PlotPanelTest, FifteenRowsAreTheMost)
{
    Panel panel;
    addRows(panel.panel(), PlotPanel::kMaximumPlots - 1);
    ASSERT_EQ(panel.panel().getConfiguration().getDataCount(), PlotPanel::kMaximumPlots);
    EXPECT_TRUE(panel.services().prompts().prompts().empty());

    QTest::mouseClick(&panel.panel().newYAxisButton(), Qt::LeftButton);
    EXPECT_EQ(panel.panel().getConfiguration().getDataCount(), PlotPanel::kMaximumPlots);
    EXPECT_EQ(panel.panel().selectorCount(), PlotPanel::kMaximumPlots);
    const std::vector<Gui::Test::ScriptedUserPrompts::Prompt>& shown =
        panel.services().prompts().prompts();
    ASSERT_EQ(shown.size(), 1U);
    EXPECT_EQ(shown.front().kind, Gui::Test::ScriptedUserPrompts::Kind::MESSAGE);
    EXPECT_EQ(shown.front().title, QStringLiteral("Cannot add plot"));
    EXPECT_EQ(shown.front().texts,
              (QStringList{QStringLiteral("ERROR"),
                           QStringLiteral("A maximum of 15 plots is allowed.")}));
}

TEST_F(PlotPanelTest, ARowsTypeMakesTheConfigurationCustomItsUnitAndAxisDoNot)
{
    Panel panel;
    panel.panel().newYAxisPlotType();
    panel.panel().configurationSelector().setCurrentIndex(0);
    const PlotTypeSelector& row = panel.panel().selector(0);

    const Unit* feet = type(Id::TYPE_ALTITUDE).getUnitGroup().getUnit("ft");
    ASSERT_NE(feet, nullptr);
    row.unitSelector().setSelectedUnit(*feet);
    row.axisSelector().setCurrentIndex(2);
    EXPECT_EQ(panel.configuration(), "Altitude over Time [s]: Altitude [ft] @1");
    EXPECT_EQ(panel.panel().configurationSelector().currentIndex(), 0);

    // Another type of the same unit group keeps the unit; another group gives its default unit.
    row.typeSelector().setSelectedType(&type(Id::TYPE_VELOCITY_Z));
    EXPECT_EQ(panel.configuration(), "Custom over Time [s]: Vertical velocity [m/s] @1");
    EXPECT_EQ(&row.unitSelector().getUnitGroup(), &type(Id::TYPE_VELOCITY_Z).getUnitGroup());
    EXPECT_EQ(panel.panel().configurationSelector().currentIndex(), 2);
    // The rows stay as they are (no updatePlots()).
    EXPECT_EQ(&panel.panel().selector(0), &row);
}

TEST_F(PlotPanelTest, TheRemoveButtonsRemoveTheirRowsTheLastOneToo)
{
    Panel panel;
    panel.panel().newYAxisPlotType();
    panel.panel().newYAxisPlotType();
    panel.panel().configurationSelector().setCurrentIndex(0);
    panel.panel().newYAxisPlotType();
    QTest::mouseClick(&panel.panel().selector(0).removeButton(), Qt::LeftButton);
    EXPECT_EQ(panel.configuration(), "Custom over Time [s]: Vertical velocity [m/s] @-1");
    EXPECT_EQ(panel.rows(), "Vertical velocity [m/s] Auto");
    EXPECT_EQ(panel.panel().selector(0).index(), 0U);
    // OpenRocket fails here with an internal error (IndexOutOfBoundsException in
    // PlotConfiguration.getGoodnessValue(), fixed in the core): the configuration has no row.
    QTest::mouseClick(&panel.panel().selector(0).removeButton(), Qt::LeftButton);
    EXPECT_EQ(panel.configuration(), "Custom over Time [s]:");
    EXPECT_EQ(panel.panel().selectorCount(), 0U);
    EXPECT_THROW(static_cast<void>(panel.panel().selector(0)), BugError);
    // The old rows go with the next turn of the event loop's deletions.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(panel.panel().typeSelectorPanel().findChildren<PlotTypeSelector*>().isEmpty());
}

TEST_F(PlotPanelTest, TheXAxisTypeMakesTheConfigurationCustomItsUnitDoesNot)
{
    Panel       panel;
    const Unit* minutes = type(Id::TYPE_TIME).getUnitGroup().getUnit("min");
    ASSERT_NE(minutes, nullptr);
    panel.panel().domainUnitSelector().setSelectedUnit(*minutes);
    EXPECT_EQ(panel.configuration(), "Altitude over Time [min]: Altitude [m] @0");
    panel.panel().domainTypeSelector().setSelectedType(&type(Id::TYPE_ALTITUDE));
    EXPECT_EQ(panel.configuration(), "Custom over Altitude [m]: Altitude [m] @0");
    EXPECT_EQ(&panel.panel().domainUnitSelector().getUnitGroup(),
              &type(Id::TYPE_ALTITUDE).getUnitGroup());
    EXPECT_EQ(panel.panel().configurationSelector().currentIndex(), 2);
}

TEST_F(PlotPanelTest, TheControlsAreLaidOutAsJavasPanel)
{
    // probe: "panel layout='fill, ins n n 0 n'": the preset line ("spanx, split", the box
    // "growx, wrap 20lp"), a separator, the X axis line, the rows' panel ("pushy, wmin 400lp,
    // grow 100"; with no extra Y widgets "spanx"), New Y axis plot type ("spanx, pushx, left");
    // "panel geometry: gap preset box-separator=20".
    const Gui::Test::TestGuiServices services;
    QWidget                          window;
    auto* panel  = makeChild<PlotPanel>(&window, services.services(), fourTypes(), fourTypes(),
                                        twoPresets(), twoPresets().front());
    auto* layout = makeChild<QVBoxLayout>(&window);
    layout->addWidget(panel);
    layout->activate();
    window.resize(800, 500);
    ASSERT_TRUE(Gui::Test::showActive(window));
    const auto* separator = panel->findChild<QFrame*>(QStringLiteral("separator"));
    ASSERT_NE(separator, nullptr);
    const QWidget& presets = panel->configurationSelector();
    EXPECT_EQ(separator->mapTo(panel, QPoint(0, 0)).y() -
                  presets.parentWidget()->mapTo(panel, QPoint(0, 0)).y() -
                  presets.parentWidget()->height(),
              PlotPanel::kPresetGap);
    const QWidget* rows = panel->findChild<QWidget*>(QStringLiteral("yPanel"));
    ASSERT_NE(rows, nullptr);
    EXPECT_EQ(rows->minimumWidth(), PlotPanel::kYPanelMinimumWidth);
    EXPECT_EQ(panel->findChild<QWidget*>(QStringLiteral("extraYPanel")), nullptr);
    EXPECT_EQ(panel->newYAxisButton().objectName(), QStringLiteral("newYAxisBtn"));
    EXPECT_EQ(panel->newYAxisButton().text(), QStringLiteral("New Y axis plot type"));
    EXPECT_EQ(panel->newYAxisButton().mapTo(panel, QPoint(0, 0)).x(),
              presets.parentWidget()->mapTo(panel, QPoint(0, 0)).x());
    EXPECT_EQ(panel->domainTypeSelector().objectName(), QStringLiteral("domainTypeSelector"));
    EXPECT_EQ(panel->domainUnitSelector().objectName(), QStringLiteral("domainUnitSelector"));
    EXPECT_EQ(panel->typeSelectorPanel().objectName(), QStringLiteral("typeSelectorPanel"));
    // No row before updatePlots() (Java's PlotPanel never calls it).
    EXPECT_EQ(panel->selectorCount(), 0U);
}

TEST_F(PlotPanelTest, APanelNeedsTypesAndPresets)
{
    const Gui::Test::TestGuiServices services;
    EXPECT_THROW(
        PlotPanel(services.services(), {}, fourTypes(), twoPresets(), twoPresets().front()),
        BugError);
    EXPECT_THROW(
        PlotPanel(services.services(), fourTypes(), {nullptr}, twoPresets(), twoPresets().front()),
        BugError);
    EXPECT_THROW(PlotPanel(services.services(), fourTypes(), fourTypes(), {}, twoPresets().front()),
                 BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
