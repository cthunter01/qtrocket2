#include "dialogs/simulation/SimulationDialogPins.h"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/util/Error.h"
#include "goldens/GoldenData.h"

namespace QtRocket::Gui::Test
{

namespace
{

using Json = nlohmann::json;

/// A text that may be null.
[[nodiscard]] std::optional<std::string> optionalText(const Json& value)
{
    if (value.is_null())
    {
        return std::nullopt;
    }
    return value.get<std::string>();
}

[[nodiscard]] SimulationDialogPinUndo undoOf(const Json& value)
{
    SimulationDialogPinUndo undo;
    undo.position = value.at("position").get<std::size_t>();
    for (const Json& description : value.at("descriptions"))
    {
        undo.descriptions.push_back(optionalText(description));
    }
    undo.next   = optionalText(value.at("next"));
    undo.stored = optionalText(value.at("stored"));
    undo.clean  = value.at("clean").get<bool>();
    undo.saved  = value.at("saved").get<bool>();
    return undo;
}

/// The walk of @p value: the lines its indices name in @p lines.
[[nodiscard]] std::vector<std::string> walkOf(const Json&                     value,
                                              const std::vector<std::string>& lines)
{
    std::vector<std::string> walk;
    for (const Json& index : value)
    {
        walk.push_back(lines.at(index.get<std::size_t>()));
    }
    return walk;
}

[[nodiscard]] SimulationDialogPinSimulation simulationOf(const Json&                     value,
                                                         const std::vector<std::string>& lines)
{
    SimulationDialogPinSimulation simulation;
    simulation.index          = value.at("index").get<std::size_t>();
    simulation.name           = value.at("name").get<std::string>();
    simulation.title          = value.at("title").get<std::string>();
    simulation.modified       = value.at("modified").get<bool>();
    simulation.modifiedTitle  = value.at("modifiedTitle").get<std::string>();
    simulation.walk           = walkOf(value.at("walk"), lines);
    simulation.tabs           = value.at("tabs").get<std::vector<std::string>>();
    simulation.extensions     = value.at("extensions").get<std::vector<std::string>>();
    simulation.wind           = value.at("wind").get<std::vector<std::string>>();
    simulation.changes        = value.at("changes").get<std::vector<int>>();
    simulation.undoBefore     = undoOf(value.at("undoBefore"));
    simulation.undoAfter      = undoOf(value.at("undoAfter"));
    simulation.statusesBefore = value.at("statusesBefore").get<std::vector<std::string>>();
    simulation.statusesAfter  = value.at("statusesAfter").get<std::vector<std::string>>();
    return simulation;
}

[[nodiscard]] SimulationDialogPinDesign designOf(const Json&                     value,
                                                 const std::vector<std::string>& lines)
{
    SimulationDialogPinDesign design;
    design.name = value.at("name").get<std::string>();
    design.kind = value.at("kind").get<std::string>();
    design.file = value.at("file").get<std::string>();
    for (const Json& simulation : value.at("simulations"))
    {
        design.simulations.push_back(simulationOf(simulation, lines));
    }
    return design;
}

[[nodiscard]] Result<SimulationDialogPins> parsePins(const Json& document)
{
    try
    {
        if (document.at("schema").get<std::string>() != "simulation-dialog" ||
            document.at("schemaVersion").get<int>() != kSimulationDialogPinsSchemaVersion)
        {
            return fail(ErrorCode::PARSE, "simulation-dialog.json: not the expected schema");
        }
        SimulationDialogPins pins;
        pins.commit      = document.at("openrocket").at("commit").get<std::string>();
        pins.colors      = document.at("colors").get<std::map<std::string, std::string>>();
        const auto lines = document.at("lines").get<std::vector<std::string>>();
        for (const Json& design : document.at("designs"))
        {
            pins.designs.push_back(designOf(design, lines));
        }
        return pins;
    }
    catch (const Json::exception& error)
    {
        return fail(ErrorCode::PARSE, std::format("simulation-dialog.json: {}", error.what()));
    }
    catch (const std::out_of_range& error)
    {
        return fail(ErrorCode::PARSE, std::format("simulation-dialog.json: {}", error.what()));
    }
}

[[nodiscard]] Result<SimulationDialogPins> loadPins()
{
    const Result<Json> document = QtRocket::Test::loadGoldenJson(
        std::filesystem::path("gui") / "simulation-dialog" / "simulation-dialog.json");
    if (!document.has_value())
    {
        return std::unexpected(document.error());
    }
    return parsePins(*document);
}

}  // namespace

const SimulationDialogPinDesign* SimulationDialogPins::findDesign(std::string_view name) const
{
    const auto found = std::ranges::find(designs, name, &SimulationDialogPinDesign::name);
    return found == designs.end() ? nullptr : &*found;
}

const Result<SimulationDialogPins>& simulationDialogPins()
{
    static const Result<SimulationDialogPins> kPins = loadPins();
    return kPins;
}

const SimulationDialogPins* simulationDialogPinsOrFail()
{
    const Result<SimulationDialogPins>& loaded = simulationDialogPins();
    if (!loaded.has_value())
    {
        ADD_FAILURE() << loaded.error().toString();
        return nullptr;
    }
    return &*loaded;
}

}  // namespace QtRocket::Gui::Test
