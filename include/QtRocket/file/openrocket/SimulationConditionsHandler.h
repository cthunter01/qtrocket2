#pragma once

#include <memory>
#include <string_view>

#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/AtmosphereHandler.h"
#include "QtRocket/file/openrocket/CsvLookupHandler.h"
#include "QtRocket/file/openrocket/GravityHandler.h"
#include "QtRocket/file/openrocket/WindHandler.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// Reads the <conditions> element of a simulation: the simulation options of the file and the
/// id of the flight configuration the simulation flies (OpenRocket's
/// file/openrocket/importt/SimulationConditionsHandler). The simulation's handler makes one
/// per <conditions> element and, when the simulation closes, takes the options
/// (getConditions()) and the id (getIdToSet()).
///
/// The options start as SimulationOptions(the context's preference store) makes them, but with
/// the FLAT geodetic computation: that is the setting of a file that does not name one
/// (OpenRocket's "default loading settings", which differ from the defaults of a new
/// simulation). An element the file lacks therefore leaves the store's value. The elements are
/// applied in the order of the file, each when it closes, so a later one wins.
///
/// The children that are plain text. "A number" is the text read with Double.parseDouble
/// (white space around it, an exponent, a hexadecimal number and a type suffix are all fine;
/// OpenRocket's own "Inf" is not), and where a warning is named it is given for a text that is
/// no number and for a number that is not finite, and the element is then not applied:
/// - <configid>: the flight configuration id, by FlightConfigurationId::fromString() of the
///   text as it is: an empty text is a new random id, a text that is no UUID an id made of its
///   hash. Without the element the id stays the error id, which the simulation's handler must
///   not hand to a Simulation (decision L8).
/// - <launchrodlength>: a number in m; "Illegal launch rod length defined, ignoring.".
/// - <launchintowind>: Boolean.parseBoolean of the text as it is: "true" in any case is true,
///   everything else (" true", "1", "yes") false.
/// - <launchrodangle>: a number in degrees, clamped to +-60 degrees by the options; "Illegal
///   launch rod angle defined, ignoring.".
/// - <launchroddirection>: a number in degrees, reduced to one turn by the options; "Illegal
///   launch rod direction defined, ignoring.".
/// - <windaverage>, <windturbulence>, <winddirection>: the wind elements of OpenRocket 23.09
///   and older, which newer files still carry before their <wind> elements. They go to the
///   average wind model: the speed in m/s (PinkNoiseWindModel::setAverage()), the turbulence
///   intensity (setTurbulenceIntensity(), which makes a standard deviation of it with the
///   average the model has at that moment) and the direction in radians, not degrees
///   (setDirection()); "Illegal average windspeed defined, ignoring.", "Illegal wind
///   turbulence intensity defined, ignoring." and "Illegal wind direction defined, ignoring.".
///   They do not choose the wind model in use.
/// - <windmodeltype>: "Average" or "MultiLevel", the case ignored and nothing trimmed
///   (windModelTypeFromString()): the wind model in use. Any other text fails the load (see
///   below).
/// - <launchaltitude>: a number in m, capped by the options at the highest altitude the
///   atmospheric model allows; with the ISA atmosphere in use the launch temperature, pressure
///   and humidity follow; "Illegal launch altitude defined, ignoring.".
/// - <launchlatitude>, <launchlongitude>: numbers in degrees, clamped to +-90 and +-180 by the
///   options; "Illegal launch latitude defined, ignoring." and "Illegal launch longitude.".
/// - <geodeticmethod>: "flat", "spherical" or "wgs84", read as DocumentConfig::findEnum()
///   reads an enum (trimmed, then compared exactly; "WGS84" is unknown); otherwise "Unknown
///   geodetic computation method '<text>'" (the text as it is, no full stop).
/// - <simulationsteppermethod>: "rk4" or "rk6", read the same way; otherwise "Unknown
///   Simulation Stepper '<text>'". Applying it also writes the choice into the context's
///   preference store (SimulationOptions::setSimulationStepperMethodChoice()), so loading a
///   file changes the stepper that new simulations start with, exactly as in OpenRocket, whose
///   loader writes the application preferences here (decision L8).
/// - <randomseed>: Integer.parseInt of the trimmed text; the seed, which is then fixed.
///   Otherwise "Illegal random seed defined, ignoring.".
/// - <timestep>, <maxtime>: a number above zero, in s; "Illegal time step defined, ignoring."
///   and "Illegal max simulation time defined, ignoring." also for zero and a negative number.
/// - <recoveryspeedwarning>, <drogueLowspeedwarning> (with its capital L),
///   <recoverydroguemainhighspeedwarning>, <recoverydroguemainlowspeedwarning>: the four
///   deployment speed thresholds in m/s, applied when they are a number above zero and passed
///   over silently otherwise.
/// - <draglookupcsv>, <stabilitylookupcsv>: the lookup table elements of older files, whose
///   trimmed text is the CSV file of the table (a relative one is resolved against the current
///   directory of the process). A blank text is nothing. Each is read only as long as no
///   <draglookup> (or <stabilitylookup>) element was opened before it in these conditions. The
///   table is read as CsvLookupHandler::loadFromFile() reads it; a failure gives "Failed to
///   load drag lookup CSV '<file>', ignoring. Reason: <reason>" (or "stability lookup CSV").
/// - any other child: nothing, and no warning. As for every plain-text child, a child element
///   of it gives PlainTextHandler's "Unknown element <child>, ignoring." and DelegatorHandler's
///   slip.
///
/// The children with a handler of their own, each made new for every such element and asked to
/// store its settings when the element closes: <wind> (WindHandler), <atmosphere>
/// (AtmosphereHandler), <gravity> (GravityHandler), <draglookup> and <stabilitylookup>
/// (CsvLookupHandler, which stores at its own end).
///
/// Failures, which end the load (decision L2; ErrorCode::INVALID_ARGUMENT with Java's message):
/// - a <windmodeltype> that names no type: "No enum constant
///   info.openrocket.core.models.wind.WindModelType for string value: <text>";
/// - what WindHandler fails with: a <windlevel> attribute that is no number, two wind levels
///   at one altitude.
/// The options keep what was applied before the failure.
///
/// Deviations from OpenRocket:
/// - Numbers that are not finite are not applied (decision U3). Where OpenRocket has a warning
///   for a NaN, an infinity now gives the same warning. Without this, OpenRocket stores an
///   infinite rod length, wind speed, turbulence intensity, time step, maximum time and
///   threshold and a launch altitude of negative infinity, clamps an infinite rod angle,
///   latitude, longitude and a launch altitude of positive infinity, and makes a NaN of an
///   infinite rod or wind direction. A threshold of positive infinity, which OpenRocket stores
///   without a word, gives Warning::kFileInvalidParameter ("Invalid parameter encountered,
///   ignoring.").
/// - A finite number is refused in the same way when an option would not be finite with it: a
///   <launchroddirection> of more than about 2.8e307 degrees, which is past the range of a
///   double in radians (OpenRocket stores a NaN), and a <windaverage> or <windturbulence>
///   whose product with the turbulence intensity or the wind speed of the average wind model
///   is (WindHandler::setAverageWind(); OpenRocket stores the infinity). Every other finite
///   number is applied as OpenRocket applies it, however large.
/// - <randomseed> reads ASCII digits only; Integer.parseInt also takes the decimal digits of
///   other scripts.
/// - Without a preference store in the context the constructor is a BugError (Java asks the
///   application's preferences, which are always there); whoever makes the context sets one.
/// - Java's constructor also takes the rocket, which it never uses. The id is read through
///   getIdToSet() (Java: the public field idToSet).
class SimulationConditionsHandler final : public AbstractElementHandler
{
public:
    /// A handler whose options start from the preference store of @p context. The store must
    /// outlive the options, wherever they go (see SimulationOptions); the context itself is
    /// not kept.
    /// @throws BugError when the context has no preference store
    explicit SimulationConditionsHandler(const DocumentLoadingContext& context);

    /// The options read so far (getConditions()). They are the handler's own: the simulation's
    /// handler moves them into the Simulation it makes (std::move(handler.getConditions()),
    /// which is what handing Java's object over amounts to), and must not use the handler's
    /// options after that.
    [[nodiscard]] SimulationOptions&       getConditions() noexcept { return m_options; }
    [[nodiscard]] const SimulationOptions& getConditions() const noexcept { return m_options; }

    /// The flight configuration id of <configid>, or FlightConfigurationId::errorId() when the
    /// element was not there (idToSet). A file can also spell the error id out.
    [[nodiscard]] const FlightConfigurationId& getIdToSet() const noexcept { return m_idToSet; }

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

private:
    // closeElement() in parts, each for some of the children; false when @p element is none of
    // them.

    /// The children that are one number each.
    bool closeNumberElement(std::string_view element, std::string_view content,
                            WarningSet& warnings);
    /// <configid>, <launchintowind>, <geodeticmethod>, <simulationsteppermethod> and
    /// <randomseed>.
    bool closeTextElement(std::string_view element, std::string_view content, WarningSet& warnings);
    /// <wind>, <atmosphere> and <gravity>: the settings of their handlers.
    bool closeHandledElement(std::string_view element, WarningSet& warnings);
    /// <draglookupcsv> and <stabilitylookupcsv>.
    void closeLegacyLookupElement(std::string_view element, std::string_view content,
                                  WarningSet& warnings);

    FlightConfigurationId              m_idToSet{FlightConfigurationId::errorId()};
    SimulationOptions                  m_options;
    std::unique_ptr<AtmosphereHandler> m_atmosphereHandler;
    std::unique_ptr<WindHandler>       m_windHandler;
    std::unique_ptr<GravityHandler>    m_gravityHandler;
    std::unique_ptr<CsvLookupHandler>  m_dragLookupHandler;
    std::unique_ptr<CsvLookupHandler>  m_stabilityLookupHandler;
};

}  // namespace QtRocket
