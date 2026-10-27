#include "QtRocket/gui/flightconfig/FlightConfigurableTableModel.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <QAbstractItemModel>
#include <QAbstractItemModelTester>
#include <QColor>
#include <QFont>
#include <QModelIndex>
#include <QObject>
#include <QSignalSpy>
#include <QString>
#include <QVariant>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"
#include "ScopedApplicationPalette.h"
#include "rocket/TestRockets.h"

namespace QtRocket::Gui
{

namespace
{

using Kind = FlightConfigurableTableModel::Kind;
using QtRocket::Test::testFcid;

/// OpenRocket's makeBeta in a document: two stages (the booster below the sustainer), a motor
/// mount in each (the booster's with a motor in TEST_FCID_1 only), the sustainer's body tube not
/// a motor mount, a parachute, the five test configurations. Its texts are those of the
/// flight-config pins (testrocket:makeBeta).
class FlightConfigurableTableModelTest : public Gui::Test::GuiTest
{
protected:
    void SetUp() override
    {
        Gui::Test::GuiTest::SetUp();
        QtRocket::Test::TestBeta beta;
        m_sustainerBody = beta.body->getId();
        m_sustainerMmt  = beta.inner->getId();
        m_boosterStage  = beta.boosterStage->getId();
        m_boosterMmt    = beta.boosterMmt->getId();
        m_parachute     = beta.chute->getId();
        m_document = OpenRocketDocumentFactory::createDocumentFromRocket(std::move(beta.rocket));
    }

    void TearDown() override
    {
        m_document.reset();
        Gui::Test::GuiTest::TearDown();
    }

    [[nodiscard]] Rocket& rocket() const { return m_document->getRocket(); }

    /// The component @p id of the rocket as it is now (an undo replaces every component).
    template <class Component>
    [[nodiscard]] Component& find(const Uuid& id) const
    {
        auto* component = dynamic_cast<Component*>(rocket().findComponent(id));
        if (component == nullptr)
        {
            bug("the test design has no component " + id.toString());
        }
        return *component;
    }

    InMemoryPreferences                 m_preferences;
    std::unique_ptr<OpenRocketDocument> m_document;
    Uuid                                m_sustainerBody;
    Uuid                                m_sustainerMmt;
    Uuid                                m_boosterStage;
    Uuid                                m_boosterMmt;
    Uuid                                m_parachute;
};

/// The header texts of @p model, column 0 included.
std::vector<QString> headers(const QAbstractItemModel& model)
{
    std::vector<QString> texts;
    for (int column = 0; column < model.columnCount(); ++column)
    {
        texts.push_back(model.headerData(column, Qt::Horizontal).toString());
    }
    return texts;
}

TEST_F(FlightConfigurableTableModelTest, HasARowPerConfigurationWithoutTheDefault)
{
    const FlightConfigurableTableModel table(Kind::MOTOR_MOUNTS, rocket(), m_preferences);
    ASSERT_EQ(table.rowCount(), 5);
    EXPECT_EQ(std::vector(table.configurationIds().begin(), table.configurationIds().end()),
              rocket().getIds());
    EXPECT_EQ(table.configurationId(3), testFcid(3));
    EXPECT_EQ(table.configurationId(5), std::nullopt);
    EXPECT_EQ(table.configurationId(-1), std::nullopt);
    EXPECT_EQ(table.rowOf(testFcid(4)), 4);
    EXPECT_EQ(table.rowOf(FlightConfigurationId::defaultValueId()), -1);
    EXPECT_EQ(table.rowCount(table.index(0, 0)), 0);
    EXPECT_EQ(table.columnCount(table.index(0, 0)), 0);
}

TEST_F(FlightConfigurableTableModelTest, HasAColumnPerActingMotorMount)
{
    const FlightConfigurableTableModel table(Kind::MOTOR_MOUNTS, rocket(), m_preferences);
    EXPECT_EQ(headers(table), (std::vector<QString>{QStringLiteral("Configuration"),
                                                    QStringLiteral("Motor Mount Tube"),
                                                    QStringLiteral("Booster MMT")}));
    EXPECT_EQ(table.componentId(0), std::nullopt);
    EXPECT_EQ(table.componentId(1), m_sustainerMmt);
    EXPECT_EQ(table.componentId(2), m_boosterMmt);
    EXPECT_EQ(table.componentId(3), std::nullopt);
    EXPECT_EQ(table.component(2), &find<InnerTube>(m_boosterMmt));
    EXPECT_EQ(table.component(0), nullptr);
    EXPECT_EQ(table.columnOf(m_boosterMmt), 2);
    // The sustainer's body tube is a MotorMount, but not an acting one.
    EXPECT_EQ(table.columnOf(m_sustainerBody), -1);
    EXPECT_FALSE(FlightConfigurableTableModel::includes(Kind::MOTOR_MOUNTS,
                                                        find<BodyTube>(m_sustainerBody)));
    EXPECT_TRUE(FlightConfigurableTableModel::includes(Kind::MOTOR_MOUNTS,
                                                       find<InnerTube>(m_sustainerMmt)));
    EXPECT_FALSE(FlightConfigurableTableModel::includes(Kind::MOTOR_MOUNTS, rocket()));
}

TEST_F(FlightConfigurableTableModelTest, HasAColumnPerRecoveryDevice)
{
    const FlightConfigurableTableModel table(Kind::RECOVERY_DEVICES, rocket(), m_preferences);
    EXPECT_EQ(headers(table),
              (std::vector<QString>{QStringLiteral("Configuration"), QStringLiteral("Parachute")}));
    EXPECT_EQ(table.componentId(1), m_parachute);
    EXPECT_TRUE(FlightConfigurableTableModel::includes(Kind::RECOVERY_DEVICES,
                                                       find<Parachute>(m_parachute)));
    EXPECT_FALSE(FlightConfigurableTableModel::includes(Kind::RECOVERY_DEVICES,
                                                        find<InnerTube>(m_boosterMmt)));
}

TEST_F(FlightConfigurableTableModelTest, HasAColumnPerStageButTheTopOne)
{
    const FlightConfigurableTableModel table(Kind::STAGES, rocket(), m_preferences);
    EXPECT_EQ(headers(table), (std::vector<QString>{QStringLiteral("Configuration"),
                                                    QStringLiteral("Booster Stage")}));
    EXPECT_EQ(table.componentId(1), m_boosterStage);
    EXPECT_FALSE(FlightConfigurableTableModel::includes(Kind::STAGES, rocket().getChild(0)));
    // No vertical header (Java's table has none) and no other role.
    EXPECT_FALSE(table.headerData(0, Qt::Vertical).isValid());
    EXPECT_FALSE(table.headerData(1, Qt::Horizontal, Qt::ToolTipRole).isValid());
    EXPECT_FALSE(table.headerData(2, Qt::Horizontal).isValid());
}

TEST_F(FlightConfigurableTableModelTest, ShowsTheConfigurationsNamesInColumnZero)
{
    const FlightConfigurableTableModel table(Kind::STAGES, rocket(), m_preferences);
    EXPECT_EQ(table.data(table.index(1, 0)).toString(), QStringLiteral("[B4-3; D21-0]"));
    EXPECT_EQ(table.data(table.index(0, 0)).toString(), QStringLiteral("[A8-0; None]"));
    EXPECT_FALSE(
        table.data(table.index(1, 0), FlightConfigurableTableModel::kDefaultRole).toBool());
    EXPECT_FALSE(table.data(table.index(1, 0), Qt::FontRole).isValid());
    EXPECT_FALSE(table.data(table.index(1, 0), Qt::ForegroundRole).isValid());
    EXPECT_FALSE(table.data(QModelIndex()).isValid());
}

TEST_F(FlightConfigurableTableModelTest, ShowsTheMotorAndItsIgnitionInAMotorCell)
{
    const FlightConfigurableTableModel table(Kind::MOTOR_MOUNTS, rocket(), m_preferences);
    const QModelIndex                  booster = table.index(1, 2);
    EXPECT_EQ(table.data(booster).toString(), QStringLiteral("D21-0"));
    EXPECT_EQ(table.data(booster, FlightConfigurableTableModel::kIgnitionTextRole).toString(),
              QStringLiteral("Default (Automatic)"));
    EXPECT_TRUE(table.data(booster, FlightConfigurableTableModel::kDefaultRole).toBool());
    // MotorCellDelegate styles the labels: no font or colour for the cell.
    EXPECT_FALSE(table.data(booster, Qt::FontRole).isValid());
    EXPECT_FALSE(table.data(booster, Qt::ForegroundRole).isValid());
    EXPECT_EQ(table.data(table.index(0, 2)).toString(), QStringLiteral("None"));
    // Column 0 has no ignition.
    EXPECT_FALSE(
        table.data(table.index(1, 0), FlightConfigurableTableModel::kIgnitionTextRole).isValid());

    find<InnerTube>(m_boosterMmt).getMotorConfig(testFcid(1)).setIgnitionDelay(2);
    EXPECT_EQ(table.data(booster, FlightConfigurableTableModel::kIgnitionTextRole).toString(),
              QStringLiteral("Automatic + 2 s"));
    EXPECT_FALSE(table.data(booster, FlightConfigurableTableModel::kDefaultRole).toBool());
}

TEST_F(FlightConfigurableTableModelTest, ShadesARecoveryCellWithoutAnOverride)
{
    const FlightConfigurableTableModel table(Kind::RECOVERY_DEVICES, rocket(), m_preferences);
    const QModelIndex                  cell = table.index(2, 1);
    EXPECT_EQ(table.data(cell).toString(), QStringLiteral("Ejection charge"));
    // OpenRocket bug 1 fixed: Java's isDefault(id) is false here, and its cell regular.
    EXPECT_TRUE(table.data(cell, FlightConfigurableTableModel::kDefaultRole).toBool());
    EXPECT_TRUE(qvariant_cast<QFont>(table.data(cell, Qt::FontRole)).italic());
    EXPECT_EQ(qvariant_cast<QColor>(table.data(cell, Qt::ForegroundRole)),
              ThemeColors::color(ThemeColors::Key::TEXT_DIM));
    EXPECT_FALSE(table.data(cell, FlightConfigurableTableModel::kIgnitionTextRole).isValid());

    auto& deployments                = find<Parachute>(m_parachute).getDeploymentConfigurations();
    DeploymentConfiguration altitude = deployments.getDefault().copy(testFcid(2));
    altitude.setDeployEvent(DeploymentConfiguration::DeployEvent::ALTITUDE);
    altitude.setDeployAltitude(150);
    deployments.set(testFcid(2), altitude);
    EXPECT_EQ(table.data(cell).toString(), QStringLiteral("Altitude 150 m"));
    EXPECT_FALSE(table.data(cell, FlightConfigurableTableModel::kDefaultRole).toBool());
    EXPECT_FALSE(table.data(cell, Qt::FontRole).isValid());
    EXPECT_FALSE(table.data(cell, Qt::ForegroundRole).isValid());
}

TEST_F(FlightConfigurableTableModelTest, TakesTheDimColourOfThePaletteWhenAsked)
{
    const FlightConfigurableTableModel table(Kind::RECOVERY_DEVICES, rocket(), m_preferences);
    const QModelIndex                  cell = table.index(0, 1);
    {
        const Gui::Test::ScopedApplicationPalette light(
            Gui::Test::ScopedApplicationPalette::light());
        EXPECT_EQ(qvariant_cast<QColor>(table.data(cell, Qt::ForegroundRole)),
                  ThemeColors::themeColor(ThemeColors::Key::TEXT_DIM, ThemeColors::Theme::LIGHT));
    }
    {
        const Gui::Test::ScopedApplicationPalette dark(Gui::Test::ScopedApplicationPalette::dark());
        EXPECT_EQ(qvariant_cast<QColor>(table.data(cell, Qt::ForegroundRole)),
                  ThemeColors::themeColor(ThemeColors::Key::TEXT_DIM, ThemeColors::Theme::DARK));
    }
}

TEST_F(FlightConfigurableTableModelTest, ShadesAStageCellWithoutAnOverride)
{
    const FlightConfigurableTableModel table(Kind::STAGES, rocket(), m_preferences);
    const QModelIndex                  cell = table.index(0, 1);
    EXPECT_EQ(table.data(cell).toString(), QStringLiteral("Current stage ejection charge"));
    EXPECT_TRUE(table.data(cell, FlightConfigurableTableModel::kDefaultRole).toBool());
    EXPECT_TRUE(qvariant_cast<QFont>(table.data(cell, Qt::FontRole)).italic());

    auto& separations = find<AxialStage>(m_boosterStage).getSeparationConfigurations();
    separations.set(testFcid(0), separations.getDefault().copy(testFcid(0)));
    EXPECT_FALSE(table.data(cell, FlightConfigurableTableModel::kDefaultRole).toBool());
    EXPECT_FALSE(table.data(cell, Qt::FontRole).isValid());
}

TEST_F(FlightConfigurableTableModelTest, MakesEveryCellSelectableAndNoneEditable)
{
    const FlightConfigurableTableModel table(Kind::MOTOR_MOUNTS, rocket(), m_preferences);
    for (const QModelIndex& index : {table.index(0, 0), table.index(4, 2)})
    {
        const Qt::ItemFlags flags = table.flags(index);
        EXPECT_TRUE(flags.testFlag(Qt::ItemIsSelectable));
        EXPECT_TRUE(flags.testFlag(Qt::ItemIsEnabled));
        EXPECT_FALSE(flags.testFlag(Qt::ItemIsEditable));
    }
    EXPECT_EQ(table.flags(QModelIndex()), Qt::NoItemFlags);
}

TEST_F(FlightConfigurableTableModelTest, ResetsWhenAMotorMountStartsActing)
{
    FlightConfigurableTableModel   table(Kind::MOTOR_MOUNTS, rocket(), m_preferences);
    const QAbstractItemModelTester tester(&table,
                                          QAbstractItemModelTester::FailureReportingMode::Warning);
    const QSignalSpy               resets(&table, &QAbstractItemModel::modelReset);
    find<BodyTube>(m_sustainerBody).setMotorMount(true);
    EXPECT_EQ(resets.count(), 1);
    // In the rocket's order: the body tube before the inner tube it holds.
    EXPECT_EQ(table.columnOf(m_sustainerBody), 1);
    EXPECT_EQ(table.columnOf(m_sustainerMmt), 2);
    EXPECT_EQ(table.columnCount(), 4);
    EXPECT_EQ(table.data(table.index(0, 1)).toString(), QStringLiteral("None"));

    find<InnerTube>(m_boosterMmt).setMotorMount(false);
    EXPECT_EQ(resets.count(), 2);
    EXPECT_EQ(table.columnOf(m_boosterMmt), -1);
}

TEST_F(FlightConfigurableTableModelTest, ResetsWhenAConfigurationComesOrGoes)
{
    FlightConfigurableTableModel   table(Kind::RECOVERY_DEVICES, rocket(), m_preferences);
    const QAbstractItemModelTester tester(&table,
                                          QAbstractItemModelTester::FailureReportingMode::Warning);
    const QSignalSpy               resets(&table, &QAbstractItemModel::modelReset);
    const FlightConfigurationId    added = rocket().createFlightConfiguration().getId();
    EXPECT_EQ(resets.count(), 1);
    EXPECT_EQ(table.rowOf(added), 5);
    EXPECT_EQ(table.data(table.index(5, 0)).toString(), QStringLiteral("[No motors]"));

    // NONFUNCTIONAL only: Java's table would read the new row count without an event.
    rocket().removeFlightConfiguration(testFcid(0));
    EXPECT_EQ(resets.count(), 2);
    EXPECT_EQ(table.rowCount(), 5);
    EXPECT_EQ(table.configurationId(0), testFcid(1));
}

TEST_F(FlightConfigurableTableModelTest, RefreshesEveryCellAndHeaderAtOtherEvents)
{
    FlightConfigurableTableModel   table(Kind::STAGES, rocket(), m_preferences);
    const QAbstractItemModelTester tester(&table,
                                          QAbstractItemModelTester::FailureReportingMode::Warning);
    const QSignalSpy               resets(&table, &QAbstractItemModel::modelReset);
    const QSignalSpy               cells(&table, &QAbstractItemModel::dataChanged);
    const QSignalSpy               headerTexts(&table, &QAbstractItemModel::headerDataChanged);
    find<AxialStage>(m_boosterStage).setName("Kicker");
    EXPECT_EQ(resets.count(), 0);
    ASSERT_EQ(cells.count(), 1);
    EXPECT_EQ(qvariant_cast<QModelIndex>(cells.at(0).at(0)), table.index(0, 0));
    EXPECT_EQ(qvariant_cast<QModelIndex>(cells.at(0).at(1)), table.index(4, 1));
    ASSERT_EQ(headerTexts.count(), 1);
    EXPECT_EQ(headerTexts.at(0).at(1).toInt(), 0);
    EXPECT_EQ(headerTexts.at(0).at(2).toInt(), 1);
    EXPECT_EQ(table.headerData(1, Qt::Horizontal).toString(), QStringLiteral("Kicker"));

    // A selection change (NONFUNCTIONAL, the rocket its source) too.
    rocket().setSelectedConfiguration(testFcid(3));
    EXPECT_EQ(cells.count(), 2);
    EXPECT_EQ(resets.count(), 0);
}

TEST_F(FlightConfigurableTableModelTest, FollowsAnUndoByTheComponentsIds)
{
    FlightConfigurableTableModel   table(Kind::MOTOR_MOUNTS, rocket(), m_preferences);
    const QAbstractItemModelTester tester(&table,
                                          QAbstractItemModelTester::FailureReportingMode::Warning);
    m_document->addUndoPosition("Rename");
    find<InnerTube>(m_boosterMmt).setName("Kicker MMT");
    find<InnerTube>(m_boosterMmt).getMotorConfig(testFcid(1)).setIgnitionDelay(3);
    EXPECT_EQ(table.headerData(2, Qt::Horizontal).toString(), QStringLiteral("Kicker MMT"));

    const QSignalSpy resets(&table, &QAbstractItemModel::modelReset);
    const QSignalSpy cells(&table, &QAbstractItemModel::dataChanged);
    m_document->undo();
    // Rocket::loadFrom() replaced every component, keeping the ids: the same keys, no reset,
    // and the cells read the new objects (the asan preset proves no old one is read).
    EXPECT_EQ(resets.count(), 0);
    EXPECT_GE(cells.count(), 1);
    EXPECT_EQ(table.headerData(2, Qt::Horizontal).toString(), QStringLiteral("Booster MMT"));
    EXPECT_EQ(
        table.data(table.index(1, 2), FlightConfigurableTableModel::kIgnitionTextRole).toString(),
        QStringLiteral("Default (Automatic)"));
    EXPECT_EQ(table.component(2), &find<InnerTube>(m_boosterMmt));

    m_document->redo();
    EXPECT_EQ(table.headerData(2, Qt::Horizontal).toString(), QStringLiteral("Kicker MMT"));
    EXPECT_EQ(resets.count(), 0);
}

TEST_F(FlightConfigurableTableModelTest, AnswersNothingForAComponentThatHasGone)
{
    FlightConfigurableTableModel   table(Kind::STAGES, rocket(), m_preferences);
    const QAbstractItemModelTester tester(&table,
                                          QAbstractItemModelTester::FailureReportingMode::Warning);
    const QSignalSpy               resets(&table, &QAbstractItemModel::modelReset);
    rocket().freeze();
    {
        const std::unique_ptr<RocketComponent> booster = rocket().removeChild(1);
        ASSERT_EQ(booster->getId(), m_boosterStage);
    }
    // The event waits for the thaw; the column's stage has gone.
    EXPECT_EQ(table.columnCount(), 2);
    EXPECT_EQ(table.component(1), nullptr);
    EXPECT_FALSE(table.data(table.index(0, 1)).isValid());
    EXPECT_FALSE(table.headerData(1, Qt::Horizontal).isValid());
    EXPECT_EQ(resets.count(), 0);
    rocket().thaw();
    EXPECT_EQ(resets.count(), 1);
    EXPECT_EQ(table.columnCount(), 1);
}

TEST_F(FlightConfigurableTableModelTest, ResetsAgainForAChangeMadeDuringItsReset)
{
    FlightConfigurableTableModel   table(Kind::MOTOR_MOUNTS, rocket(), m_preferences);
    const QAbstractItemModelTester tester(&table,
                                          QAbstractItemModelTester::FailureReportingMode::Warning);
    const QSignalSpy               resets(&table, &QAbstractItemModel::modelReset);
    bool                           changed = false;
    QObject::connect(&table, &QAbstractItemModel::modelReset, &table, [this, &changed] {
        if (!changed)
        {
            changed = true;
            find<BodyTube>(m_sustainerBody).setMotorMount(true);
        }
    });
    find<InnerTube>(m_boosterMmt).setMotorMount(false);
    EXPECT_TRUE(changed);
    EXPECT_EQ(resets.count(), 2);
    EXPECT_EQ(std::vector(table.componentIds().begin(), table.componentIds().end()),
              (std::vector<Uuid>{m_sustainerBody, m_sustainerMmt}));
}

TEST_F(FlightConfigurableTableModelTest, TakesTheRocketsKeysOnRequest)
{
    FlightConfigurableTableModel table(Kind::RECOVERY_DEVICES, rocket(), m_preferences);
    const QSignalSpy             resets(&table, &QAbstractItemModel::modelReset);
    const QSignalSpy             cells(&table, &QAbstractItemModel::dataChanged);
    table.refresh();
    EXPECT_EQ(resets.count(), 0);
    EXPECT_EQ(cells.count(), 1);

    rocket().freeze();
    const FlightConfigurationId added = rocket().createFlightConfiguration().getId();
    table.refresh();
    EXPECT_EQ(resets.count(), 1);
    EXPECT_EQ(table.rowOf(added), 5);
    rocket().thaw();
    // The thaw's event finds the keys taken.
    EXPECT_EQ(resets.count(), 1);
}

}  // namespace

}  // namespace QtRocket::Gui
