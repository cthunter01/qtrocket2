#include "QtRocket/rocket/AxialStage.h"

#include <cstddef>
#include <format>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

AxialStage::AxialStage()
  : ComponentAssembly(AxialMethod::AFTER), m_separations(StageSeparationConfiguration{})
{
}

std::unique_ptr<RocketComponent> AxialStage::cloneShallow() const
{
    // The implicit copy constructor clones the separations (Java's copyWithOriginalID()).
    return std::make_unique<AxialStage>(*this);
}

bool AxialStage::allowsChildren() const
{
    return true;
}

bool AxialStage::isCompatible(ComponentKind kind) const
{
    return isBodyComponent(kind);
}

void AxialStage::reset(const FlightConfigurationId& fcid)
{
    m_separations.reset(fcid);
}

void AxialStage::copyFlightConfiguration(const FlightConfigurationId& oldConfigId,
                                         const FlightConfigurationId& newConfigId)
{
    m_separations.copyFlightConfiguration(oldConfigId, newConfigId);
}

bool AxialStage::isStageActive() const
{
    return getRocket().isStageActiveInSelectedConfiguration(getStageNumber());
}

int AxialStage::getStageNumber() const
{
    return m_stageNumber;
}

bool AxialStage::isAfter() const
{
    return true;
}

bool AxialStage::hasRecoveryDevice() const
{
    bool found = false;
    forEach([this, &found](const RocketComponent& child) {
        if (child.findStage() == this && isRecoveryDevice(child.kind()))
        {
            found = true;
        }
    });
    return found;
}

std::string AxialStage::toDebugSeparation() const
{
    return m_separations.toDebug();
}

AxialStage* AxialStage::getUpperStage()
{
    if (m_parent == nullptr)
    {
        return nullptr;
    }
    if (dynamic_cast<const Rocket*>(m_parent) != nullptr)
    {
        const auto thisIndex = m_parent->getChildPosition(this);
        if (thisIndex && *thisIndex > 0)
        {
            return dynamic_cast<AxialStage*>(&m_parent->getChild(*thisIndex - 1));
        }
        return nullptr;
    }
    return m_parent->findStage();
}

void AxialStage::toDebugTreeNode(std::string& buffer, const std::string& indent) const
{
    const std::vector<Coordinate> relCoords = getInstanceOffsets();
    const std::vector<Coordinate> absCoords = getComponentLocations();
    if (1 == getInstanceCount())
    {
        std::format_to(std::back_inserter(buffer), "{:<40}|  {:5.3f}; {:>24}; {:>24};",
                       indent + getName() + " (# " + std::to_string(getStageNumber()) + ")",
                       getLength(), getPosition().toString(), absCoords.front().toString());
        std::format_to(std::back_inserter(buffer), "len: {:6.4f} )(offset: {:4.1f}  via: {} )\n",
                       getLength(), getAxialOffset(), axialMethodName(m_axialMethod));
    }
    else
    {
        std::format_to(std::back_inserter(buffer),
                       "{:<40}|(len: {:6.4f} )(offset: {:4.1f} via: {})\n",
                       indent + getName() + "(# " + std::to_string(getStageNumber()) + ")",
                       getLength(), getAxialOffset(), axialMethodName(m_axialMethod));
        for (int instanceNumber = 0; instanceNumber < getInstanceCount(); instanceNumber++)
        {
            const auto        index = static_cast<std::size_t>(instanceNumber);
            const std::string prefix =
                std::format("{}    [{:2}/{:2}]", indent, instanceNumber + 1, getInstanceCount());
            std::format_to(std::back_inserter(buffer), "{:<40}|  {:5.3f}; {:>24}; {:>24};\n",
                           prefix, getLength(), relCoords.at(index).toString(),
                           absCoords.at(index).toString());
        }
    }
}

}  // namespace QtRocket
