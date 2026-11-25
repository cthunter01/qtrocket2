#include "simulation/SimulationTablePins.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/util/Error.h"
#include "exit/OpenDesigns.h"
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

[[nodiscard]] SimulationTablePinOrder orderOf(const Json& value)
{
    return SimulationTablePinOrder{.ascending  = value.at("asc").get<std::vector<int>>(),
                                   .descending = value.at("desc").get<std::vector<int>>()};
}

[[nodiscard]] std::map<std::string, SimulationTablePinOrder> ordersOf(const Json& value)
{
    std::map<std::string, SimulationTablePinOrder> orders;
    for (const auto& [id, order] : value.items())
    {
        orders.emplace(id, orderOf(order));
    }
    return orders;
}

[[nodiscard]] SimulationTablePinRow rowOf(const Json& value)
{
    SimulationTablePinRow row;
    row.row            = value.at("row").get<int>();
    row.status         = value.at("status").get<std::string>();
    row.statusIcon     = value.at("statusIcon").get<std::string>();
    row.statusSortText = value.at("statusSortText").get<std::string>();
    row.description    = value.at("description").get<std::string>();
    row.statusToolTip  = value.at("statusToolTip").get<std::string>();
    row.fresh          = value.value("fresh", false);
    if (const auto warnings = value.find("warnings"); warnings != value.end())
    {
        SimulationTablePinWarnings pin;
        if (!warnings->at("counts").is_null())
        {
            pin.counts = warnings->at("counts").get<std::array<std::size_t, 3>>();
        }
        pin.box     = warnings->at("box").get<std::string>();
        pin.toolTip = warnings->at("toolTip").get<std::string>();
        for (const Json& message : warnings->at("messages"))
        {
            pin.messages.emplace_back(message.at(0).get<std::string>(),
                                      message.at(1).get<std::string>());
        }
        row.warnings = std::move(pin);
    }
    if (const auto cells = value.find("cells"); cells != value.end())
    {
        for (const Json& cell : *cells)
        {
            row.cells.push_back(optionalText(cell));
        }
    }
    return row;
}

[[nodiscard]] std::vector<SimulationTablePinRow> rowsOf(const Json& value)
{
    std::vector<SimulationTablePinRow> rows;
    for (const Json& row : value)
    {
        rows.push_back(rowOf(row));
    }
    return rows;
}

[[nodiscard]] std::vector<SimulationTablePinStep> stepsOf(const Json& value)
{
    std::vector<SimulationTablePinStep> steps;
    for (const Json& step : value)
    {
        steps.push_back(SimulationTablePinStep{
            .step     = step.at("step").get<std::string>(),
            .statuses = step.at("statuses").get<std::vector<std::string>>(),
            .undo     = optionalText(step.at("undo")),
            .redo     = optionalText(step.at("redo")),
            .saved    = step.at("saved").get<bool>(),
        });
    }
    return steps;
}

[[nodiscard]] SimulationTablePinDesign designOf(const Json& value)
{
    SimulationTablePinDesign design;
    design.name               = value.at("name").get<std::string>();
    design.kind               = value.at("kind").get<std::string>();
    design.file               = value.at("file").get<std::string>();
    design.hasValidConfig     = value.at("hasValidConfig").get<bool>();
    design.nextSimulationName = value.at("nextSimulationName").get<std::string>();
    design.simulationCount    = value.at("simulationCount").get<std::size_t>();
    for (const Json& width : value.at("widths"))
    {
        design.widths.push_back(SimulationTablePinWidth{.id        = width.at(0).get<std::string>(),
                                                        .preferred = width.at(1).get<int>(),
                                                        .minimum   = width.at(2).get<int>(),
                                                        .maximum   = width.at(3).get<int>()});
    }
    design.actions = value.at("actions").get<std::map<std::string, std::map<std::string, bool>>>();
    for (const auto& [key, target] :
         {std::pair{"csv", &design.csv}, std::pair{"copy", &design.copy},
          std::pair{"csvExponential", &design.csvExponential}})
    {
        if (const auto found = value.find(key); found != value.end())
        {
            *target = found->get<std::string>();
        }
    }
    design.simulations = rowsOf(value.at("simulations"));
    design.orders      = ordersOf(value.at("orders"));
    return design;
}

[[nodiscard]] SimulationTablePinScenario scenarioOf(const Json& value)
{
    SimulationTablePinScenario scenario;
    scenario.name = value.at("name").get<std::string>();
    scenario.file = value.at("file").get<std::string>();
    for (const Json& initial : value.at("initial"))
    {
        scenario.initial.push_back(SimulationTablePinInitial{
            .name            = initial.at("name").get<std::string>(),
            .configurationId = initial.at("configurationId").get<std::string>(),
            .configuration   = initial.at("configuration").get<std::string>(),
        });
    }
    scenario.steps       = stepsOf(value.at("steps"));
    scenario.simulations = rowsOf(value.at("simulations"));
    scenario.orders      = ordersOf(value.at("orders"));
    return scenario;
}

[[nodiscard]] SimulationTablePinPairing pairingOf(const Json& value)
{
    return SimulationTablePinPairing{
        .name           = value.at("name").get<std::string>(),
        .file           = value.at("file").get<std::string>(),
        .steps          = stepsOf(value.at("steps")),
        .copy           = value.at("copy").get<std::string>(),
        .openRocketCopy = value.at("openRocketCopy").get<std::string>(),
    };
}

[[nodiscard]] Result<SimulationTablePins> parsePins(const Json& document)
{
    try
    {
        if (document.at("schema").get<std::string>() != "simulation-table" ||
            document.at("schemaVersion").get<int>() != kSimulationTablePinsSchemaVersion)
        {
            return fail(ErrorCode::PARSE, "simulation-table.json: not the expected schema");
        }
        SimulationTablePins pins;
        pins.commit = document.at("openrocket").at("commit").get<std::string>();
        pins.colors = document.at("colors").get<std::map<std::string, std::string>>();
        for (const Json& column : document.at("columns"))
        {
            pins.columns.push_back(SimulationTablePinColumn{
                .id      = column.at("id").get<std::string>(),
                .header  = column.at("header").get<std::string>(),
                .toolTip = column.at("toolTip").get<std::string>(),
                .unit    = column.at("unit").get<std::string>(),
            });
        }
        for (const Json& design : document.at("designs"))
        {
            pins.designs.push_back(designOf(design));
        }
        for (const Json& scenario : document.at("scenarios"))
        {
            pins.scenarios.push_back(scenarioOf(scenario));
        }
        for (const Json& pairing : document.at("pairings"))
        {
            pins.pairings.push_back(pairingOf(pairing));
        }
        return pins;
    }
    catch (const Json::exception& error)
    {
        return fail(ErrorCode::PARSE, std::format("simulation-table.json: {}", error.what()));
    }
}

[[nodiscard]] Result<SimulationTablePins> loadPins()
{
    const Result<Json> document = QtRocket::Test::loadGoldenJson(
        std::filesystem::path("gui") / "simulation-table" / "simulation-table.json");
    if (!document.has_value())
    {
        return std::unexpected(document.error());
    }
    return parsePins(*document);
}

/// The element of @p items named @p name, or null.
template <class T>
[[nodiscard]] const T* findNamed(const std::vector<T>& items, std::string_view name)
{
    const auto found =
        std::ranges::find_if(items, [name](const T& item) { return item.name == name; });
    return found == items.end() ? nullptr : &*found;
}

}  // namespace

const SimulationTablePinOrder& SimulationTablePinDesign::order(std::string_view id) const
{
    static const SimulationTablePinOrder kNone;
    const auto                           found = orders.find(std::string(id));
    if (found == orders.end())
    {
        ADD_FAILURE() << "the pins of " << name << " have no order of the column " << id;
        return kNone;
    }
    return found->second;
}

const SimulationTablePinDesign* SimulationTablePins::findDesign(std::string_view name) const
{
    return findNamed(designs, name);
}

const SimulationTablePinScenario* SimulationTablePins::findScenario(std::string_view name) const
{
    return findNamed(scenarios, name);
}

const SimulationTablePinPairing* SimulationTablePins::findPairing(std::string_view name) const
{
    return findNamed(pairings, name);
}

const Result<SimulationTablePins>& simulationTablePins()
{
    static const Result<SimulationTablePins> kPins = loadPins();
    return kPins;
}

const SimulationTablePins* simulationTablePinsOrFail()
{
    const Result<SimulationTablePins>& loaded = simulationTablePins();
    if (!loaded.has_value())
    {
        ADD_FAILURE() << loaded.error().toString();
        return nullptr;
    }
    return &*loaded;
}

std::vector<std::string> simulationTablePinnedDesigns()
{
    std::vector<std::string> names;
    for (const OpenDesign& design : openDesigns())
    {
        names.push_back(design.name);
    }
    names.emplace_back(kNewDesignPinName);
    return names;
}

std::vector<std::string> simulationTablePinnedExampleDesigns()
{
    std::vector<std::string> names;
    for (const OpenDesign& design : openDesigns())
    {
        if (design.source == OpenDesignSource::EXAMPLE)
        {
            names.push_back(design.name);
        }
    }
    names.emplace_back(kNewDesignPinName);
    return names;
}

std::string simulationTablePinTestName(const ::testing::TestParamInfo<std::string>& param)
{
    return param.param;
}

}  // namespace QtRocket::Gui::Test
