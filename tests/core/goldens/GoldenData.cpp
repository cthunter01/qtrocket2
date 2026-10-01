#include "goldens/GoldenData.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/file/GzipStream.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"
#include "TestPaths.h"

namespace QtRocket::Test
{

namespace
{

std::filesystem::path resolve(const std::filesystem::path& path)
{
    return path.is_absolute() ? path : goldensDir() / path;
}

/// The string member @p key of @p object, or a PARSE failure naming @p context.
Result<std::string> requireString(const nlohmann::json& object, std::string_view key,
                                  std::string_view context)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string())
    {
        return fail(ErrorCode::PARSE, std::format(R"({}: missing string "{}")", context, key));
    }
    return it->get<std::string>();
}

/// The array member @p key of @p object, or a PARSE failure naming @p context.
Result<const nlohmann::json*> requireArray(const nlohmann::json& object, std::string_view key,
                                           std::string_view context)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_array())
    {
        return fail(ErrorCode::PARSE, std::format(R"({}: missing array "{}")", context, key));
    }
    return &*it;
}

Result<GoldenSimulation> parseSimulation(const nlohmann::json& json, std::string_view context)
{
    if (!json.is_object())
    {
        return fail(ErrorCode::PARSE, std::format("{}: a simulation is not an object", context));
    }
    auto name = requireString(json, "name", context);
    if (!name)
    {
        return std::unexpected(name.error());
    }
    auto file = requireString(json, "json", context);
    if (!file)
    {
        return std::unexpected(file.error());
    }
    const auto branches = requireArray(json, "branches", context);
    if (!branches)
    {
        return std::unexpected(branches.error());
    }
    GoldenSimulation simulation{.name = std::move(*name), .json = std::move(*file), .branches = {}};
    for (const auto& branch : **branches)
    {
        if (!branch.is_string())
        {
            return fail(ErrorCode::PARSE,
                        std::format("{}: a branch file name is not a string", context));
        }
        simulation.branches.push_back(branch.get<std::string>());
    }
    return simulation;
}

/// Reads the string members of an input; the first missing one is the failure.
Result<void> readInputStrings(const nlohmann::json& json, std::string_view context,
                              GoldenInput& input)
{
    for (auto [key, target] :
         {std::pair{"kind", &input.kind}, std::pair{"source", &input.source},
          std::pair{"geometry", &input.geometry}, std::pair{"mass", &input.mass},
          std::pair{"aero", &input.aero}, std::pair{"resave", &input.resave}})
    {
        auto value = requireString(json, key, context);
        if (!value)
        {
            return std::unexpected(value.error());
        }
        *target = std::move(*value);
    }
    return {};
}

Result<GoldenInput> parseInput(const nlohmann::json& json)
{
    if (!json.is_object())
    {
        return fail(ErrorCode::PARSE, "manifest.json: an input is not an object");
    }
    GoldenInput input;
    auto        name = requireString(json, "name", "manifest.json input");
    if (!name)
    {
        return std::unexpected(name.error());
    }
    input.name                = std::move(*name);
    const std::string context = std::format(R"(manifest.json input "{}")", input.name);
    if (auto strings = readInputStrings(json, context, input); !strings)
    {
        return std::unexpected(strings.error());
    }
    const auto simulations = requireArray(json, "simulations", context);
    if (!simulations)
    {
        return std::unexpected(simulations.error());
    }
    for (const auto& simulationJson : **simulations)
    {
        auto simulation = parseSimulation(simulationJson, context);
        if (!simulation)
        {
            return std::unexpected(simulation.error());
        }
        input.simulations.push_back(std::move(*simulation));
    }
    return input;
}

/// Parses one data line of a branch CSV into @p row (cleared first). The fields are split
/// without allocating: the goldens hold millions of values.
Result<void> parseRow(std::string_view line, std::size_t lineNumber,
                      const std::vector<std::string>& columns, std::vector<double>& row)
{
    row.clear();
    std::size_t fieldStart = 0;
    while (true)
    {
        const std::size_t      comma = line.find(',', fieldStart);
        const std::string_view field =
            line.substr(fieldStart, comma == std::string_view::npos ? std::string_view::npos
                                                                    : comma - fieldStart);
        if (row.size() == columns.size())
        {
            return fail(ErrorCode::PARSE, std::format("line {} has more values than the {} columns",
                                                      lineNumber, columns.size()));
        }
        const auto value = Strings::javaParseDouble(field);
        if (!value)
        {
            return fail(ErrorCode::PARSE,
                        std::format(R"(line {}, column "{}": "{}" is not a number)", lineNumber,
                                    columns[row.size()], field));
        }
        row.push_back(*value);
        if (comma == std::string_view::npos)
        {
            break;
        }
        fieldStart = comma + 1;
    }
    if (row.size() != columns.size())
    {
        return fail(ErrorCode::PARSE, std::format("line {} has {} values for {} columns",
                                                  lineNumber, row.size(), columns.size()));
    }
    return {};
}

}  // namespace

std::filesystem::path goldensDir()
{
    return testDataDir() / "goldens";
}

Result<nlohmann::json> parseGoldenJson(std::string_view text)
{
    nlohmann::json json = nlohmann::json::parse(text.begin(), text.end(), nullptr,
                                                /*allow_exceptions=*/false);
    if (json.is_discarded())
    {
        return fail(ErrorCode::PARSE, "not valid JSON");
    }
    return json;
}

Result<nlohmann::json> loadGoldenJson(const std::filesystem::path& path)
{
    const std::filesystem::path file = resolve(path);
    const auto                  text = readTextFile(file);
    if (!text)
    {
        return std::unexpected(text.error());
    }
    auto json = parseGoldenJson(*text);
    if (!json)
    {
        return fail(ErrorCode::PARSE, std::format("{}: not valid JSON", file.generic_string()));
    }
    return json;
}

std::optional<double> goldenNumber(const nlohmann::json& value)
{
    if (value.is_number())
    {
        return value.get<double>();
    }
    if (value.is_string())
    {
        const auto& text = value.get_ref<const std::string&>();
        if (text == "NaN")
        {
            return std::numeric_limits<double>::quiet_NaN();
        }
        if (text == "Infinity")
        {
            return std::numeric_limits<double>::infinity();
        }
        if (text == "-Infinity")
        {
            return -std::numeric_limits<double>::infinity();
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> GoldenTable::columnIndex(std::string_view key) const
{
    for (std::size_t i = 0; i < columns.size(); ++i)
    {
        if (columns[i] == key)
        {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<std::vector<double>> GoldenTable::column(std::string_view key) const
{
    const auto index = columnIndex(key);
    if (!index)
    {
        return std::nullopt;
    }
    std::vector<double> values;
    values.reserve(rows.size());
    for (const auto& row : rows)
    {
        values.push_back(row[*index]);
    }
    return values;
}

Result<GoldenTable> parseGoldenCsv(std::string_view text)
{
    if (text.empty())
    {
        return fail(ErrorCode::PARSE, "empty CSV");
    }
    GoldenTable table;
    std::size_t lineNumber = 0;
    std::size_t position   = 0;
    while (position < text.size())
    {
        const std::size_t end = text.find('\n', position);
        ++lineNumber;
        if (end == std::string_view::npos)
        {
            return fail(ErrorCode::PARSE,
                        std::format("line {} does not end with a newline", lineNumber));
        }
        const std::string_view line = text.substr(position, end - position);
        position                    = end + 1;
        if (lineNumber == 1)
        {
            table.columns = Strings::split(line, ',');
            continue;
        }
        std::vector<double> row;
        row.reserve(table.columns.size());
        if (auto parsed = parseRow(line, lineNumber, table.columns, row); !parsed)
        {
            return std::unexpected(parsed.error());
        }
        table.rows.push_back(std::move(row));
    }
    return table;
}

Result<GoldenTable> loadGoldenCsv(const std::filesystem::path& path)
{
    const std::filesystem::path file       = resolve(path);
    const auto                  compressed = readFile(file);
    if (!compressed)
    {
        return std::unexpected(compressed.error());
    }
    const auto plain = gzipInflate(*compressed);
    if (!plain)
    {
        return fail(ErrorCode::PARSE,
                    std::format("{}: {}", file.generic_string(), plain.error().message));
    }
    auto table = parseGoldenCsv(bytesToString(*plain));
    if (!table)
    {
        return fail(ErrorCode::PARSE,
                    std::format("{}: {}", file.generic_string(), table.error().message));
    }
    return table;
}

const GoldenInput* GoldenManifest::find(std::string_view name) const
{
    for (const auto& input : inputs)
    {
        if (input.name == name)
        {
            return &input;
        }
    }
    return nullptr;
}

Result<GoldenManifest> parseGoldenManifest(const nlohmann::json& manifest)
{
    constexpr std::string_view kContext = "manifest.json";
    if (!manifest.is_object())
    {
        return fail(ErrorCode::PARSE, "manifest.json is not an object");
    }
    GoldenManifest result;
    const auto     version = manifest.find("schemaVersion");
    if (version == manifest.end() || !version->is_number_integer())
    {
        return fail(ErrorCode::PARSE, R"(manifest.json: missing integer "schemaVersion")");
    }
    result.schemaVersion = version->get<int>();

    const auto openrocket = manifest.find("openrocket");
    if (openrocket == manifest.end() || !openrocket->is_object())
    {
        return fail(ErrorCode::PARSE, R"(manifest.json: missing object "openrocket")");
    }
    auto commit = requireString(*openrocket, "commit", kContext);
    if (!commit)
    {
        return std::unexpected(commit.error());
    }
    auto openrocketVersion = requireString(*openrocket, "version", kContext);
    if (!openrocketVersion)
    {
        return std::unexpected(openrocketVersion.error());
    }
    const auto inputs = requireArray(manifest, "inputs", kContext);
    if (!inputs)
    {
        return std::unexpected(inputs.error());
    }
    result.openrocketCommit  = std::move(*commit);
    result.openrocketVersion = std::move(*openrocketVersion);
    for (const auto& inputJson : **inputs)
    {
        auto input = parseInput(inputJson);
        if (!input)
        {
            return std::unexpected(input.error());
        }
        result.inputs.push_back(std::move(*input));
    }
    return result;
}

Result<GoldenManifest> loadGoldenManifest()
{
    const auto json = loadGoldenJson("manifest.json");
    if (!json)
    {
        return std::unexpected(json.error());
    }
    return parseGoldenManifest(*json);
}

const nlohmann::json* aeroResults(const nlohmann::json& aero, std::size_t index)
{
    if (!aero.is_object())
    {
        return nullptr;
    }
    const auto configurations = aero.find("configurations");
    if (configurations == aero.end() || !configurations->is_array() ||
        index >= configurations->size())
    {
        return nullptr;
    }
    const nlohmann::json& configuration = (*configurations)[index];
    const auto            same          = configuration.find("sameResultsAs");
    if (same == configuration.end())
    {
        return nullptr;
    }
    if (same->is_null())
    {
        return &configuration;
    }
    if (!same->is_number_unsigned())
    {
        return nullptr;
    }
    const auto target = same->get<std::size_t>();
    if (target >= index)
    {
        return nullptr;
    }
    const nlohmann::json& referenced = (*configurations)[target];
    const auto            again      = referenced.find("sameResultsAs");
    if (again == referenced.end() || !again->is_null())
    {
        return nullptr;
    }
    return &referenced;
}

// ================================================================================ GoldenCheck

GoldenCheck::GoldenCheck(const nlohmann::json& golden, double relativeTolerance) noexcept
  : m_golden(&golden), m_relativeTolerance(relativeTolerance)
{
}

const nlohmann::json* GoldenCheck::find(std::string_view pointer) const
{
    const nlohmann::json::json_pointer path{std::string{pointer}};
    if (!m_golden->contains(path))
    {
        return nullptr;
    }
    return &m_golden->at(path);
}

bool GoldenCheck::matches(double actual, const nlohmann::json& expected) const
{
    const std::optional<double> value = goldenNumber(expected);
    if (!value)
    {
        return false;
    }
    if (std::isnan(*value) || std::isnan(actual))
    {
        return std::isnan(*value) && std::isnan(actual);
    }
    if (*value == actual)
    {
        return true;  // equal infinities included
    }
    const double tolerance =
        (m_relativeTolerance * std::max(std::abs(*value), std::abs(actual))) + 1e-18;
    return std::abs(actual - *value) <= tolerance;
}

void GoldenCheck::fail(std::string_view pointer, std::string_view actual,
                       const nlohmann::json* golden)
{
    m_failures.push_back(std::format("{}: {} differs from the golden {}", pointer, actual,
                                     golden == nullptr ? std::string{"(none)"} : golden->dump()));
}

void GoldenCheck::number(std::string_view pointer, double actual)
{
    const nlohmann::json* golden = find(pointer);
    if (golden == nullptr || !matches(actual, *golden))
    {
        fail(pointer, std::format("{:.17g}", actual), golden);
    }
}

void GoldenCheck::coordinate(std::string_view pointer, const Coordinate& actual)
{
    const nlohmann::json*       golden = find(pointer);
    const std::array<double, 4> values{actual.x, actual.y, actual.z, actual.weight};
    bool ok = golden != nullptr && golden->is_array() && golden->size() >= 3 &&
              golden->size() <= values.size();
    for (std::size_t i = 0; ok && i < golden->size(); ++i)
    {
        ok = matches(values.at(i), golden->at(i));
    }
    if (!ok)
    {
        fail(pointer, actual.toPreciseString(), golden);
    }
}

void GoldenCheck::coordinates(std::string_view pointer, std::span<const Coordinate> actual)
{
    const nlohmann::json* golden = find(pointer);
    if (golden == nullptr || !golden->is_array() || golden->size() != actual.size())
    {
        fail(pointer, std::format("{} coordinates", actual.size()), golden);
        return;
    }
    for (std::size_t i = 0; i < actual.size(); ++i)
    {
        coordinate(std::format("{}/{}", pointer, i), actual[i]);
    }
}

void GoldenCheck::string(std::string_view pointer, std::string_view actual)
{
    const nlohmann::json* golden = find(pointer);
    if (golden == nullptr || !golden->is_string() || golden->get<std::string>() != actual)
    {
        fail(pointer, std::format("\"{}\"", actual), golden);
    }
}

void GoldenCheck::boolean(std::string_view pointer, bool actual)
{
    const nlohmann::json* golden = find(pointer);
    if (golden == nullptr || !golden->is_boolean() || golden->get<bool>() != actual)
    {
        fail(pointer, actual ? "true" : "false", golden);
    }
}

void GoldenCheck::integer(std::string_view pointer, long long actual)
{
    const nlohmann::json* golden = find(pointer);
    if (golden == nullptr || !golden->is_number_integer() || golden->get<long long>() != actual)
    {
        fail(pointer, std::to_string(actual), golden);
    }
}

}  // namespace QtRocket::Test
