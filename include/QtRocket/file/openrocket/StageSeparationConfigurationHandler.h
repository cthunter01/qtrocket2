#pragma once

#include <optional>
#include <string_view>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class AxialStage;
class DocumentLoadingContext;

/// Reads one <separationconfiguration> element of a stage: when the stage separates in one
/// flight configuration (OpenRocket's
/// file/openrocket/importt/StageSeparationConfigurationHandler).
///
/// The children, each plain text that is trimmed (String.trim()) before it is read:
/// - <separationevent>: the event the text names (separationEventFromOrkName(), which matches
///   as DocumentConfig.findEnum() does: "upperignition", not "upper_ignition"); a text that
///   names none gives Warning::kFileInvalidParameter and takes back an event read before it.
/// - <separationaltitude>, <separationdelay>: Double.parseDouble; a text that is no number
///   gives Warning::kFileInvalidParameter and takes back a value read before it.
/// - any other child: AbstractElementHandler's warnings for its text and its attributes.
///
/// When the element closes, the stage's separation of the configuration
/// DocumentConfig::configurationId() names becomes a copy of the separation that configuration
/// has at that moment, its own or else the stage's default one, with the event, the altitude
/// and the delay that were read put in (getConfiguration()). This is where the handler differs
/// from the one of a recovery device, which always starts from the default: a second
/// <separationconfiguration> of the same id keeps what the first one set and it does not give.
/// An element without a configid makes the separation of a new random id, which no flight
/// configuration has. Neither the element's own text nor its attributes are warned of.
///
/// Deviations from OpenRocket:
/// - A delay or an altitude that is a NaN or an infinity is refused with
///   Warning::kFileInvalidParameter, like a text that is no number. OpenRocket stores an
///   infinity (a simulation with it cannot run) and passes over a NaN without a warning.
/// - A configid that spells out the key of the error id is refused with
///   Warning::kFileInvalidParameter, and the element changes nothing; OpenRocket, in which no
///   id made from a text is the error id (see FlightConfigurationId), stores a separation
///   under it. One that spells out the key of the default id replaces the stage's default
///   separation, which is what OpenRocket's map does with it.
/// - The constructor takes the loading context, as every handler of the loader does, and does
///   not need it; neither does Java's.
class StageSeparationConfigurationHandler final : public AbstractElementHandler
{
public:
    /// The handler of a <separationconfiguration> element of @p stage, which must outlive it.
    StageSeparationConfigurationHandler(AxialStage&                   stage,
                                        const DocumentLoadingContext& context) noexcept;

    /// A copy of @p def with what this handler has read put in: the event when one was read,
    /// the altitude and the delay when one was read and is finite (getConfiguration()).
    [[nodiscard]] StageSeparationConfiguration getConfiguration(
        const StageSeparationConfiguration& def) const;

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

    [[nodiscard]] Result<void> endHandler(std::string_view element, const Attributes& attributes,
                                          std::string_view content, WarningSet& warnings) override;

private:
    AxialStage*                                                  m_stage;
    std::optional<StageSeparationConfiguration::SeparationEvent> m_event;
    /// Always finite; nullopt when none was read (Java: NaN).
    std::optional<double> m_altitude;
    /// Always finite; nullopt when none was read (Java: NaN).
    std::optional<double> m_delay;
};

}  // namespace QtRocket
