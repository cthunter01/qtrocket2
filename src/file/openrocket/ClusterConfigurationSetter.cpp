#include "QtRocket/file/openrocket/ClusterConfigurationSetter.h"

#include <string_view>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/ClusterConfiguration.h"
#include "QtRocket/rocket/Clusterable.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

Result<void> ClusterConfigurationSetter::set(RocketComponent& component, std::string_view  value,
                                             const Attributes& /*attributes*/, WarningSet& warnings,
                                             const DocumentLoadingContext& /*context*/) const
{
    auto* const clusterable = dynamic_cast<Clusterable*>(&component);
    if (clusterable == nullptr)
    {
        warnings.add("Illegal component defined as cluster.");
        return {};
    }

    // for (ClusterConfiguration c : ClusterConfiguration.CONFIGURATIONS)
    //     if (c.getXMLName().equals(value)) ...
    const ClusterConfiguration* const config = ClusterConfiguration::fromXmlName(value);
    if (config == nullptr)
    {
        warnings.add("Illegal cluster configuration specified.");
        return {};
    }

    // Not OpenRocket's: a layout that would take the rocket beyond its instance budget is
    // refused (see the class comment).
    if (!DocumentConfig::instanceCountFits(component, config->getClusterCount()))
    {
        warnings.add(Warning::kFileInvalidParameter);
        return {};
    }

    clusterable->setClusterConfiguration(*config);
    return {};
}

}  // namespace QtRocket
