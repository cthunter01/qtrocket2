#pragma once

#include <optional>
#include <string_view>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class RecoveryDevice;

/// Reads one <deploymentconfiguration> element of a parachute or a streamer: when the device
/// deploys in one flight configuration (OpenRocket's
/// file/openrocket/importt/DeploymentConfigurationHandler).
///
/// The children, each plain text that is trimmed (String.trim()) before it is read:
/// - <deployevent>: the event the text names (deployEventFromOrkName(), which matches as
///   DocumentConfig.findEnum() does: "apogee", "lowerstageseparation"); a text that names none
///   gives Warning::kFileInvalidParameter and takes back an event read before it.
/// - <deploydelay>, <deployaltitude>: Double.parseDouble; a text that is no number gives
///   Warning::kFileInvalidParameter and takes back a value read before it.
/// - any other child: AbstractElementHandler's warnings for its text and its attributes.
///
/// When the element closes, the device's deployment of the configuration
/// DocumentConfig::configurationId() names becomes a copy of the device's DEFAULT deployment
/// as it is at that moment, with the event, the delay and the altitude that were read put in
/// (getConfiguration()). What the element does not give is therefore the default's, also when
/// the configuration had a deployment of its own before. An element without a configid makes
/// the deployment of a new random id, which no flight configuration has, as in OpenRocket.
/// Neither the element's own text nor its attributes are warned of.
///
/// Deviations from OpenRocket:
/// - A delay or an altitude that is a NaN or an infinity is refused with
///   Warning::kFileInvalidParameter, like a text that is no number. OpenRocket stores an
///   infinity (a simulation with it cannot run) and passes over a NaN without a warning.
/// - A configid that spells out the key of the error id is refused with
///   Warning::kFileInvalidParameter, and the element changes nothing; OpenRocket, in which no
///   id made from a text is the error id (see FlightConfigurationId), stores a deployment
///   under it. One that spells out the key of the default id replaces the device's default
///   deployment, which is what OpenRocket's map does with it.
/// - The constructor takes the loading context, as every handler of the loader does, and does
///   not need it; neither does Java's.
class DeploymentConfigurationHandler final : public AbstractElementHandler
{
public:
    /// The handler of a <deploymentconfiguration> element of @p component, which must outlive
    /// it.
    DeploymentConfigurationHandler(RecoveryDevice&               component,
                                   const DocumentLoadingContext& context) noexcept;

    /// A copy of @p def with what this handler has read put in: the event when one was read,
    /// the delay and the altitude when one was read and is finite (getConfiguration()).
    [[nodiscard]] DeploymentConfiguration getConfiguration(
        const DeploymentConfiguration& def) const;

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

    [[nodiscard]] Result<void> endHandler(std::string_view element, const Attributes& attributes,
                                          std::string_view content, WarningSet& warnings) override;

private:
    RecoveryDevice*                                     m_recoveryDevice;
    std::optional<DeploymentConfiguration::DeployEvent> m_event;
    /// Always finite; nullopt when none was read (Java: NaN).
    std::optional<double> m_delay;
    /// Always finite; nullopt when none was read (Java: NaN).
    std::optional<double> m_altitude;
};

}  // namespace QtRocket
