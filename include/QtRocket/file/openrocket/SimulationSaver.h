#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "QtRocket/logging/WarningSet.h"

namespace QtRocket
{

class FlightDataBranch;
class LandingDispersionSettings;
class OrkWriter;
class Simulation;

/// Writes one simulation of a document into the text of an .ork file: the <simulation> element
/// that OpenRocketSaver writes for each simulation inside <simulations> (OpenRocket's
/// OpenRocketSaver.saveSimulation() with the methods it calls, saveLandingDispersionSettings(),
/// writeEntry() through OrkWriter::entry(), saveFlightDataBranch(), writeDataPointString() and
/// writeCsvLookup()). Every line goes through the writer, and every text and attribute value
/// through OrkWriter::escape() (decision E3 of tier 10).
///
/// What saveSimulation() writes, in Java's order:
/// - `<simulation status="...">`: the status as Simulation::getStatus() works it out when the
///   data is saved, NOT_SIMULATED otherwise, lower case without '_' ("notsimulated");
/// - <name>, <simulator>RK4Simulator</simulator>, <calculator>BarrowmanCalculator</calculator>;
/// - <conditions>: the configuration id; the launch rod length, whether it points into the
///   wind, its angle in degrees (angle * 180 / pi) and its EFFECTIVE direction in degrees
///   (SimulationOptions::getLaunchRodDirection(): the wind's direction when the rod points into
///   the wind, as in Java); the average wind model three times over (<windaverage>,
///   <windturbulence>, <winddirection>, then <wind model="average"> with the speed, direction
///   and standard deviation); the multi-level wind model when it has levels (<wind
///   model="multilevel" altituderef="msl|agl"> with one <windlevel> per level); the wind model
///   type ("Average", "MultiLevel"); the launch altitude, latitude and longitude; the geodetic
///   computation and the stepper (lower case); <randomseed> only when the seed is fixed; the
///   atmosphere (<atmosphere model="isa"/>, or "extendedisa" with the base temperature,
///   pressure and humidity); the gravity (<gravity model="wgs"/>, or "constant" with its
///   value); the time step, the maximum time and the four recovery warning speeds; then a
///   <draglookup> and a <stabilitylookup> when the options have a table or a path for it (see
///   the lookup tables below);
/// - <landingdispersion> when the simulation has landing dispersion settings;
/// - <plotappearance> when it has plot appearances: one <series> per symbol, in the order of
///   String.compareTo() (Java's TreeMap), with the line style and the color each has;
/// - one <extension extensionid="..."> per simulation extension with an <entry> per key of its
///   configuration, in the configuration's order, each with its Java type (OrkWriter::entry()
///   without explicit number types: an Integer 5 is "5", a Double "5.0", a Float through
///   Float.toString());
/// - <flightdata> when the simulation has simulated data: the summary values that are not NaN
///   as attributes (Strings::doubleToString() with 6 decimals); every stored warning, in the
///   order of the set (`<warning type="...">`, the type being Java's simple class name,
///   Warning::typeName(); the id, the description, the priority, the id of every source, the
///   angle of a LargeAOA or the speed of a RecoveryHighSpeedDeployment as <parameter>, and the
///   whole text of the warning as a line of its own); the branches when the data is saved or
///   the status is EXTERNAL (saveFlightDataBranch()).
/// Simulation::getStatus() is called where Java calls it: for the status attribute when the
/// data is saved, and for every simulation with simulated data when the branches are decided
/// on. It stores the status it works out (LOADED becomes OUTDATED when the options changed),
/// as Java's does; nothing else of the simulation changes.
///
/// The lookup tables (Java's writeCsvLookup()): `<draglookup file="<path>">`, the attribute
/// only when the options hold a path (written as it is held: absolute, normalised, with the
/// platform's separators), then one <row> per CSV line of the table that is not blank after
/// String.trim(), as the line is. The lines are the rows the options hold; when they hold none,
/// the file of the path is read again (CsvMachAoALookup::readLines(), bounded and strict UTF-8),
/// never while there are rows. When that read fails, the element is written without rows and
/// the save adds the warning lookupRowsNotSaved().
///
/// Deviations from OpenRocket:
/// - Every text and attribute value goes through OrkWriter::escape(), also those Java writes
///   unescaped (the keys of an extension's configuration, the attributes of the landing
///   dispersion settings); see OrkWriter for what that changes.
/// - The landing dispersion settings are the texts a file gave (LandingDispersionSettings,
///   until the Monte Carlo analysis exists: HOOK(monte-carlo)): the element's attributes "runs"
///   and "seed" first, then the others by name; the uncertainties in their stored order, each
///   with "parameter", "distribution" and "spread" first and then its other attributes by name;
///   every value escaped, and an attribute that is not there left out. These are the one place
///   where the names of attributes come from the design, so an attribute that the loader could
///   not read back is left out too (attributesLeftOut(); decision 3 of tier 10, never write an
///   unreadable file, and the user's answer of 2026-10-16): one whose name is not a name of an XML
///   1.0 document without a prefix of at most XmlScanner::kMaxNameLength characters, or is
///   "xmlns" (XmlScanner::isPlainAttributeName(); a caller can make such a name, and so can an
///   XML 1.1 file, whose names are wider, or a name of characters that are not well-formed
///   UTF-8), and every attribute of an element beyond its first XmlScanner::kMaxAttributes.
///   OpenRocketSaver warns of them (dispersionAttributesLeftOut()). Java writes its
///   MonteCarloSettings: the number of runs and the seed, then the uncertainties in the order of
///   its parameters, each spread as Double.toString() prints it.
/// - A source of a stored warning that names no component of the rocket is written with the id
///   the warning keeps (MessageSource::removed(), from the file that was loaded); Java writes
///   the id of its REMOVED component, a random one per process.
/// - An EventAfterLanding warning without an event gives a SIM_WARN event without "eventid"
///   (Java dies of a NullPointerException).
/// - The file of a lookup table is read through CsvMachAoALookup's bounded read, and a read that
///   fails, a missing file included, gives the warning lookupRowsNotSaved() (Java skips a
///   missing file and only logs a failed read).
/// - A null extension in the simulation's list is left out (Java dies of a
///   NullPointerException).
/// - With a multi-level wind and the rod pointing into the wind, the effective direction is the
///   wind's at time 0, pink noise included, taken through atan2 (decision O11 of tier 10): its
///   text depends on the noise generator and the platform's libm, in Java too.
class SimulationSaver final
{
public:
    SimulationSaver() = delete;

    /// Writes the <simulation> element of @p simulation (Java: saveSimulation()), with the
    /// branches of its data when @p saveSimulationData or when its status is EXTERNAL. Save
    /// warnings go to @p warnings.
    static void saveSimulation(Simulation& simulation, bool saveSimulationData, OrkWriter& writer,
                               WarningSet& warnings);

    /// Writes the <databranch> element of @p branch (Java: saveFlightDataBranch()): the name, the
    /// optimum altitude and its time when they are not NaN (Double.toString()), the save keys of
    /// the types in the order of FlightDataBranch::getTypes(); one <event> per event in its
    /// order (time, type, id; the source's id when it has one; for a SIM_WARN the warning's id
    /// and, for an EventAfterLanding with an event, that event's id; for a SIM_ABORT the cause),
    /// then one <datapoint> per row, every value with 6 decimals
    /// (Strings::doubleToString()).
    static void saveFlightDataBranch(const FlightDataBranch& branch, OrkWriter& writer);

    /// How many attributes of @p settings saveSimulation() leaves out (see the deviations): of
    /// the element and of every uncertainty, those whose names a design file cannot hold and
    /// those beyond the first XmlScanner::kMaxAttributes of an element.
    [[nodiscard]] static std::size_t attributesLeftOut(const LandingDispersionSettings& settings);

    /// The save warning for a lookup table whose rows could not be embedded: the options of the
    /// simulation @p simulation hold no rows for the element @p element ("draglookup",
    /// "stabilitylookup") and its file @p file could not be read: "The rows of the draglookup
    /// table of the simulation "Sim" were not saved: the file "/x/drag.csv" could not be read."
    [[nodiscard]] static std::string lookupRowsNotSaved(std::string_view element,
                                                        std::string_view simulation,
                                                        std::string_view file);
};

}  // namespace QtRocket
