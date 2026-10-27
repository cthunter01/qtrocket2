// OpenRocket's "Motors & Configuration" tables (FlightConfigurationTablesProbe, OpenRocket
// 5f164fd0e: the table models MotorConfigurationPanel, RecoveryConfigurationPanel and
// SeparationConfigurationPanel make and their cell renderers, and MotorMountTableModel, headless
// with SwingPreferences and FlatLaf Light) over the 16 examples and the 13 test rockets, against
// QtRocket's FlightConfigurableTableModel (its three kinds) and MotorMountTableModel over the same
// designs: the rows and their configuration ids, every header, every cell's texts and whether it
// is shaded, the motor mounts with their check boxes, and column 0 after a rename. The pins are
// FlightConfigurationTablePins.h, generated from the probe's output.
//
// A design is loaded as the probe loads it: an example as loaded (not settled: no text depends on
// the automatic dimensions), without component presets, its motors from the bundled database; a
// test rocket from its maker. The texts take the default preferences (the motors' designations,
// the default configuration name "[{motors}]") and the default units.

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QColor>
#include <QFont>
#include <QModelIndex>
#include <QString>
#include <QVariant>
#include <Qt>

#include <gtest/gtest.h>

#include "FlightConfigurationTablePins.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/flightconfig/FlightConfigurableTableModel.h"
#include "QtRocket/gui/flightconfig/MotorMountTableModel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Error.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenExamples.h"
#include "rocket/TestRockets.h"

namespace QtRocket::Gui
{

namespace
{

using Kind = FlightConfigurableTableModel::Kind;
using Test::CellStylePin;
using Test::ConfigurationRowPin;
using Test::FlightConfigurationTablesPin;
using Test::MotorCellPin;
using Test::MotorMountRowPin;
using Test::NamePin;
using Test::ValueCellPin;

/// The design of a scenario as the probe made it: an example loaded without presets, or a
/// TestRockets maker.
class TablesDesign
{
public:
    explicit TablesDesign(const FlightConfigurationTablesPin& pin)
    {
        if (pin.source == "example")
        {
            loadExample(pin.design);
        }
        else if (pin.source == "testrocket")
        {
            for (const QtRocket::Test::TestRocketMaker& maker : QtRocket::Test::testRocketMakers())
            {
                if (maker.method == pin.design)
                {
                    m_rocket            = maker.make();
                    m_randomFirstConfig = maker.randomConfigurationId;
                }
            }
        }
        if (!isLoaded())
        {
            ADD_FAILURE() << "no design " << pin.name;
        }
    }

    [[nodiscard]] bool isLoaded() const noexcept
    {
        return m_rocket != nullptr || m_document != nullptr;
    }

    [[nodiscard]] Rocket& rocket() const
    {
        return m_rocket != nullptr ? *m_rocket : m_document->getRocket();
    }

    /// Whether QtRocket's maker of a test rocket draws its configuration's id (TestRocketMaker).
    [[nodiscard]] bool drawsItsConfigurationId() const noexcept { return m_randomFirstConfig; }

private:
    void loadExample(std::string_view file)
    {
        const QtRocket::Test::GoldenExample* example = QtRocket::Test::findGoldenExample(file);
        if (example == nullptr)
        {
            return;
        }
        Result<LoadedDocument> loaded =
            QtRocket::Test::loadGoldenExample(*example, QtRocket::Test::ExampleSource::ORIGINAL,
                                              QtRocket::Test::DesignFileEnvironment::Presets::NONE);
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
        m_document = std::move(loaded->document);
    }

    std::unique_ptr<OpenRocketDocument> m_document;
    std::unique_ptr<Rocket>             m_rocket;
    bool                                m_randomFirstConfig{false};
};

/// @p text of the pins as a QString.
QString text(std::string_view utf8)
{
    return QtStrings::fromUtf8(utf8);
}

/// The headers of @p table as Java's @p headers.
void expectHeadersAsJava(const FlightConfigurableTableModel& table,
                         std::span<const std::string_view>   headers)
{
    ASSERT_EQ(static_cast<std::size_t>(table.columnCount()), headers.size());
    for (std::size_t column = 0; column < headers.size(); ++column)
    {
        EXPECT_EQ(table.headerData(static_cast<int>(column), Qt::Horizontal).toString(),
                  text(headers[column]))
            << column;
    }
}

/// Column 0 of @p table: the rows' names, regular.
void expectNamesAsJava(const FlightConfigurableTableModel&  table,
                       std::span<const ConfigurationRowPin> rows)
{
    ASSERT_EQ(static_cast<std::size_t>(table.rowCount()), rows.size());
    for (std::size_t row = 0; row < rows.size(); ++row)
    {
        const QModelIndex index = table.index(static_cast<int>(row), 0);
        EXPECT_EQ(table.data(index).toString(), text(rows[row].name)) << row;
        EXPECT_FALSE(table.data(index, FlightConfigurableTableModel::kDefaultRole).toBool()) << row;
        EXPECT_FALSE(table.data(index, Qt::FontRole).isValid()) << row;
    }
}

/// The motor cell of @p table at @p pin as Java's: the motor label always regular, the ignition
/// shaded where Java shades it.
void expectMotorCellAsJava(const FlightConfigurableTableModel& table, const MotorCellPin& pin)
{
    SCOPED_TRACE(testing::Message() << "row " << pin.row << ", column " << pin.column);
    const QModelIndex index = table.index(pin.row, pin.column);
    EXPECT_EQ(table.data(index).toString(), text(pin.motor));
    EXPECT_EQ(table.data(index, FlightConfigurableTableModel::kIgnitionTextRole).toString(),
              text(pin.ignition));
    const bool shaded = pin.ignitionStyle == CellStylePin::SHADED;
    EXPECT_EQ(table.data(index, FlightConfigurableTableModel::kDefaultRole).toBool(), shaded);
    // Java shades the ignition exactly when the cell's configuration has no override.
    EXPECT_EQ(shaded, !pin.ignitionOverride);
    EXPECT_FALSE(table.data(index, Qt::FontRole).isValid());
}

/// The motor cells of @p table as Java's @p cells, in row order.
void expectMotorCellsAsJava(const FlightConfigurableTableModel& table,
                            std::span<const MotorCellPin>       cells)
{
    ASSERT_EQ(cells.size(), static_cast<std::size_t>(table.rowCount() * (table.columnCount() - 1)));
    for (const MotorCellPin& pin : cells)
    {
        expectMotorCellAsJava(table, pin);
    }
}

/// The recovery or stage cell of @p table at @p pin as Java's. QtRocket shades a cell whose
/// configuration has no override of its own (!containsId(), OpenRocket bug 1 fixed), where Java
/// shades by isDefault(), which is false for such a cell.
void expectValueCellAsJava(const FlightConfigurableTableModel& table, const ValueCellPin& pin)
{
    SCOPED_TRACE(testing::Message() << "row " << pin.row << ", column " << pin.column);
    const QModelIndex index = table.index(pin.row, pin.column);
    EXPECT_EQ(table.data(index).toString(), text(pin.text));
    // Java's rule, as the probe saw it.
    EXPECT_EQ(pin.style == CellStylePin::SHADED, pin.isDefault);
    const bool shaded = !pin.containsId;
    EXPECT_EQ(table.data(index, FlightConfigurableTableModel::kDefaultRole).toBool(), shaded);
    EXPECT_EQ(qvariant_cast<QFont>(table.data(index, Qt::FontRole)).italic(), shaded);
    EXPECT_EQ(table.data(index, Qt::ForegroundRole).isValid(), shaded);
}

/// The recovery or stage cells of @p table as Java's @p cells, in row order.
void expectValueCellsAsJava(const FlightConfigurableTableModel& table,
                            std::span<const ValueCellPin>       cells)
{
    ASSERT_EQ(cells.size(), static_cast<std::size_t>(table.rowCount() * (table.columnCount() - 1)));
    for (const ValueCellPin& pin : cells)
    {
        expectValueCellAsJava(table, pin);
    }
}

/// The configuration of the row @p id of @p design as Java's @p java: its key (unless the maker
/// draws it: compared by row only), raw name and override of the default name.
void expectRowAsJava(const TablesDesign& design, const FlightConfigurationId& id,
                     const ConfigurationRowPin& java, const Preferences& preferences)
{
    if (java.drawn)
    {
        EXPECT_TRUE(design.drawsItsConfigurationId());
    }
    else
    {
        EXPECT_EQ(id.key().toString(), java.key);
    }
    const FlightConfiguration& configuration = design.rocket().getFlightConfiguration(id);
    EXPECT_EQ(configuration.getNameRaw(preferences), java.rawName);
    EXPECT_EQ(configuration.isNameOverridden(preferences), java.nameOverridden);
}

/// The rows of @p table over @p design as Java's @p rows.
void expectRowsAsJava(const TablesDesign& design, const FlightConfigurableTableModel& table,
                      std::span<const ConfigurationRowPin> rows, const Preferences& preferences)
{
    ASSERT_EQ(table.configurationIds().size(), rows.size());
    for (std::size_t row = 0; row < rows.size(); ++row)
    {
        SCOPED_TRACE(row);
        expectRowAsJava(design, table.configurationIds()[row], rows[row], preferences);
    }
}

/// The motor mount table of @p rocket as Java's @p rows.
void expectMotorMountsAsJava(Rocket& rocket, std::span<const MotorMountRowPin> rows)
{
    const MotorMountTableModel mounts(rocket);
    ASSERT_EQ(static_cast<std::size_t>(mounts.rowCount()), rows.size());
    for (std::size_t row = 0; row < rows.size(); ++row)
    {
        const int r = static_cast<int>(row);
        EXPECT_EQ(
            mounts
                .data(mounts.index(r, MotorMountTableModel::kMotorMountColumn), Qt::CheckStateRole)
                .toInt(),
            static_cast<int>(rows[row].motorMount ? Qt::Checked : Qt::Unchecked))
            << row;
        EXPECT_EQ(mounts.data(mounts.index(r, MotorMountTableModel::kNameColumn)).toString(),
                  text(rows[row].name))
            << row;
    }
}

[[nodiscard]] std::string scenarioName(
    const ::testing::TestParamInfo<FlightConfigurationTablesPin>& param)
{
    return std::string(param.param.testName);
}

class FlightConfigurationTablesGolden
  : public Gui::Test::GuiTest,
    public ::testing::WithParamInterface<FlightConfigurationTablesPin>
{ };

class FlightConfigurationNamesGolden
  : public Gui::Test::GuiTest,
    public ::testing::WithParamInterface<FlightConfigurationTablesPin>
{ };

/// The scenarios with a configuration to rename: all but the three test rockets that have none.
[[nodiscard]] std::vector<FlightConfigurationTablesPin> namedScenarios()
{
    std::vector<FlightConfigurationTablesPin> scenarios;
    for (const FlightConfigurationTablesPin& pin : Gui::Test::kFlightConfigurationTables)
    {
        if (!pin.names.empty())
        {
            scenarios.push_back(pin);
        }
    }
    return scenarios;
}

TEST(FlightConfigurationTablePinsGolden, ComeFromTheGoldensOpenRocket)
{
    EXPECT_EQ(Gui::Test::kFlightConfigurationTablePinsCommit,
              "5f164fd0e05d6e188857b0185601293b7a6e9eaf");
    EXPECT_EQ(Gui::Test::kFlightConfigurationTables.size(), 29U);
    // The two colours of Java's styles are ThemeColors' light ones.
    EXPECT_EQ(ThemeColors::themeColor(ThemeColors::Key::TEXT, ThemeColors::Theme::LIGHT).rgb() &
                  0xffffffU,
              Gui::Test::kJavaTextColour);
    EXPECT_EQ(ThemeColors::themeColor(ThemeColors::Key::TEXT_DIM, ThemeColors::Theme::LIGHT).rgb() &
                  0xffffffU,
              Gui::Test::kJavaDimTextColour);
}

TEST_P(FlightConfigurationTablesGolden, ShowsTheTablesAsOpenRocket)
{
    const FlightConfigurationTablesPin& pin = GetParam();
    const TablesDesign                  design(pin);
    ASSERT_TRUE(design.isLoaded());
    Rocket&                   rocket = design.rocket();
    const InMemoryPreferences preferences;
    EXPECT_EQ(static_cast<int>(rocket.getStageCount()), pin.stageCount);

    const FlightConfigurableTableModel motors(Kind::MOTOR_MOUNTS, rocket, preferences);
    expectRowsAsJava(design, motors, pin.configurations, preferences);
    EXPECT_EQ(motors.rowOf(rocket.getSelectedConfiguration().getId()), pin.selectedRow);

    {
        SCOPED_TRACE("motors");
        expectHeadersAsJava(motors, pin.motorHeaders);
        expectNamesAsJava(motors, pin.configurations);
        expectMotorCellsAsJava(motors, pin.motorCells);
    }
    {
        SCOPED_TRACE("recovery");
        const FlightConfigurableTableModel recovery(Kind::RECOVERY_DEVICES, rocket, preferences);
        expectHeadersAsJava(recovery, pin.recoveryHeaders);
        expectNamesAsJava(recovery, pin.configurations);
        expectValueCellsAsJava(recovery, pin.recoveryCells);
    }
    {
        SCOPED_TRACE("stages");
        const FlightConfigurableTableModel stages(Kind::STAGES, rocket, preferences);
        expectHeadersAsJava(stages, pin.stageHeaders);
        expectNamesAsJava(stages, pin.configurations);
        expectValueCellsAsJava(stages, pin.stageCells);
    }
    {
        SCOPED_TRACE("motor mounts");
        expectMotorMountsAsJava(rocket, pin.motorMounts);
    }
}

TEST_P(FlightConfigurationNamesGolden, NamesARenamedConfigurationAsOpenRocket)
{
    const FlightConfigurationTablesPin& pin = GetParam();
    const TablesDesign                  design(pin);
    ASSERT_TRUE(design.isLoaded());
    Rocket&                            rocket = design.rocket();
    const InMemoryPreferences          preferences;
    const FlightConfigurableTableModel table(Kind::MOTOR_MOUNTS, rocket, preferences);
    const int                          last = table.rowCount() - 1;
    ASSERT_GE(last, 0);
    FlightConfiguration& configuration = rocket.getFlightConfiguration(
        table.configurationId(last).value_or(FlightConfigurationId()));
    for (const NamePin& name : pin.names)
    {
        SCOPED_TRACE(name.raw);
        configuration.setName(name.raw);
        EXPECT_EQ(table.data(table.index(last, 0)).toString(), text(name.text));
    }
}

INSTANTIATE_TEST_SUITE_P(Scenarios, FlightConfigurationTablesGolden,
                         ::testing::ValuesIn(Gui::Test::kFlightConfigurationTables), scenarioName);
INSTANTIATE_TEST_SUITE_P(Scenarios, FlightConfigurationNamesGolden,
                         ::testing::ValuesIn(namedScenarios()), scenarioName);

}  // namespace

}  // namespace QtRocket::Gui
