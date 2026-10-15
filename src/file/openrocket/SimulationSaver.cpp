#include "QtRocket/file/openrocket/SimulationSaver.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <map>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/aero/lookup/CsvMachAoALookup.h"
#include "QtRocket/file/openrocket/OrkWriter.h"
#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/LandingDispersionSettings.h"
#include "QtRocket/simulation/PlotAppearance.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

namespace
{

/// TextUtil.doubleToString(value, STORAGE_DECIMAL_PLACES): how flight data is stored.
[[nodiscard]] std::string storageText(double value)
{
    return Strings::doubleToString(value, Strings::kStorageDecimalPlaces);
}

// ------------------------------------------------------------------------------ conditions

/// The wind elements of the conditions: the average model three times over and the
/// multi-level model when it has levels.
void writeWind(const SimulationOptions& options, OrkWriter& writer)
{
    const PinkNoiseWindModel& average = options.getAverageWindModel();

    // TODO: remove once support for OR 23.09 and prior is dropped (Java)
    writer.element("windaverage", average.getAverage());
    writer.element("windturbulence", average.getTurbulenceIntensity());
    writer.element("winddirection", average.getDirection());

    writer.line("<wind model=\"average\">");
    writer.indent();
    writer.element("speed", average.getAverage());
    writer.element("direction", average.getDirection());
    writer.element("standarddeviation", average.getStandardDeviation());
    writer.outdent();
    writer.line("</wind>");

    const MultiLevelPinkNoiseWindModel& multiLevel = options.getMultiLevelWindModel();
    const std::vector<const MultiLevelPinkNoiseWindModel::LevelWindModel*> levels =
        multiLevel.getLevels();
    if (!levels.empty())
    {
        writer.line(std::format(R"(<wind model="multilevel" altituderef="{}">)",
                                toString(multiLevel.getAltitudeReference())));
        writer.indent();
        for (const MultiLevelPinkNoiseWindModel::LevelWindModel* level : levels)
        {
            writer.line(std::format(
                R"(<windlevel altitude="{}" speed="{}" direction="{}" standarddeviation="{}"/>)",
                Strings::javaDoubleToString(level->getAltitude()),
                Strings::javaDoubleToString(level->getSpeed()),
                Strings::javaDoubleToString(level->getDirection()),
                Strings::javaDoubleToString(level->getStandardDeviation())));
        }
        writer.outdent();
        writer.line("</wind>");
    }

    writer.element("windmodeltype", toStringValue(options.getWindModelType()));
}

/// The atmosphere and the gravity of the conditions.
void writeAtmosphereAndGravity(const SimulationOptions& options, OrkWriter& writer)
{
    if (options.isIsaAtmosphere())
    {
        writer.line("<atmosphere model=\"isa\"/>");
    }
    else
    {
        writer.line("<atmosphere model=\"extendedisa\">");
        writer.indent();
        writer.element("basetemperature", options.getLaunchTemperature());
        writer.element("basepressure", options.getLaunchPressure());
        writer.element("baserelativehumidity", options.getLaunchRelativeHumidity());
        writer.outdent();
        writer.line("</atmosphere>");
    }

    // Gravity model
    if (options.getGravityModelType() == GravityModelType::WGS)
    {
        writer.line("<gravity model=\"wgs\"/>");
    }
    else if (options.getGravityModelType() == GravityModelType::CONSTANT)
    {
        writer.line("<gravity model=\"constant\">");
        writer.indent();
        writer.element("value", options.getConstantGravity());
        writer.outdent();
        writer.line("</gravity>");
    }
}

/// Java's writeCsvLookup(element, csvPath, csvRows): the element of a lookup table of the
/// simulation named @p simulation (see the header for the rows and the warning).
void writeCsvLookup(std::string_view element, const std::optional<std::filesystem::path>& csvPath,
                    const std::optional<std::vector<std::string>>& csvRows,
                    std::string_view simulation, OrkWriter& writer, WarningSet& warnings)
{
    std::string fileAttribute;
    if (csvPath.has_value())
    {
        fileAttribute = std::format(R"( file="{}")", writer.escape(pathToUtf8(*csvPath)));
    }

    writer.line(std::format("<{}{}>", element, fileAttribute));
    writer.indent();

    // Use stored CSV rows if available, otherwise read from file
    std::optional<std::vector<std::string>> fileRows;
    const std::vector<std::string>*         rows = nullptr;
    if (csvRows.has_value() && !csvRows->empty())
    {
        rows = &*csvRows;
    }
    else if (csvPath.has_value())
    {
        Result<std::vector<std::string>> lines = CsvMachAoALookup::readLines(*csvPath);
        if (lines.has_value())
        {
            fileRows = std::move(*lines);
            rows     = &*fileRows;
        }
        else
        {
            // If we can't read the file, just save the path reference (Java; QtRocket says so)
            warnings.add(
                SimulationSaver::lookupRowsNotSaved(element, simulation, pathToUtf8(*csvPath)));
        }
    }
    if (rows != nullptr)
    {
        for (const std::string& line : *rows)
        {
            if (!Strings::trim(line).empty())
            {
                writer.element("row", line);
            }
        }
    }

    writer.outdent();
    writer.line(std::format("</{}>", element));
}

/// The <conditions> element of @p simulation.
void writeConditions(const Simulation& simulation, OrkWriter& writer, WarningSet& warnings)
{
    const SimulationOptions& options = simulation.getOptions();

    writer.line("<conditions>");
    writer.indent();

    writer.element("configid", simulation.getId().toString());
    writer.element("launchrodlength", options.getLaunchRodLength());
    writer.element("launchintowind", options.getLaunchIntoWind());
    writer.element("launchrodangle", MathUtil::rad2deg(options.getLaunchRodAngle()));
    writer.element("launchroddirection",
                   (options.getLaunchRodDirection() * 360.0) / (2.0 * std::numbers::pi));

    writeWind(options, writer);

    writer.element("launchaltitude", options.getLaunchAltitude());
    writer.element("launchlatitude", options.getLaunchLatitude());
    writer.element("launchlongitude", options.getLaunchLongitude());
    writer.element("geodeticmethod", toString(options.getGeodeticComputation()));
    writer.element("simulationsteppermethod", toString(options.getSimulationStepperMethodChoice()));
    if (options.isRandomSeedFixed())
    {
        writer.element("randomseed", options.getRandomSeed());
    }

    writeAtmosphereAndGravity(options, writer);

    writer.element("timestep", options.getTimeStep());
    writer.element("maxtime", options.getMaxSimulationTime());
    writer.element("recoveryspeedwarning", options.getRecoverySpeedWarning());
    writer.element("drogueLowspeedwarning", options.getDrogueLowSpeedWarning());
    writer.element("recoverydroguemainhighspeedwarning",
                   options.getRecoveryDrogueMainHighSpeedWarning());
    writer.element("recoverydroguemainlowspeedwarning",
                   options.getRecoveryDrogueMainLowSpeedWarning());
    if (options.getDragLookupCsvPath().has_value() || options.getDragLookupTable() != nullptr)
    {
        writeCsvLookup("draglookup", options.getDragLookupCsvPath(), options.getDragLookupCsvRows(),
                       simulation.getName(), writer, warnings);
    }
    if (options.getStabilityLookupCsvPath().has_value() ||
        options.getStabilityLookupTable() != nullptr)
    {
        writeCsvLookup("stabilitylookup", options.getStabilityLookupCsvPath(),
                       options.getStabilityLookupCsvRows(), simulation.getName(), writer, warnings);
    }

    writer.outdent();
    writer.line("</conditions>");
}

// ---------------------------------------------------------------------- landing dispersion

/// Whether @p name can be the name of an XML attribute, as far as the saver checks: not empty,
/// starting with a letter, '_', ':' or a byte of a multi-byte character, and going on with
/// those, digits, '-' and '.'.
[[nodiscard]] bool isXmlName(std::string_view name) noexcept
{
    const auto startsName = [](char c) {
        const auto byte = static_cast<unsigned char>(c);
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == ':' ||
               byte >= 0x80;
    };
    if (name.empty() || !startsName(name.front()))
    {
        return false;
    }
    return std::ranges::all_of(name, [&startsName](char c) {
        return startsName(c) || (c >= '0' && c <= '9') || c == '-' || c == '.';
    });
}

/// The attributes @p attributes as XML text, each with a space before it: those named in
/// @p first first, in that order, then the others by name; an attribute that is not there, or
/// whose name is no XML name, is left out.
[[nodiscard]] std::string attributesText(const LandingDispersionSettings::Attributes& attributes,
                                         std::initializer_list<std::string_view>      first,
                                         OrkWriter&                                   writer)
{
    std::string text;
    const auto  append = [&text, &writer](std::string_view name, std::string_view value) {
        if (isXmlName(name))
        {
            text += std::format(R"( {}="{}")", name, writer.escape(value));
        }
    };
    for (const std::string_view name : first)
    {
        const auto found = attributes.find(name);
        if (found != attributes.end())
        {
            append(name, found->second);
        }
    }
    for (const auto& [name, value] : attributes)
    {
        if (std::ranges::find(first, std::string_view{name}) == first.end())
        {
            append(name, value);
        }
    }
    return text;
}

/// Java's saveLandingDispersionSettings(): nothing without settings (see the header for what
/// is written of them).
void saveLandingDispersionSettings(const std::optional<LandingDispersionSettings>& settings,
                                   OrkWriter&                                      writer)
{
    if (!settings.has_value())
    {
        return;
    }

    // HOOK(monte-carlo): Java writes its MonteCarloSettings: the run count and the seed, then
    // per MonteCarloParameter in its order the uncertainty with Double.toString() of the spread.
    writer.line(std::format("<landingdispersion{}>",
                            attributesText(settings->getAttributes(), {"runs", "seed"}, writer)));
    writer.indent();
    for (const LandingDispersionSettings::Attributes& uncertainty : settings->getUncertainties())
    {
        writer.line(std::format(
            "<uncertainty{}/>",
            attributesText(uncertainty, {"parameter", "distribution", "spread"}, writer)));
    }
    writer.outdent();
    writer.line("</landingdispersion>");
}

// ------------------------------------------------------------------------ plot appearances

/// The plot appearances of @p simulation: <plotappearance> with a <series> per symbol.
void savePlotAppearances(const Simulation& simulation, OrkWriter& writer)
{
    const std::map<std::string, PlotAppearance> plotAppearances = simulation.getPlotAppearances();
    if (plotAppearances.empty())
    {
        return;
    }
    // Persist per-series appearance tweaks (color and/or line style). Java goes through a
    // TreeMap: the symbols in the order of String.compareTo(), not of their bytes.
    std::vector<const std::pair<const std::string, PlotAppearance>*> entries;
    entries.reserve(plotAppearances.size());
    for (const auto& entry : plotAppearances)
    {
        entries.push_back(&entry);
    }
    std::ranges::stable_sort(entries, [](const auto* a, const auto* b) {
        return Strings::javaCompareTo(a->first, b->first) < 0;
    });

    writer.line("<plotappearance>");
    writer.indent();
    for (const auto* entry : entries)
    {
        const std::string&    symbol     = entry->first;
        const PlotAppearance& appearance = entry->second;
        if (appearance.isEmpty() || symbol.empty())
        {
            continue;
        }
        std::string series = std::format(R"(<series symbol="{}")", writer.escape(symbol));
        if (const std::optional<LineStyle>& lineStyle = appearance.getLineStyle())
        {
            series += std::format(R"( linestyle="{}")",
                                  Strings::toOrkEnumName(lineStyleName(*lineStyle)));
        }
        if (const std::optional<Color>& color = appearance.getColor())
        {
            series += " " + color->toXmlAttributes();
        }
        series += "/>";
        writer.line(series);
    }
    writer.outdent();
    writer.line("</plotappearance>");
}

// ------------------------------------------------------------------------------ extensions

/// One <extension> per extension of @p simulation, with its configuration.
void saveExtensions(const Simulation& simulation, OrkWriter& writer)
{
    for (const std::shared_ptr<SimulationExtension>& extension :
         simulation.getSimulationExtensions())
    {
        if (extension == nullptr)
        {
            continue;  // not Java's: a NullPointerException there
        }
        const Config config = extension->getConfig();
        writer.line(
            std::format(R"(<extension extensionid="{}">)", writer.escape(extension->getId())));
        writer.indent();
        for (const std::string& key : config.keySet())
        {
            if (const std::optional<Config::Value> value = config.get(key))
            {
                writer.entry("entry", key, *value, false);
            }
        }
        writer.outdent();
        writer.line("</extension>");
    }
}

// ----------------------------------------------------------------------------- flight data

/// One summary value of the flight data: its attribute and its getter.
struct SummaryAttribute
{
    std::string_view name;
    double (FlightData::*get)() const noexcept;
};

/// The summary attributes of <flightdata>, in Java's order.
constexpr std::array<SummaryAttribute, 10> kSummaryAttributes{{
    {.name = "maxaltitude", .get = &FlightData::getMaxAltitude},
    {.name = "maxvelocity", .get = &FlightData::getMaxVelocity},
    {.name = "maxacceleration", .get = &FlightData::getMaxAcceleration},
    {.name = "maxmach", .get = &FlightData::getMaxMachNumber},
    {.name = "timetoapogee", .get = &FlightData::getTimeToApogee},
    {.name = "flighttime", .get = &FlightData::getFlightTime},
    {.name = "groundhitvelocity", .get = &FlightData::getGroundHitVelocity},
    {.name = "launchrodvelocity", .get = &FlightData::getLaunchRodVelocity},
    {.name = "deploymentvelocity", .get = &FlightData::getDeploymentVelocity},
    {.name = "optimumdelay", .get = &FlightData::getOptimumDelay},
}};

/// One stored warning: <warning type="..."> with its id, description, priority, sources, the
/// parameter of its type and its whole text.
void saveWarning(const Warning& warning, OrkWriter& writer)
{
    writer.line(std::format(R"(<warning type="{}">)", warning.typeName()));
    writer.indent();

    writer.element("id", warning.id().toString());
    writer.element("description", warning.messageDescription());
    writer.element("priority", exportLabel(warning.priority()));

    // Java writes the id of the component the rocket has, or of its REMOVED component; a
    // source here keeps the id that was read (MessageSource::removed()).
    for (const MessageSource& source : warning.sources())
    {
        writer.element("source", source.id.toString());
    }

    // Data for specific warning types
    if (const auto* largeAoa = dynamic_cast<const Warning::LargeAOA*>(&warning))
    {
        writer.element("parameter", largeAoa->aoa());
    }

    if (const auto* highSpeed = dynamic_cast<const Warning::RecoveryHighSpeedDeployment*>(&warning))
    {
        writer.element("parameter", highSpeed->speed());
    }

    // We write the whole string content for backwards compatibility with old versions
    writer.line(writer.escape(warning.toString()));

    writer.outdent();
    writer.line("</warning>");
}

/// The <event> line of @p event.
[[nodiscard]] std::string eventLine(const FlightEvent& event, OrkWriter& writer)
{
    std::string line =
        std::format(R"(<event time="{}" type="{}" id="{}")", storageText(event.getTime()),
                    orkName(event.getType()), event.getId().toString());

    if (const std::optional<Uuid>& source = event.getSourceId())
    {
        line += std::format(R"( source="{}")", writer.escape(source->toString()));
    }

    if (event.getType() == FlightEvent::Type::SIM_WARN)
    {
        // Every SIM_WARN event holds its warning (FlightEvent checks it when it is made).
        if (const std::shared_ptr<const Warning> warning = event.getWarning())
        {
            line += std::format(R"( warnid="{}")", writer.escape(warning->id().toString()));

            if (const auto* afterLanding =
                    dynamic_cast<const Warning::EventAfterLanding*>(warning.get()))
            {
                // Java's getEvent().getID(); a warning without an event has no id to write.
                if (const std::optional<Uuid>& eventId = afterLanding->eventId())
                {
                    line += std::format(R"( eventid="{}")", writer.escape(eventId->toString()));
                }
            }
        }
    }

    if (event.getType() == FlightEvent::Type::SIM_ABORT)
    {
        if (const SimulationAbort* abort = event.getAbort())
        {
            line +=
                std::format(R"( cause="{}")", Strings::toOrkEnumName(causeName(abort->cause())));
        }
    }

    line += "/>";
    return line;
}

/// The <flightdata> element of @p data, the flight of @p simulation.
void saveFlightData(Simulation& simulation, const FlightData& data, bool saveSimulationData,
                    OrkWriter& writer)
{
    std::string str = "<flightdata";
    for (const SummaryAttribute& attribute : kSummaryAttributes)
    {
        const double value = (data.*attribute.get)();
        if (!std::isnan(value))
        {
            str += std::format(R"( {}="{}")", attribute.name, storageText(value));
        }
    }
    str += '>';
    writer.line(str);
    writer.indent();

    for (const Warning& warning : data.getWarningSet())
    {
        saveWarning(warning, writer);
    }

    // Check whether to store data (Java asks for the status first)
    if (simulation.getStatus() == Simulation::Status::EXTERNAL ||  // Always store external data
        saveSimulationData)
    {
        for (std::size_t i = 0; i < data.getBranchCount(); i++)
        {
            SimulationSaver::saveFlightDataBranch(data.getBranch(i), writer);
        }
    }

    writer.outdent();
    writer.line("</flightdata>");
}

}  // namespace

void SimulationSaver::saveSimulation(Simulation& simulation, bool saveSimulationData,
                                     OrkWriter& writer, WarningSet& warnings)
{
    const Simulation::Status simStatus =
        saveSimulationData ? simulation.getStatus() : Simulation::Status::NOT_SIMULATED;

    writer.line(
        std::format(R"(<simulation status="{}">)", Strings::toOrkEnumName(name(simStatus))));
    writer.indent();

    writer.element("name", simulation.getName());
    // TODO: MEDIUM: Other simulators/calculators (Java)

    writer.line("<simulator>RK4Simulator</simulator>");
    writer.line("<calculator>BarrowmanCalculator</calculator>");

    writeConditions(simulation, writer, warnings);

    saveLandingDispersionSettings(simulation.getLandingDispersionSettings(), writer);

    savePlotAppearances(simulation, writer);

    saveExtensions(simulation, writer);

    // Write basic simulation data
    if (const std::shared_ptr<FlightData> data = simulation.getSimulatedData())
    {
        saveFlightData(simulation, *data, saveSimulationData, writer);
    }

    writer.outdent();
    writer.line("</simulation>");
}

void SimulationSaver::saveFlightDataBranch(const FlightDataBranch& branch, OrkWriter& writer)
{
    // Retrieve the types from the branch (a branch always has one)
    const std::vector<const FlightDataType*> types = branch.getTypes();

    // Retrieve the data from the branch
    std::vector<const std::vector<double>*> data;
    data.reserve(types.size());
    for (const FlightDataType* type : types)
    {
        const std::vector<double>* column = branch.getView(*type);
        QTROCKET_ASSERT(column != nullptr);  // a type getTypes() gives
        data.push_back(column);
    }

    // Build the <databranch> tag
    std::string sb = std::format(R"(<databranch name="{}" )", writer.escape(branch.getName()));
    if (!std::isnan(branch.getOptimumAltitude()))
    {
        sb += std::format(R"(optimumAltitude="{}" )",
                          Strings::javaDoubleToString(branch.getOptimumAltitude()));
    }
    if (!std::isnan(branch.getTimeToOptimumAltitude()))
    {
        sb += std::format(R"(timeToOptimumAltitude="{}" )",
                          Strings::javaDoubleToString(branch.getTimeToOptimumAltitude()));
    }
    sb += "types=\"";
    for (std::size_t i = 0; i < types.size(); i++)
    {
        if (i > 0)
        {
            sb += ',';
        }
        sb += writer.escape(types[i]->getSaveKey());
    }
    sb += "\">";
    writer.line(sb);
    writer.indent();

    // Write events
    for (const FlightEvent& event : branch.getEvents())
    {
        writer.line(eventLine(event, writer));
    }

    // Write the data (Java's writeDataPointString())
    const std::size_t length = branch.getLength();
    for (std::size_t row = 0; row < length; row++)
    {
        sb = "<datapoint>";
        for (std::size_t j = 0; j < data.size(); j++)
        {
            if (j > 0)
            {
                sb += ',';
            }
            sb += storageText((*data[j])[row]);
        }
        sb += "</datapoint>";
        writer.line(sb);
    }

    writer.outdent();
    writer.line("</databranch>");
}

std::string SimulationSaver::lookupRowsNotSaved(std::string_view element,
                                                std::string_view simulation, std::string_view file)
{
    return std::format(
        "The rows of the {} table of the simulation \"{}\" were not saved: the file \"{}\" could "
        "not be read.",
        element, simulation, file);
}

}  // namespace QtRocket
