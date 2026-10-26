#include "QtRocket/rocket/MotorConfigurationSet.h"

#include <format>
#include <iterator>
#include <string>

#include "QtRocket/rocket/FlightConfigurableParameterSet.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"

namespace QtRocket
{

MotorConfigurationSet::MotorConfigurationSet(MotorMount& mount)
  : FlightConfigurableParameterSet<MotorConfiguration>(
        MotorConfiguration{mount, FlightConfigurationId::defaultValueId()})
{
}

MotorConfigurationSet::MotorConfigurationSet(
    const FlightConfigurableParameterSet<MotorConfiguration>& sourceSet, MotorMount& newMount)
  // QtRocket fixes M3's bug 6 (U-M3-5): the default is the source's, made anew for the new mount,
  // so that its ignition survives undo, redo, copy and paste. Java starts from a new empty default
  // (new MotorConfiguration(newMount, DEFAULT_VALUE_FCID)) and loses it.
  : FlightConfigurableParameterSet<MotorConfiguration>(MotorConfiguration{
        newMount, FlightConfigurationId::defaultValueId(), sourceSet.getDefault()})
{
    // Every value, the default included: set() ignores the default id (as in Java), which the
    // constructor above has taken care of.
    for (const MotorConfiguration& sourceConfig : sourceSet.values())
    {
        const FlightConfigurationId nextFcid = sourceConfig.getFcid();
        set(nextFcid, MotorConfiguration{newMount, nextFcid, sourceConfig});
    }
}

std::string MotorConfigurationSet::toDebug(const Preferences& preferences) const
{
    std::string buffer;
    std::format_to(std::back_inserter(buffer),
                   " ====== Dumping MotorConfigurationSet: {} motors in {} ======\n", size(),
                   asComponent(getDefault().getMount()).getDebugName());

    // Java walks the map's entries: the default first, then the overrides in insertion order.
    const auto appendEntry = [&](const FlightConfigurationId& loopFcid,
                                 const MotorConfiguration&    curConfig) {
        buffer += isDefault(loopFcid) ? "  [DEF]" : "       ";
        std::format_to(std::back_inserter(buffer),
                       "@{:>10}=[fcid//{:>8}][mid//{:>8}][    {:>8} ign@: {:>12}]\n",
                       loopFcid.toShortKey(), curConfig.getFcid().toShortKey(),
                       curConfig.getMid().toShortKey(), curConfig.toMotorName(preferences),
                       curConfig.toIgnitionDescription());
    };
    appendEntry(FlightConfigurationId::defaultValueId(), getDefault());
    for (const FlightConfigurationId& fcid : getIds())
    {
        appendEntry(fcid, get(fcid));
    }
    return buffer;
}

}  // namespace QtRocket
