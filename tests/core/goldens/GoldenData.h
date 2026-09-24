#pragma once

// Access to the golden reference data that tools/openrocket-goldens dumps from OpenRocket into
// tests/data/goldens (see tools/openrocket-goldens/README.md for the file formats). Test-only: the
// core never reads these files.

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "QtRocket/util/Error.h"

namespace QtRocket::Test
{

/// The schema version the golden files are written with (GoldenDumper.SCHEMA_VERSION).
inline constexpr int kGoldenSchemaVersion = 1;

/// tests/data/goldens in the source tree.
[[nodiscard]] std::filesystem::path goldensDir();

/// Reads and parses a golden JSON file. A relative @p path is taken relative to goldensDir().
/// Fails with ErrorCode::IO when the file cannot be read and ErrorCode::PARSE when it is not JSON.
[[nodiscard]] Result<nlohmann::json> loadGoldenJson(const std::filesystem::path& path);

/// Parses golden JSON text (ErrorCode::PARSE when it is not JSON).
[[nodiscard]] Result<nlohmann::json> parseGoldenJson(std::string_view text);

/// A number as the golden files store it: a JSON number, or one of the strings "NaN",
/// "Infinity" and "-Infinity" for the values JSON cannot represent. nullopt for anything else.
[[nodiscard]] std::optional<double> goldenNumber(const nlohmann::json& value);

/// The time series of one simulation branch (a sim_<sim>_branch<i>.csv.gz file): the column keys
/// of the header line and the rows of values, each row as long as the header.
struct GoldenTable
{
    std::vector<std::string>         columns;
    std::vector<std::vector<double>> rows;

    /// The index of the column with key @p key ("altitude", "custom:<name>"), or nullopt.
    [[nodiscard]] std::optional<std::size_t> columnIndex(std::string_view key) const;

    /// The values of column @p key, or nullopt when there is no such column.
    [[nodiscard]] std::optional<std::vector<double>> column(std::string_view key) const;
};

/// Reads a gzip-compressed branch CSV. A relative @p path is taken relative to goldensDir().
[[nodiscard]] Result<GoldenTable> loadGoldenCsv(const std::filesystem::path& path);

/// Parses branch CSV text: a header line of column keys, then one line of values per row
/// (Java's Double.toString, "NaN" and "Infinity" included), every line ended by '\n'. Fails with
/// ErrorCode::PARSE on an empty text, a row whose length differs from the header's, or a value
/// that is not a number.
[[nodiscard]] Result<GoldenTable> parseGoldenCsv(std::string_view text);

/// One simulation of a golden input, as listed in manifest.json.
struct GoldenSimulation
{
    std::string              name;
    std::string              json;      ///< sim_<sim>.json, relative to goldensDir()
    std::vector<std::string> branches;  ///< the branch CSV files (none for a skipped simulation)
};

/// One input design (an example file or a TestRockets factory), as listed in manifest.json.
/// File names are relative to goldensDir().
struct GoldenInput
{
    std::string                   name;    ///< e.g. "example-a-simple-model-rocket"
    std::string                   kind;    ///< "example" or "testrocket"
    std::string                   source;  ///< the data/examples file or the TestRockets method
    std::string                   geometry;
    std::string                   mass;
    std::string                   aero;
    std::string                   resave;  ///< OpenRocket's re-save of the design (XML)
    std::vector<GoldenSimulation> simulations;
};

/// manifest.json: where the goldens came from and which files they are.
struct GoldenManifest
{
    int                      schemaVersion{};
    std::string              openrocketCommit;
    std::string              openrocketVersion;
    std::vector<GoldenInput> inputs;

    /// The input named @p name, or null.
    [[nodiscard]] const GoldenInput* find(std::string_view name) const;
};

/// Reads goldensDir()/manifest.json.
[[nodiscard]] Result<GoldenManifest> loadGoldenManifest();

/// Interprets parsed manifest JSON (ErrorCode::PARSE when a required field is missing or has the
/// wrong type).
[[nodiscard]] Result<GoldenManifest> parseGoldenManifest(const nlohmann::json& manifest);

/// The results of configuration @p index of an aero.json document: that configuration's own
/// entry, or, when its "sameResultsAs" names an earlier configuration (one that differs only in
/// its motors), that configuration's entry. Null when @p index is out of range or the reference
/// is broken.
[[nodiscard]] const nlohmann::json* aeroResults(const nlohmann::json& aero, std::size_t index);

}  // namespace QtRocket::Test
