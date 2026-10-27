#include "QtRocket/gui/flightconfig/MotorMountTableModel.h"

#include <memory>
#include <optional>
#include <utility>

#include <QAbstractItemModel>
#include <QAbstractItemModelTester>
#include <QModelIndex>
#include <QSignalSpy>
#include <QString>
#include <QVariant>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"
#include "rocket/TestRockets.h"

namespace QtRocket::Gui
{

namespace
{

/// OpenRocket's makeBeta in a document: four MotorMounts, in the rocket's order the sustainer's
/// body tube (not acting), its inner tube (acting), the booster's body tube (not acting) and its
/// inner tube (acting): the motor-mount rows of the flight-config pins (testrocket:makeBeta).
class MotorMountTableModelTest : public Gui::Test::GuiTest
{
protected:
    void SetUp() override
    {
        Gui::Test::GuiTest::SetUp();
        QtRocket::Test::TestBeta beta;
        m_sustainerBody = beta.body->getId();
        m_sustainerMmt  = beta.inner->getId();
        m_boosterBody   = beta.boosterBody->getId();
        m_boosterMmt    = beta.boosterMmt->getId();
        m_document = OpenRocketDocumentFactory::createDocumentFromRocket(std::move(beta.rocket));
    }

    void TearDown() override
    {
        m_document.reset();
        Gui::Test::GuiTest::TearDown();
    }

    [[nodiscard]] Rocket& rocket() const { return m_document->getRocket(); }

    /// The mount @p id of the rocket as it is now.
    [[nodiscard]] MotorMount& mountOf(const Uuid& id) const
    {
        auto* mount = dynamic_cast<MotorMount*>(rocket().findComponent(id));
        if (mount == nullptr)
        {
            bug("the test design has no motor mount " + id.toString());
        }
        return *mount;
    }

    std::unique_ptr<OpenRocketDocument> m_document;
    Uuid                                m_sustainerBody;
    Uuid                                m_sustainerMmt;
    Uuid                                m_boosterBody;
    Uuid                                m_boosterMmt;
};

/// The check state of @p row.
Qt::CheckState checkState(const MotorMountTableModel& model, int row)
{
    return static_cast<Qt::CheckState>(
        model.data(model.index(row, MotorMountTableModel::kMotorMountColumn), Qt::CheckStateRole)
            .toInt());
}

/// The name of @p row.
QString name(const MotorMountTableModel& model, int row)
{
    return model.data(model.index(row, MotorMountTableModel::kNameColumn)).toString();
}

TEST_F(MotorMountTableModelTest, HasARowPerMotorMountActingOrNot)
{
    const MotorMountTableModel table(rocket());
    ASSERT_EQ(table.rowCount(), 4);
    EXPECT_EQ(table.columnCount(), 2);
    EXPECT_EQ(name(table, 0), QStringLiteral("Sustainer Body Tube"));
    EXPECT_EQ(name(table, 1), QStringLiteral("Motor Mount Tube"));
    EXPECT_EQ(name(table, 2), QStringLiteral("Booster Body"));
    EXPECT_EQ(name(table, 3), QStringLiteral("Booster MMT"));
    EXPECT_EQ(checkState(table, 0), Qt::Unchecked);
    EXPECT_EQ(checkState(table, 1), Qt::Checked);
    EXPECT_EQ(checkState(table, 2), Qt::Unchecked);
    EXPECT_EQ(checkState(table, 3), Qt::Checked);
    EXPECT_EQ(table.mountId(2), m_boosterBody);
    EXPECT_EQ(table.mountId(4), std::nullopt);
    EXPECT_EQ(table.mount(3), &mountOf(m_boosterMmt));
    EXPECT_EQ(table.mount(-1), nullptr);
    EXPECT_EQ(table.rowOf(m_sustainerMmt), 1);
    EXPECT_EQ(table.rowOf(rocket().getId()), -1);
    // The check box has no text, the name no check box; no header.
    EXPECT_FALSE(table.data(table.index(0, MotorMountTableModel::kMotorMountColumn)).isValid());
    EXPECT_FALSE(table.data(table.index(0, MotorMountTableModel::kNameColumn), Qt::CheckStateRole)
                     .isValid());
    EXPECT_FALSE(table.headerData(0, Qt::Horizontal).isValid());
}

TEST_F(MotorMountTableModelTest, LetsOnlyTheCheckBoxChangeAndNothingBeSelected)
{
    const MotorMountTableModel table(rocket());
    const Qt::ItemFlags        check =
        table.flags(table.index(0, MotorMountTableModel::kMotorMountColumn));
    EXPECT_TRUE(check.testFlag(Qt::ItemIsUserCheckable));
    EXPECT_TRUE(check.testFlag(Qt::ItemIsEnabled));
    EXPECT_FALSE(check.testFlag(Qt::ItemIsSelectable));
    EXPECT_FALSE(check.testFlag(Qt::ItemIsEditable));
    const Qt::ItemFlags text = table.flags(table.index(0, MotorMountTableModel::kNameColumn));
    EXPECT_FALSE(text.testFlag(Qt::ItemIsUserCheckable));
    EXPECT_FALSE(text.testFlag(Qt::ItemIsSelectable));
    EXPECT_FALSE(text.testFlag(Qt::ItemIsEditable));
    EXPECT_EQ(table.flags(QModelIndex()), Qt::NoItemFlags);
}

TEST_F(MotorMountTableModelTest, AsksForAChangeOfTheCheckBoxWithoutMakingIt)
{
    MotorMountTableModel table(rocket());
    const QSignalSpy     requests(&table, &MotorMountTableModel::motorMountChangeRequested);
    const QModelIndex    body = table.index(0, MotorMountTableModel::kMotorMountColumn);
    EXPECT_TRUE(table.setData(body, Qt::Checked, Qt::CheckStateRole));
    ASSERT_EQ(requests.count(), 1);
    EXPECT_EQ(requests.at(0).at(0).value<Uuid>(), m_sustainerBody);
    EXPECT_TRUE(requests.at(0).at(1).toBool());
    // The panel applies it (part F3's operation); the model changes nothing.
    EXPECT_FALSE(mountOf(m_sustainerBody).isMotorMount());

    EXPECT_TRUE(table.setData(table.index(3, MotorMountTableModel::kMotorMountColumn),
                              Qt::Unchecked, Qt::CheckStateRole));
    ASSERT_EQ(requests.count(), 2);
    EXPECT_EQ(requests.at(1).at(0).value<Uuid>(), m_boosterMmt);
    EXPECT_FALSE(requests.at(1).at(1).toBool());

    // The state it has already: nothing to ask (setMotorMount() would do nothing).
    EXPECT_TRUE(table.setData(body, Qt::Unchecked, Qt::CheckStateRole));
    EXPECT_EQ(requests.count(), 2);
}

TEST_F(MotorMountTableModelTest, RefusesAnythingButACheckState)
{
    MotorMountTableModel table(rocket());
    const QSignalSpy     requests(&table, &MotorMountTableModel::motorMountChangeRequested);
    const QModelIndex    body = table.index(0, MotorMountTableModel::kMotorMountColumn);
    EXPECT_FALSE(table.setData(body, Qt::Checked, Qt::EditRole));
    EXPECT_FALSE(table.setData(body, Qt::PartiallyChecked, Qt::CheckStateRole));
    EXPECT_FALSE(table.setData(body, QStringLiteral("yes"), Qt::CheckStateRole));
    EXPECT_FALSE(table.setData(table.index(0, MotorMountTableModel::kNameColumn), Qt::Checked,
                               Qt::CheckStateRole));
    EXPECT_FALSE(table.setData(QModelIndex(), Qt::Checked, Qt::CheckStateRole));
    EXPECT_EQ(requests.count(), 0);
}

TEST_F(MotorMountTableModelTest, FollowsTheRocket)
{
    MotorMountTableModel           table(rocket());
    const QAbstractItemModelTester tester(&table,
                                          QAbstractItemModelTester::FailureReportingMode::Warning);
    const QSignalSpy               resets(&table, &QAbstractItemModel::modelReset);
    const QSignalSpy               cells(&table, &QAbstractItemModel::dataChanged);

    // The panel's answer to a request: the rows stay, their cells change.
    mountOf(m_sustainerBody).setMotorMount(true);
    EXPECT_EQ(checkState(table, 0), Qt::Checked);
    EXPECT_EQ(resets.count(), 0);
    EXPECT_EQ(cells.count(), 1);

    asComponent(mountOf(m_boosterBody)).setName("Kicker");
    EXPECT_EQ(name(table, 2), QStringLiteral("Kicker"));

    // A new tube: a new row, in the rocket's order.
    asComponent(mountOf(m_sustainerBody)).addChild(std::make_unique<InnerTube>());
    EXPECT_EQ(resets.count(), 1);
    ASSERT_EQ(table.rowCount(), 5);
    EXPECT_EQ(table.rowOf(m_sustainerMmt), 1);
    EXPECT_EQ(name(table, 2), QStringLiteral("Inner Tube"));
    EXPECT_EQ(table.rowOf(m_boosterBody), 3);
}

TEST_F(MotorMountTableModelTest, FollowsAnUndoByTheMountsIds)
{
    MotorMountTableModel           table(rocket());
    const QAbstractItemModelTester tester(&table,
                                          QAbstractItemModelTester::FailureReportingMode::Warning);
    m_document->addUndoPosition("Modify motor mount");
    mountOf(m_boosterBody).setMotorMount(true);
    EXPECT_EQ(checkState(table, 2), Qt::Checked);

    const QSignalSpy resets(&table, &QAbstractItemModel::modelReset);
    m_document->undo();
    EXPECT_EQ(resets.count(), 0);
    EXPECT_EQ(checkState(table, 2), Qt::Unchecked);
    EXPECT_EQ(table.mount(2), &mountOf(m_boosterBody));
}

TEST_F(MotorMountTableModelTest, AnswersNothingForAMountThatHasGone)
{
    MotorMountTableModel           table(rocket());
    const QAbstractItemModelTester tester(&table,
                                          QAbstractItemModelTester::FailureReportingMode::Warning);
    rocket().freeze();
    {
        const std::unique_ptr<RocketComponent> booster = rocket().removeChild(1);
    }
    EXPECT_EQ(table.rowCount(), 4);
    EXPECT_EQ(table.mount(3), nullptr);
    EXPECT_FALSE(table.data(table.index(3, MotorMountTableModel::kNameColumn)).isValid());
    EXPECT_FALSE(table.setData(table.index(3, MotorMountTableModel::kMotorMountColumn),
                               Qt::Unchecked, Qt::CheckStateRole));
    rocket().thaw();
    EXPECT_EQ(table.rowCount(), 2);
}

}  // namespace

}  // namespace QtRocket::Gui
