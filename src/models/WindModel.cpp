#include "QtRocket/models/WindModel.h"

#include <array>
#include <optional>
#include <string_view>

#include "QtRocket/util/Strings.h"

namespace QtRocket
{

std::string_view toString(WindModel::AltitudeReference reference) noexcept
{
    switch (reference)
    {
        case WindModel::AltitudeReference::MSL:
            return "msl";
        case WindModel::AltitudeReference::AGL:
            return "agl";
    }
    return "msl";  // not reached: the switch covers every reference
}

std::optional<WindModel::AltitudeReference> altitudeReferenceFromString(
    std::string_view text) noexcept
{
    const std::string_view name = Strings::trim(text);
    for (const WindModel::AltitudeReference reference :
         std::array{WindModel::AltitudeReference::MSL, WindModel::AltitudeReference::AGL})
    {
        if (name == toString(reference))
        {
            return reference;
        }
    }
    return std::nullopt;
}

}  // namespace QtRocket
