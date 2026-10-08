#pragma once

#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class SimulationOptions;

/// Reads one <atmosphere> element of a simulation's <conditions> (OpenRocket's
/// file/openrocket/importt/AtmosphereHandler). The conditions handler makes one per
/// <atmosphere> element with the element's model attribute, and when the element closes has it
/// store what it read (storeSettings()).
///
/// The children, each read with Double.parseDouble:
/// - <basetemperature>: the launch temperature in K. A text that is no number and a value that
///   is not finite give "Illegal base temperature specified, ignoring." and no temperature;
///   the last element counts, so one that is refused also takes back an earlier one. Zero and
///   a negative value are taken (the atmospheric model refuses them when a simulation runs).
/// - <basepressure>: the launch pressure in Pa, raised to 0.001 Pa when it is lower, so that
///   it is never zero or negative. A text that is no number and a value that is not finite
///   give "Illegal base pressure specified, ignoring." and no pressure; the last element
///   counts as for the temperature.
/// - <baserelativehumidity>: the relative humidity, 0 ... 1. Anything else (no number, a NaN,
///   a value outside that range) gives "Illegal base humidity specified, ignoring" (without a
///   full stop, as in OpenRocket) and leaves a humidity read before as it is.
/// - any other child: AbstractElementHandler's warnings for its text and its attributes.
///
/// storeSettings() first applies the pressure, the temperature and the humidity, each when
/// there is one, and then the model attribute, compared exactly:
/// - "isa": the ISA atmosphere, which replaces the three values by the standard atmosphere's
///   at the launch altitude the options have at that moment;
/// - "extendedisa": the three values as they now are;
/// - anything else, and no attribute: the ISA atmosphere with "Unknown atmospheric model,
///   using ISA.".
///
/// Deviations from OpenRocket:
/// - An infinite temperature or pressure is refused like a NaN (decision U3: the loader applies
///   no simulation option that is not finite). OpenRocket stores a temperature of either
///   infinity and a pressure of positive infinity, and makes 0.001 Pa of a pressure of negative
///   infinity.
/// - Java's handler also takes the loading context, which it never uses.
class AtmosphereHandler final : public AbstractElementHandler
{
public:
    /// A handler of an <atmosphere> element whose model attribute is @p model (none: the
    /// element has no such attribute).
    explicit AtmosphereHandler(std::optional<std::string> model);

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

    /// Applies what the element said to @p options (storeSettings(); see the class comment).
    void storeSettings(SimulationOptions& options, WarningSet& warnings) const;

    /// The model attribute, or none.
    [[nodiscard]] const std::optional<std::string>& getModel() const noexcept { return m_model; }
    /// The temperature read, in K, or NaN for none. Never an infinity.
    [[nodiscard]] double getTemperature() const noexcept { return m_temperature; }
    /// The pressure read, in Pa and at least 0.001, or NaN for none. Never an infinity.
    [[nodiscard]] double getPressure() const noexcept { return m_pressure; }
    /// The relative humidity read, 0 ... 1, or NaN for none.
    [[nodiscard]] double getHumidity() const noexcept { return m_humidity; }

private:
    std::optional<std::string> m_model;
    double                     m_temperature{std::numeric_limits<double>::quiet_NaN()};
    double                     m_pressure{std::numeric_limits<double>::quiet_NaN()};
    double                     m_humidity{std::numeric_limits<double>::quiet_NaN()};
};

}  // namespace QtRocket
