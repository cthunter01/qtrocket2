#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class SimulationOptions;

/// Reads one <wind> element of a simulation's <conditions> (OpenRocket's
/// file/openrocket/importt/WindHandler). The conditions handler makes one per <wind> element
/// with the element's model attribute, the options it fills and the element's attributes, and
/// when the element closes has it choose the wind model (storeSettings()). Unlike the handlers
/// of <atmosphere> and <gravity> this one writes into the options as it reads.
///
/// The model attribute is compared exactly ("average", "multilevel"); with any other value, or
/// without the attribute, the element's children are ignored without a word.
///
/// model="average", the children each read with Double.parseDouble and applied to the average
/// wind model of the options:
/// - <speed>: PinkNoiseWindModel::setAverage() (a negative speed turns the direction, and the
///   standard deviation follows the speed so that the turbulence intensity stays);
/// - <direction>: setDirection(), in radians;
/// - <standarddeviation>: setStandardDeviation().
/// A text that is no number and a NaN are passed over silently, and so is any other child.
///
/// model="multilevel": making the handler removes every level of the multi-level wind model
/// (so a second such element drops the levels of the first, and the level the options start
/// with never stays) and sets the altitude reference from the altituderef attribute ("msl" or
/// "agl", read as DocumentConfig.findEnum reads an enum: trimmed, then compared exactly;
/// without the attribute the reference stays what it is). Every <windlevel> child adds a level
/// from its attributes altitude, speed, direction and standarddeviation, each read with
/// Double.parseDouble (MultiLevelPinkNoiseWindModel::addWindLevel(), which keeps the levels
/// sorted by altitude). Any other child is passed over silently.
///
/// Failures, which end the load (decision L2; ErrorCode::INVALID_ARGUMENT with Java's message):
/// - an attribute of a <windlevel> that is no number: `For input string: "<text>"`, "empty
///   String" or "multiple points" (DocumentConfig::parseDouble());
/// - a second level at an altitude: "Wind level already exists for altitude: <altitude>". The
///   levels read before it stay in the options.
///
/// storeSettings(): "average" makes the average model the wind model in use, "multilevel" the
/// multi-level one; anything else changes nothing and gives "Unknown wind model type
/// '<model>', using default." ('null' without the attribute, as Java prints it).
///
/// Deviations from OpenRocket:
/// - Values that are not finite are not applied (decision U3). An infinite <speed>,
///   <direction> or <standarddeviation> of the average model gives
///   Warning::kFileInvalidParameter ("Invalid parameter encountered, ignoring."), since
///   OpenRocket has no warning there: it stores the infinity (an infinite direction as a NaN).
///   A <windlevel> with an attribute that is a NaN or an infinity is not added and gives the
///   same warning; OpenRocket adds it, and fails the load for a second level whose altitude is
///   a NaN.
/// - The same holds for a finite number that would make the average model compute one that is
///   not finite (setAverageWind()): a <speed> takes the turbulence intensity along, so that a
///   huge standard deviation over a small speed becomes an infinite one at the next <speed>.
///   OpenRocket stores that infinity.
/// - A <windlevel> that lacks one of its four attributes is not added and gives
///   Warning::kFileInvalidParameter (decision D9). OpenRocket ends with a
///   NullPointerException. The attributes are looked at in the order altitude, speed,
///   direction, standarddeviation, and the first that is missing or is no number decides, as
///   in Java: `<windlevel speed="abc"/>` is skipped, `<windlevel altitude="abc"/>` fails the
///   load.
/// - An altituderef that names no reference ("AGL", "") sets the reference to mean sea level
///   and gives "Unknown wind altitude reference '<text>', using MSL." (decision L9, QtRocket's
///   own text). OpenRocket stores null, which fails later in the simulation or the saver.
/// - The constructor takes the warning set, for that warning.
class WindHandler final : public AbstractElementHandler
{
public:
    /// A handler of a <wind> element whose model attribute is @p model (none: the element has
    /// no such attribute) and whose attributes are @p attributes, filling @p options, which
    /// must outlive the handler. For the model "multilevel" this already clears the levels and
    /// sets the altitude reference (see the class comment).
    WindHandler(std::optional<std::string> model, SimulationOptions& options,
                const Attributes& attributes, WarningSet& warnings);

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

    /// Makes the model the element names the wind model in use of @p options (storeSettings();
    /// see the class comment).
    void storeSettings(SimulationOptions& options, WarningSet& warnings) const;

    /// The model attribute, or none.
    [[nodiscard]] const std::optional<std::string>& getModel() const noexcept { return m_model; }

    /// Calls the setter @p setter of the average wind model of @p options with @p value,
    /// unless the model would then hold a number that is not finite: false then, and nothing
    /// changes. This is decision U3 for what a number of the file makes the model compute: a
    /// wind speed takes the turbulence intensity along (PinkNoiseWindModel::setAverage()) and
    /// a turbulence intensity is multiplied by the wind speed (setTurbulenceIntensity()), and
    /// either product can leave the range of a double although both numbers are finite.
    /// OpenRocket stores the infinity. The conditions handler applies the wind elements of
    /// older files with it.
    [[nodiscard]] static bool setAverageWind(SimulationOptions& options,
                                             void (PinkNoiseWindModel::*setter)(double),
                                             double value);

private:
    /// Reads @p content as the <speed>, <direction> or <standarddeviation> of the average
    /// model that @p element is.
    void closeAverageElement(std::string_view element, std::string_view content,
                             WarningSet& warnings);
    /// Adds the level of a <windlevel> with @p attributes.
    [[nodiscard]] Result<void> closeWindLevel(const Attributes& attributes, WarningSet& warnings);

    std::optional<std::string> m_model;
    SimulationOptions*         m_options;
};

}  // namespace QtRocket
