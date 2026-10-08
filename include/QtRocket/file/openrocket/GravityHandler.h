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

/// Reads one <gravity> element of a simulation's <conditions> (OpenRocket's
/// file/openrocket/importt/GravityHandler). The conditions handler makes one per <gravity>
/// element with the element's model attribute, and when the element closes has it store what
/// it read (storeSettings()).
///
/// The children:
/// - <value>: the gravity of the constant model in m/s^2, read with Double.parseDouble. A text
///   that is no number and a value that is not finite give "Illegal gravity value specified,
///   ignoring." and no value; the last <value> counts, so one that is refused also takes back
///   an earlier one. A negative value and zero are taken as they are.
/// - any other child: AbstractElementHandler's warnings for its text and its attributes
///   ("Unknown text in element '<e>', ignoring.", "Unknown attributes in element '<e>',
///   ignoring."). The attributes of <value> are not looked at.
///
/// storeSettings() goes by the model attribute, compared exactly:
/// - "wgs": the WGS gravity model; a value is not applied.
/// - "constant": the constant gravity model, and the value when there is one (without one the
///   options keep the value they have).
/// - anything else, and no attribute: the WGS model with "Unknown gravity model type
///   '<model>', using WGS." ('null' without the attribute, as Java prints it).
///
/// Deviation from OpenRocket:
/// - An infinite <value> is refused like a NaN (decision U3: the loader applies no simulation
///   option that is not finite). OpenRocket stores it, and a flight under it ends in an error.
class GravityHandler final : public AbstractElementHandler
{
public:
    /// A handler of a <gravity> element whose model attribute is @p model (none: the element
    /// has no such attribute).
    explicit GravityHandler(std::optional<std::string> model);

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
    /// The value read, in m/s^2, or NaN when none was read or the last one was refused. Never
    /// an infinity.
    [[nodiscard]] double getConstantValue() const noexcept { return m_constantValue; }

private:
    std::optional<std::string> m_model;
    double                     m_constantValue{std::numeric_limits<double>::quiet_NaN()};
};

}  // namespace QtRocket
