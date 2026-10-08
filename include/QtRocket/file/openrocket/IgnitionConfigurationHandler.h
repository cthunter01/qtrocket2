#pragma once

#include <optional>
#include <string_view>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;

/// Reads one <ignitionconfiguration> element of a motor mount: when the motor of one flight
/// configuration ignites (OpenRocket's file/openrocket/importt/IgnitionConfigurationHandler).
/// It only reads; the motor mount's handler, which makes one per element, applies what it read
/// when the element closes (MotorMountHandler).
///
/// The children, each plain text that is trimmed (String.trim()) before it is read:
/// - <ignitionevent>: the event whose name in an .ork file is exactly the text
///   (ignitionEventFromOrkName(): "automatic", "launch", "ejectioncharge", "burnout", "never").
///   A text that names none leaves the event as it was, and gives "Unknown ignition event type
///   '<text>', ignoring." only while no event has been read: after a first <ignitionevent>
///   that was read, a second one that names nothing is passed over without a word, as in
///   OpenRocket.
/// - <ignitiondelay>: Double.parseDouble; a text that is no number gives "Illegal ignition
///   delay specified, ignoring." and leaves the delay as it was.
/// - any other child: AbstractElementHandler's warnings for its text and its attributes.
///
/// Deviations from OpenRocket:
/// - A delay that is a NaN or an infinity is refused with the warning of a text that is no
///   number (OpenRocket stores it, and a simulation with it cannot run).
/// - What was read is asked for through getIgnitionDelay() and getIgnitionEvent(), each
///   nullopt until it was read (Java: two public fields that start as null).
/// - The constructor takes the loading context, as every handler of the loader does, and does
///   not need it; neither does Java's.
class IgnitionConfigurationHandler final : public AbstractElementHandler
{
public:
    /// A handler that has read nothing.
    explicit IgnitionConfigurationHandler(const DocumentLoadingContext& context) noexcept;

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

    /// The ignition delay read, in seconds, always finite; nullopt when the element gave none
    /// that could be read.
    [[nodiscard]] std::optional<double> getIgnitionDelay() const noexcept
    {
        return m_ignitionDelay;
    }

    /// The ignition event read; nullopt when the element gave none that could be read.
    [[nodiscard]] std::optional<IgnitionEvent> getIgnitionEvent() const noexcept
    {
        return m_ignitionEvent;
    }

private:
    std::optional<double>        m_ignitionDelay;
    std::optional<IgnitionEvent> m_ignitionEvent;
};

}  // namespace QtRocket
