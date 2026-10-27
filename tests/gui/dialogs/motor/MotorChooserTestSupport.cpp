#include "MotorChooserTestSupport.h"

#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <QComboBox>
#include <QLineEdit>
#include <QModelIndex>
#include <QPoint>
#include <QString>
#include <QTest>
#include <QVariant>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "QtRocket/gui/dialogs/motor/MotorTableModel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/motor/chooser/MotorTableSorter.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Md5.h"

namespace QtRocket::Gui::Test
{

std::string motorTableRows(const MotorTableModel& model)
{
    std::string rows;
    for (int row = 0; row < model.rowCount(); ++row)
    {
        rows += std::to_string(model.modelRow(row));
        for (int column = 0; column < model.columnCount(); ++column)
        {
            rows += '\t';
            rows +=
                QtStrings::toUtf8(model.data(model.index(row, column), Qt::DisplayRole).toString());
        }
        rows += '\n';
    }
    return rows;
}

std::string motorTableMd5(const MotorTableModel& model)
{
    const std::string rows = motorTableRows(model);
    return toHex(md5(std::as_bytes(std::span(rows))));
}

std::string sortKeysText(const MotorTableSorter& sorter)
{
    std::string text;
    for (const MotorTableSorter::SortKey& key : sorter.getSortKeys())
    {
        if (!text.empty())
        {
            text += ' ';
        }
        text += std::format("{}{}", static_cast<int>(key.column),
                            key.sortOrder == MotorTableSorter::SortOrder::ASCENDING ? 'A' : 'D');
    }
    return text;
}

const ThrustCurveMotorSet& findMotorSet(const ThrustCurveMotorSetDatabase& database,
                                        std::string_view manufacturer, std::string_view designation)
{
    for (const ThrustCurveMotorSet& set : database.getMotorSets())
    {
        if (set.getManufacturer() != nullptr && set.getManufacturer()->matches(manufacturer) &&
            set.getDesignation() == designation)
        {
            return set;
        }
    }
    bug("no motor set " + std::string(manufacturer) + " " + std::string(designation));
}

UserPrompts::MotorChoice chosenMotor(const std::optional<UserPrompts::MotorChoice>& choice)
{
    if (!choice.has_value())
    {
        ADD_FAILURE() << "no motor was chosen";
        return {};
    }
    return *choice;
}

void chooseComboItem(QComboBox& box, int index)
{
    box.setCurrentIndex(index);
    Q_EMIT box.activated(index);
}

void doubleClickAt(QWidget& widget, QPoint position, Qt::MouseButton button)
{
    QTest::mouseClick(&widget, button, {}, position);
    QTest::mousePress(&widget, button, {}, position);
    QTest::mouseDClick(&widget, button, {}, position);
    QTest::mouseRelease(&widget, button, {}, position);
}

void typeIntoComboBox(QComboBox& box, const QString& text)
{
    box.setFocus();
    box.lineEdit()->selectAll();
    QTest::keyClicks(&box, text);
}

InnerTubeMount::InnerTubeMount()
{
    AxialStage& stage = m_rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&   body  = stage.addChild(std::make_unique<BodyTube>());
    m_mount           = &body.addChild(std::make_unique<InnerTube>());
    m_mount->setOuterRadius(0.0145);
    m_mount->setInnerRadius(0.0145 - 0.0005);
    m_mount->setLength(0.1);
    m_mount->setMotorMount(true);
    m_rocket.createFlightConfiguration(m_fcid);
}

BodyTubeMount::BodyTubeMount()
{
    AxialStage& stage = m_rocket.addChild(std::make_unique<AxialStage>());
    m_mount           = &stage.addChild(std::make_unique<BodyTube>());
    m_mount->setMotorMount(true);
}

FlightConfigurationId BodyTubeMount::configure(std::shared_ptr<const ThrustCurveMotor> motor,
                                               double                                  delay)
{
    const FlightConfigurationId fcid;
    m_rocket.createFlightConfiguration(fcid);
    MotorConfiguration configuration(*m_mount, fcid);
    configuration.setMotor(std::move(motor));
    configuration.setEjectionDelay(delay);
    m_mount->setMotorConfig(configuration, fcid);
    return fcid;
}

FlightConfigurationId BodyTubeMount::configure(std::shared_ptr<const ThrustCurveMotor> motor,
                                               double delay, double nozzleExitDiameter)
{
    const FlightConfigurationId fcid;
    m_rocket.createFlightConfiguration(fcid);
    MotorConfiguration configuration(*m_mount, fcid);
    configuration.setMotor(std::move(motor));
    configuration.setEjectionDelay(delay);
    const Result<void> nozzle = configuration.setNozzleExitDiameter(nozzleExitDiameter);
    if (!nozzle)
    {
        bug("the nozzle exit diameter is refused: " + nozzle.error().message);
    }
    m_mount->setMotorConfig(configuration, fcid);
    return fcid;
}

FlightConfigurationId BodyTubeMount::addEmptyConfiguration()
{
    const FlightConfigurationId fcid;
    m_rocket.createFlightConfiguration(fcid);
    return fcid;
}

namespace
{

/// A motor mount of @p body: an inner tube of inner radius @p innerRadius, its wall 0.5 mm thick,
/// @p length long.
[[nodiscard]] InnerTube& addInnerTubeMount(BodyTube& body, double innerRadius, double length)
{
    InnerTube& tube = body.addChild(std::make_unique<InnerTube>());
    tube.setOuterRadius(innerRadius + 0.0005);
    tube.setInnerRadius(innerRadius);
    tube.setLength(length);
    tube.setMotorMount(true);
    return tube;
}

}  // namespace

TwoInnerTubeMounts::TwoInnerTubeMounts()
{
    AxialStage& stage = m_rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&   body  = stage.addChild(std::make_unique<BodyTube>());
    m_mount29         = &addInnerTubeMount(body, 0.0145, 0.15);
    m_mount18         = &addInnerTubeMount(body, 0.009, 0.075);
    m_rocket.createFlightConfiguration(m_fcid);
}

void TwoInnerTubeMounts::configure(InnerTube&                              mount,
                                   std::shared_ptr<const ThrustCurveMotor> motor) const
{
    MotorConfiguration configuration(mount, m_fcid);
    configuration.setMotor(std::move(motor));
    configuration.setEjectionDelay(0.0);
    mount.setMotorConfig(configuration, m_fcid);
}

}  // namespace QtRocket::Gui::Test
