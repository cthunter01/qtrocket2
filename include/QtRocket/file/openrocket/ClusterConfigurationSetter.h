#pragma once

#include <string_view>

#include "QtRocket/file/openrocket/Setter.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class RocketComponent;

/// Sets the cluster layout of an inner tube from `<clusterconfiguration>3-ring
/// </clusterconfiguration>` (OpenRocket's file/openrocket/importt/ClusterConfigurationSetter).
///
/// The layout is the one of OpenRocket's fourteen whose XML name the text is
/// (ClusterConfiguration::fromXmlName(): "single", "double", "3-row", "4-row", "3-ring" to
/// "6-ring", "3-star" to "6-star", "9-grid", "9-star"). The text is compared as it is: nothing
/// is trimmed and the case counts, so " 3-ring " and "3-RING" name no layout. A text that names
/// none adds the warning "Illegal cluster configuration specified." and the tube keeps the
/// layout it has.
///
/// A component that cannot be clustered (it is no Clusterable) gets the warning "Illegal
/// component defined as cluster." and nothing is set. With the key the setter table has,
/// "InnerTube:clusterconfiguration", no file brings this about: for any other component the
/// element is an unknown parameter.
class ClusterConfigurationSetter final : public Setter
{
public:
    ClusterConfigurationSetter() = default;

    [[nodiscard]] Result<void> set(RocketComponent& component, std::string_view value,
                                   const Attributes& attributes, WarningSet& warnings,
                                   const DocumentLoadingContext& context) const override;
};

}  // namespace QtRocket
