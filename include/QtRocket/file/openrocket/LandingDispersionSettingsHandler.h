#pragma once

#include <string_view>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/LandingDispersionSettings.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// Reads the <landingdispersion> element of a simulation, the optional settings of a
/// landing-dispersion analysis (OpenRocket's
/// file/openrocket/importt/LandingDispersionSettingsHandler). The simulation's handler makes
/// one per <landingdispersion> element, with the attributes of the element, and takes
/// getSettings() when the simulation closes; a second <landingdispersion> element of a
/// simulation is a second handler, whose settings replace the first's.
///
///     <landingdispersion runs="500" seed="12345">
///       <uncertainty parameter="windspeed" distribution="normal" spread="0.5"/>
///     </landingdispersion>
///
/// QtRocket has no landing-dispersion analysis yet, so the handler keeps what the element
/// holds and understands none of it (see LandingDispersionSettings): the attributes the
/// element opens with, and for every <uncertainty> child, in the order of the file, the
/// attributes that child closes with. Text in the element and in an <uncertainty> is ignored.
/// A child of an <uncertainty> gives PlainTextHandler's "Unknown element <child>, ignoring."
/// and DelegatorHandler's slip: the <uncertainty> then closes with the child's attributes,
/// which are what is kept for it, as OpenRocket would read them.
///
/// Any other child of <landingdispersion> is ignored with "Unknown landing-dispersion element
/// '<name>', ignoring.", which shifts the attributes of the elements around it by one
/// (DelegatorHandler): the simulation then ends with the attributes of the <landingdispersion>
/// element in place of its own and does not see its status. That warning is OpenRocket's and
/// is kept, because which child a handler refuses decides what the elements around it are
/// told.
///
/// Deviations from OpenRocket:
/// - Nothing is validated, and the eight warnings of OpenRocket's validation are not given:
///   "Missing landing-dispersion run count, ignoring settings.", "Missing landing-dispersion
///   seed, ignoring settings.", "Invalid landing-dispersion run count, ignoring settings.",
///   "Invalid landing-dispersion seed, ignoring settings.", "Invalid landing-dispersion
///   uncertainty parameter or distribution, ignoring.", "Invalid landing-dispersion
///   uncertainty spread, ignoring.", "Invalid landing-dispersion uncertainty, ignoring." and
///   "Invalid landing-dispersion settings, ignoring.". Where OpenRocket gives one of the first
///   four or the last, the simulation has no settings there; here it has the element as read.
///   Where it gives one of the other three, it drops that uncertainty; here it is kept. See
///   LandingDispersionSettings for the rest of what OpenRocket's settings do with what they
///   read. "HOOK(monte-carlo)" in the source file marks where the validation belongs.
/// - The constructor takes the attributes only; Java's also takes the warning set, for the
///   first four of those warnings.
/// - getSettings() returns the settings by reference, never "none" (Java: null for settings
///   it refused).
class LandingDispersionSettingsHandler final : public AbstractElementHandler
{
public:
    /// A handler of a <landingdispersion> element that opened with @p attributes.
    explicit LandingDispersionSettingsHandler(const Attributes& attributes);

    /// What was read so far (getSettings()): the attributes of the element and the
    /// uncertainties that have closed.
    [[nodiscard]] const LandingDispersionSettings& getSettings() const noexcept
    {
        return m_settings;
    }

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    /// Keeps the attributes of an <uncertainty>; never fails.
    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

private:
    LandingDispersionSettings m_settings;
};

}  // namespace QtRocket
