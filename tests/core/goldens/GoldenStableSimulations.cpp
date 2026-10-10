#include "goldens/GoldenStableSimulations.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenGeometry.h"
#include "goldens/GoldenMismatches.h"
#include "goldens/GoldenSimulations.h"
#include "goldens/GoldenWarnings.h"

namespace QtRocket::Test
{

namespace
{

using nlohmann::json;

/// The comparison collector of the golden tests.
using Mismatches = GoldenMismatches;

}  // namespace

std::optional<double> eventTime(const json& events, std::string_view type, bool last)
{
    std::optional<double> time;
    for (const json& event : events)
    {
        if (event.at("type").get<std::string>() == type && (last || !time.has_value()))
        {
            time = goldenValue(event.at("time"));
        }
    }
    return time;
}

std::optional<std::size_t> eulerApogeeRow(const GoldenTable& table)
{
    const std::optional<std::size_t> aoa      = table.columnIndex("aoa");
    const std::optional<std::size_t> velocity = table.columnIndex("velocity_z");
    if (!aoa.has_value() || !velocity.has_value())
    {
        return std::nullopt;
    }
    std::size_t row = 0;
    while (row < table.rows.size() && std::isfinite(table.rows[row][*aoa]))
    {
        row++;
    }
    if (row == table.rows.size() || !(table.rows[row][*velocity] > 0))
    {
        return std::nullopt;
    }
    while (row < table.rows.size() && !(std::abs(table.rows[row][*velocity]) < kApogeeVelocity))
    {
        row++;
    }
    return row < table.rows.size() ? std::optional<std::size_t>{row} : std::nullopt;
}

std::string apogeeStepProblem(const ApogeeStep& step)
{
    if (std::isnan(step.step))
    {
        return "there is no row that steps from the apogee";
    }
    if (!(std::abs(step.left) <= kApogeeResidual * std::abs(step.before)))
    {
        return std::format(
            "the step to the apogee leaves a vertical velocity of {} m/s after {} "
            "m/s, which is no rounding error",
            step.left, step.before);
    }
    if ((step.left > 0) != step.isShort())
    {
        return std::format("a step of {} s follows a vertical velocity of {} m/s at the apogee",
                           step.step, step.left);
    }
    return {};
}

double firstLateHandling(const json& events, const std::vector<double>& times)
{
    for (const json& event : events)
    {
        const double time = goldenValue(event.at("time"));
        if (!times.empty() && std::ranges::none_of(times, [time](double row) {
                return std::abs(row - time) <= kOnTime;
            }))
        {
            return time;
        }
    }
    return std::numeric_limits<double>::infinity();
}

ApogeeStep apogeeStepOf(const FlightDataBranch& branch, std::size_t row)
{
    const std::vector<double>* velocity =
        branch.getView(FlightDataType::builtin(QtRocket::FlightDataTypeId::TYPE_VELOCITY_Z));
    const std::vector<double>* steps =
        branch.getView(FlightDataType::builtin(QtRocket::FlightDataTypeId::TYPE_TIME_STEP));
    if (velocity == nullptr || steps == nullptr || row == 0 || row >= velocity->size() ||
        row >= steps->size())
    {
        return {};
    }
    return {.left = (*velocity)[row], .before = (*velocity)[row - 1], .step = (*steps)[row]};
}

void compareStableNumber(Mismatches& m, StableComparison& c, const StableNumber& number,
                         bool compared)
{
    if (!compared)
    {
        c.excluded.numbers++;
        return;
    }
    c.compared.numbers++;
    if (c.measurements != nullptr && std::isfinite(number.expected) &&
        std::isfinite(number.actual) && number.unit > 0)
    {
        c.measurements->record(number.what, std::abs(number.actual - number.expected) / number.unit,
                               number.tolerance / number.unit, c.context);
    }
    m.within(number.field, number.expected, number.actual, 0.0, number.tolerance);
}

void compareStableCount(Mismatches& m, StableComparison& c, std::string_view field,
                        std::int64_t expected, std::int64_t actual, bool compared)
{
    if (!compared)
    {
        c.excluded.numbers++;
        return;
    }
    c.compared.numbers++;
    m.integer(field, expected, actual);
}

void compareStableWarning(Mismatches& m, StableComparison& c, const std::string& field,
                          const json& expected, const Warning* actual)
{
    if (!expected.is_object() || !expected.contains("parameter"))
    {
        return;
    }
    const std::optional<double> parameter =
        actual != nullptr ? parameterOf(*actual) : std::optional<double>{};
    if (!parameter.has_value())
    {
        c.excluded.numbers++;  // the structure reports a warning of another kind
        return;
    }
    const double goldenNumber = goldenValue(expected.at("parameter"));
    const double unit         = referenceOf(0, goldenNumber, *parameter);
    compareStableNumber(m, c,
                        {.field     = field + ".parameter",
                         .what      = "warning parameter",
                         .expected  = goldenNumber,
                         .actual    = *parameter,
                         .tolerance = kStableSummaryRelative * unit,
                         .unit      = unit},
                        true);
    m.text(field + ".description", expected.at("description").get<std::string>(),
           actual->messageDescription());
    m.text(field + ".text", expected.at("text").get<std::string>(), actual->toString());
}

void compareStableWarnings(Mismatches& m, StableComparison& c, const json& expected,
                           const FlightData& data)
{
    std::vector<const Warning*> warnings;
    for (const Warning& warning : data.getWarningSet())
    {
        warnings.push_back(&warning);
    }
    for (std::size_t i = 0; i < expected.size(); i++)
    {
        compareStableWarning(m, c, std::format("warnings[{}]", i), expected.at(i),
                             i < warnings.size() ? warnings[i] : nullptr);
    }
}

int eventNumbers(const json& events)
{
    int numbers = static_cast<int>(events.size());
    for (const json& event : events)
    {
        const json& data = event.at("data");
        numbers += data.is_object() && data.contains("parameter") ? 1 : 0;
    }
    return numbers;
}

GoldenTable timeSeriesOf(const FlightDataBranch& branch)
{
    // (The rows are made with the table: GCC takes assign() on the rows of a new table for a
    // potential null pointer dereference.)
    GoldenTable table{.columns = {}, .rows = std::vector<std::vector<double>>(branch.getLength())};
    for (const FlightDataType* type : csvTypes(branch))
    {
        table.columns.push_back(columnKey(*type));
        // A type of the branch has a column: getView() gives it.
        const std::vector<double>* values = branch.getView(*type);
        for (std::size_t row = 0; values != nullptr && row < values->size(); row++)
        {
            table.rows[row].push_back((*values)[row]);
        }
    }
    return table;
}

json numberJson(double value)
{
    if (std::isnan(value))
    {
        return "NaN";
    }
    if (std::isinf(value))
    {
        return value > 0 ? "Infinity" : "-Infinity";
    }
    return value;
}

json goldenBranchOf(const SimulationRun& run, std::size_t index, GoldenTable& table)
{
    const FlightDataBranch& branch = run.data->getBranch(index);
    json                    form   = json::object();
    form["rows"]                   = branch.getLength();
    form["optimumAltitude"]        = numberJson(branch.getOptimumAltitude());
    form["timeToOptimumAltitude"]  = numberJson(branch.getTimeToOptimumAltitude());
    form["optimumDelay"]           = numberJson(branch.getOptimumDelay());
    form["separationTime"]         = numberJson(branch.getSeparationTime());
    form["columns"]                = json::array();
    table                          = timeSeriesOf(branch);
    for (const FlightDataType* type : csvTypes(branch))
    {
        form["columns"].push_back({{"min", numberJson(branch.getMinimum(*type))},
                                   {"max", numberJson(branch.getMaximum(*type))}});
    }
    form["events"] = json::array();
    for (const FlightEvent& event : branch.getEvents())
    {
        const Warning* warning = run.data->findWarning(event);
        form["events"].push_back(
            {{"time", numberJson(event.getTime())},
             {"type", std::string{name(event.getType())}},
             {"source",
              event.getSource() == nullptr ? json(nullptr) : json(pathOrNull(event.getSource()))},
             {"data", warning != nullptr ? goldenFormOf(*warning) : json(nullptr)}});
    }
    return form;
}

GoldenFiles goldenFilesOf(const SimulationRun& run)
{
    GoldenFiles files;
    files.document["result"]   = {{"jitterReplacements", run.jitterReplacements}};
    files.document["summary"]  = json::object();
    files.document["warnings"] = json::array();
    files.document["branches"] = json::array();
    if (run.data == nullptr)
    {
        return files;
    }
    for (const StableSummaryValue& value : kStableSummaryValues)
    {
        files.document["summary"][std::string{value.key}] = numberJson((*run.data.*value.get)());
    }
    for (const Warning& warning : run.data->getWarningSet())
    {
        files.document["warnings"].push_back(goldenFormOf(warning));
    }
    files.tables.resize(run.data->getBranchCount());
    for (std::size_t i = 0; i < run.data->getBranchCount(); i++)
    {
        files.document["branches"].push_back(goldenBranchOf(run, i, files.tables[i]));
    }
    return files;
}

}  // namespace QtRocket::Test
