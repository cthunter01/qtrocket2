#include "QtRocket/simulation/SimulationInputRanges.h"

#include <cmath>
#include <optional>

#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

const SimulationInputRanges::Range& SimulationInputRanges::ofLookupNumber(
    const MachAoALookup::Number& number)
{
    switch (number.kind)
    {
        case MachAoALookup::Number::Kind::MACH:
            return kLookupMach;
        case MachAoALookup::Number::Kind::AOA:
            return kLookupAngleOfAttack;
        case MachAoALookup::Number::Kind::VALUE:
            return number.column == "cp" ? kLookupCentreOfPressure : kLookupCoefficient;
    }
    bug("Unknown kind of a lookup table's number");
}

std::optional<MachAoALookup::Number> SimulationInputRanges::findRefusedLookupNumber(
    const MachAoALookup& table)
{
    return table.findNumber([](const MachAoALookup::Number& number) {
        return !std::isfinite(number.value) || !ofLookupNumber(number).contains(number.value);
    });
}

}  // namespace QtRocket
