#include "RocketInfoFlightPins.h"

#include <array>
#include <cctype>
#include <cstddef>
#include <exception>
#include <fstream>
#include <ios>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"
#include "goldens/GoldenData.h"

namespace QtRocket::Gui::Test
{

namespace
{

using Json = nlohmann::json;

/// What makes the document unreadable.
class FlightPinsFormatError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

/// The member @p key of @p object.
[[nodiscard]] const Json& member(const Json& object, std::string_view key)
{
    if (!object.is_object() || !object.contains(key))
    {
        throw FlightPinsFormatError("no member " + std::string(key));
    }
    return object.at(key);
}

[[nodiscard]] std::string text(const Json& object, std::string_view key)
{
    const Json& value = member(object, key);
    if (!value.is_string())
    {
        throw FlightPinsFormatError(std::string(key) + " is not a string");
    }
    return value.get<std::string>();
}

[[nodiscard]] bool flag(const Json& object, std::string_view key)
{
    const Json& value = member(object, key);
    if (!value.is_boolean())
    {
        throw FlightPinsFormatError(std::string(key) + " is not a boolean");
    }
    return value.get<bool>();
}

[[nodiscard]] std::size_t indexOf(const Json& value, std::string_view what)
{
    if (!value.is_number_unsigned())
    {
        throw FlightPinsFormatError(std::string(what) + " is not an index");
    }
    return value.get<std::size_t>();
}

/// The index at @p key, or nullopt for null.
[[nodiscard]] std::optional<std::size_t> optionalIndex(const Json& object, std::string_view key)
{
    const Json& value = member(object, key);
    if (value.is_null())
    {
        return std::nullopt;
    }
    return indexOf(value, key);
}

[[nodiscard]] std::vector<std::size_t> indices(const Json& value, std::string_view what)
{
    if (!value.is_array())
    {
        throw FlightPinsFormatError(std::string(what) + " is not a list");
    }
    std::vector<std::size_t> result;
    for (const Json& element : value)
    {
        result.push_back(indexOf(element, what));
    }
    return result;
}

[[nodiscard]] double numberAt(const Json& object, std::string_view key)
{
    const std::optional<double> number = QtRocket::Test::goldenNumber(member(object, key));
    if (!number.has_value())
    {
        throw FlightPinsFormatError(std::string(key) + " is not a number");
    }
    return *number;
}

/// "<source>_<design>" of a scenario's name, with every other character '_'.
[[nodiscard]] std::string testNameOf(std::string_view name)
{
    std::string result;
    for (const char c : name)
    {
        result += std::isalnum(static_cast<unsigned char>(c)) != 0 ? c : '_';
    }
    return result;
}

[[nodiscard]] FlightPinSimulation simulationOf(const Json& value)
{
    FlightPinSimulation simulation;
    simulation.name                   = text(value, "name");
    simulation.status                 = text(value, "status");
    simulation.hasData                = flag(value, "hasData");
    simulation.selectedConfiguration  = flag(value, "selectedConfiguration");
    simulation.configurationHasMotors = flag(value, "configurationHasMotors");
    if (const Json& maxima = member(value, "maxima"); !maxima.is_null())
    {
        simulation.maxima =
            std::array{numberAt(maxima, "altitude"), numberAt(maxima, "velocity"),
                       numberAt(maxima, "machNumber"), numberAt(maxima, "acceleration")};
    }
    return simulation;
}

[[nodiscard]] FlightPinText textOf(const Json& value)
{
    FlightPinText pin;
    pin.text         = text(value, "text");
    pin.font         = text(value, "font");
    pin.colour       = text(value, "colour");
    const Json& rgba = member(value, "rgba");
    if (!rgba.is_array() || rgba.size() != pin.rgba.size())
    {
        throw FlightPinsFormatError("rgba is not four numbers");
    }
    for (std::size_t i = 0; i < pin.rgba.size(); ++i)
    {
        pin.rgba.at(i) = rgba.at(i).get<int>();
    }
    pin.rowFromBottom    = member(value, "rowFromBottom").get<int>();
    const std::string at = text(value, "column");
    if (at != "LEFT" && at != "VALUE")
    {
        throw FlightPinsFormatError("a column is " + at);
    }
    pin.leftColumn = at == "LEFT";
    return pin;
}

[[nodiscard]] FlightPinShown shownOf(const std::string& shown)
{
    if (shown == "NONE")
    {
        return FlightPinShown::NONE;
    }
    if (shown == "NAN")
    {
        return FlightPinShown::NAN_DATA;
    }
    if (shown == "DATA")
    {
        return FlightPinShown::DATA;
    }
    throw FlightPinsFormatError("an overlay shows " + shown);
}

/// The set-up, the configuration and the simulations of @p value into @p pin.
void readDesign(const Json& value, RocketInfoFlightPin& pin)
{
    const Json& setup         = member(value, "setup");
    pin.estimated             = flag(setup, "estimated");
    pin.imperial              = text(setup, "units") == "imperial";
    pin.selectConfigurationOf = optionalIndex(setup, "selectConfigurationOf");
    const Json& configuration = member(value, "configuration");
    pin.hasMotors             = flag(configuration, "hasMotors");
    pin.configurationName     = text(configuration, "name");
    for (const Json& simulation : member(value, "simulations"))
    {
        pin.simulations.push_back(simulationOf(simulation));
    }
    const Json& runs = member(value, "runs");
    pin.runsCurrent  = indices(member(runs, "current"), "runs.current");
    pin.runsAll      = indices(member(runs, "all"), "runs.all");
}

/// The overlay, the status and the texts of @p value into @p pin.
void readOverlay(const Json& value, RocketInfoFlightPin& pin)
{
    const Json& overlay = member(value, "overlay");
    pin.shown           = shownOf(text(overlay, "shown"));
    pin.calculating     = flag(overlay, "calculating");
    pin.shownSimulation = optionalIndex(overlay, "simulation");
    if (const Json& status = member(value, "status"); !status.is_null())
    {
        pin.status     = text(status, "status");
        pin.statusText = text(status, "text");
    }
    for (const Json& entry : member(value, "texts"))
    {
        pin.texts.push_back(textOf(entry));
    }
}

[[nodiscard]] RocketInfoFlightPin scenarioOf(const Json& value)
{
    RocketInfoFlightPin pin;
    pin.name     = text(value, "name");
    pin.testName = testNameOf(pin.name);
    pin.kind     = text(value, "kind");
    pin.file     = text(value, "file");
    readDesign(value, pin);
    readOverlay(value, pin);
    return pin;
}

[[nodiscard]] RocketInfoFlightPins readPins()
{
    RocketInfoFlightPins pins;
    const auto           path =
        QtRocket::Test::testDataDir() / "goldens" / "gui" / "rocket-info" / "flight-blocks.json";
    try
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            throw FlightPinsFormatError("cannot open " + pathToUtf8(path));
        }
        const Json document   = Json::parse(in);
        pins.schema           = member(document, "schema").get<int>();
        pins.openRocketCommit = text(document, "openrocket");
        for (const Json& scenario : member(document, "scenarios"))
        {
            pins.scenarios.push_back(scenarioOf(scenario));
        }
    }
    catch (const std::exception& error)
    {
        pins.problem = error.what();
        pins.scenarios.clear();
    }
    return pins;
}

}  // namespace

const RocketInfoFlightPins& rocketInfoFlightPins()
{
    static const RocketInfoFlightPins kPins = readPins();
    return kPins;
}

}  // namespace QtRocket::Gui::Test
