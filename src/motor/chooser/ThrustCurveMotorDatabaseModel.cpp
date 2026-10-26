#include "QtRocket/motor/chooser/ThrustCurveMotorDatabaseModel.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorColumns.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

ThrustCurveMotorDatabaseModel::ThrustCurveMotorDatabaseModel(
    std::vector<const ThrustCurveMotorSet*> database)
{
    setDatabase(std::move(database));
}

void ThrustCurveMotorDatabaseModel::setDatabase(std::vector<const ThrustCurveMotorSet*> database)
{
    QTROCKET_ASSERT(std::ranges::none_of(
        database, [](const ThrustCurveMotorSet* set) { return set == nullptr; }));
    m_database = std::move(database);
}

const ThrustCurveMotorSet& ThrustCurveMotorDatabaseModel::getMotorSet(std::size_t row) const
{
    QTROCKET_ASSERT(row < m_database.size());
    return *m_database[row];
}

std::optional<std::size_t> ThrustCurveMotorDatabaseModel::getIndex(
    const ThrustCurveMotorSet& set) const noexcept
{
    for (std::size_t row = 0; row < m_database.size(); row++)
    {
        if (m_database[row] == &set)
        {
            return row;
        }
    }
    return std::nullopt;
}

std::string ThrustCurveMotorDatabaseModel::getValueText(std::size_t             row,
                                                        ThrustCurveMotorColumns column,
                                                        const Preferences&      preferences) const
{
    return QtRocket::getValueText(column, getMotorSet(row), preferences);
}

}  // namespace QtRocket
