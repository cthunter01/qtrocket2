#include "QtRocket/gui/dialogs/simulation/WindLevelSummary.h"

#include <limits>
#include <vector>

#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket::Gui
{

WindLevelSummary::WindLevelSummary(MultiLevelPinkNoiseWindModel& model, QWidget* parent)
  : QWidget(parent),
    m_model(&model),
    m_summaryLabel(makeChild<QLabel>(this)),
    //: simedtdlg.but.editWindLevels
    m_editButton(makeChild<QPushButton>(this, Icons::icon(Icons::Id::EDIT_EDIT), tr("Edit levels")))
{
    m_summaryLabel->setObjectName(QStringLiteral("summaryLabel"));
    m_editButton->setObjectName(QStringLiteral("editButton"));

    // Java: the multi-level panel's MigLayout("fillx, ins 0, gap rel rel"): the summary (in a
    // panel of its own, "grow, wrap") above the button, which spans the width ("spanx, growx").
    auto* layout = makeChild<QVBoxLayout>(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_summaryLabel);
    layout->addWidget(m_editButton);

    connect(m_editButton, &QPushButton::clicked, this,
            guarded([this] { Q_EMIT editLevelsRequested(); }, QStringLiteral("Edit levels")));
    // OpenRocket bug 36: the summary follows every change of the levels (see the class comment).
    m_connection = model.changed().connect([this] { refresh(); });
    refresh();
}

WindLevelSummary::~WindLevelSummary() = default;

QString WindLevelSummary::summaryText(const MultiLevelPinkNoiseWindModel& model)
{
    const std::vector<const MultiLevelPinkNoiseWindModel::LevelWindModel*> levels =
        model.getLevels();
    if (levels.empty())
    {
        //: simedtdlg.lbl.noWindLevels
        return tr("No wind levels defined");
    }
    //: simedtdlg.lbl.windLevelCount (%1: the number of levels)
    QString text = QStringLiteral("<html>") + tr("%1 wind level(s)").arg(levels.size()) +
                   QStringLiteral("<br>");
    if (levels.size() > 1)
    {
        // OpenRocket bug 36: Java starts the highest altitude and speed at Double.MIN_VALUE, the
        // smallest positive double, which is above every altitude below sea level.
        double minAltitude = std::numeric_limits<double>::max();
        double maxAltitude = std::numeric_limits<double>::lowest();
        double minSpeed    = std::numeric_limits<double>::max();
        double maxSpeed    = std::numeric_limits<double>::lowest();
        for (const MultiLevelPinkNoiseWindModel::LevelWindModel* level : levels)
        {
            minAltitude = MathUtil::javaMin(minAltitude, level->getAltitude());
            maxAltitude = MathUtil::javaMax(maxAltitude, level->getAltitude());
            minSpeed    = MathUtil::javaMin(minSpeed, level->getSpeed());
            maxSpeed    = MathUtil::javaMax(maxSpeed, level->getSpeed());
        }
        //: simedtdlg.lbl.altitudeRange (%1 and %2: the lowest and the highest altitude)
        text += tr("Altitude range: %1 - %2")
                    .arg(QtStrings::defaultUnitText(UnitGroupId::DISTANCE, minAltitude),
                         QtStrings::defaultUnitText(UnitGroupId::DISTANCE, maxAltitude)) +
                QStringLiteral("<br>");
        //: simedtdlg.lbl.speedRange (%1 and %2: the lowest and the highest speed)
        text += tr("Speed range: %1 - %2")
                    .arg(QtStrings::defaultUnitText(UnitGroupId::VELOCITY, minSpeed),
                         QtStrings::defaultUnitText(UnitGroupId::VELOCITY, maxSpeed));
    }
    else
    {
        const MultiLevelPinkNoiseWindModel::LevelWindModel& level = *levels.front();
        //: simedtdlg.lbl.altitude (%1: the altitude of the one level)
        text += tr("Altitude: %1")
                    .arg(QtStrings::defaultUnitText(UnitGroupId::DISTANCE, level.getAltitude())) +
                QStringLiteral("<br>");
        //: simedtdlg.lbl.speed (%1: the speed of the one level)
        text += tr("Speed: %1")
                    .arg(QtStrings::defaultUnitText(UnitGroupId::VELOCITY, level.getSpeed()));
    }
    return text + QStringLiteral("</html>");
}

bool WindLevelSummary::hasModel() const noexcept
{
    return m_connection.connected();
}

void WindLevelSummary::refresh()
{
    if (!hasModel())
    {
        return;
    }
    m_summaryLabel->setText(summaryText(*m_model));
}

}  // namespace QtRocket::Gui
