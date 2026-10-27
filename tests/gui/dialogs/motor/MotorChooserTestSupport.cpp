#include "MotorChooserTestSupport.h"

#include <format>
#include <memory>
#include <span>
#include <string>
#include <utility>

#include <QModelIndex>
#include <QString>
#include <QVariant>
#include <Qt>

#include "QtRocket/gui/dialogs/motor/MotorTableModel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/chooser/MotorTableSorter.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MotorConfiguration.h"
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

}  // namespace QtRocket::Gui::Test
