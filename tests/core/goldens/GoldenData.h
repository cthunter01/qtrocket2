#pragma once

// Access to the golden reference data that tools/openrocket-goldens dumps from OpenRocket into
// tests/data/goldens (see tools/openrocket-goldens/README.md for the file formats). Test-only: the
// core never reads these files.

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "QtRocket/util/Coordinate.h"
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

/// The entry of the component at @p path ("/0/1/2", the child indices from the rocket) in the
/// golden geometry of input @p input (<input>/geometry.json). Each geometry file is read and
/// parsed once per test run and kept, so the entry lives as long as the process. Fails as
/// loadGoldenJson() does, and with ErrorCode::NOT_FOUND when the file has no such component.
[[nodiscard]] Result<const nlohmann::json*> goldenGeometryComponent(std::string_view input,
                                                                    std::string_view path);

/// goldenGeometryComponent() for a test: the entry, or, after reporting why there is none as a
/// test failure (ADD_FAILURE()), an empty object, against which every GoldenCheck fails.
[[nodiscard]] const nlohmann::json& goldenGeometryComponentOrFail(std::string_view input,
                                                                  std::string_view path);

/// No mismatch: what GoldenCheck::failures() is when everything matches, for
/// EXPECT_EQ(check.failures(), noGoldenMismatches()).
[[nodiscard]] inline std::vector<std::string> noGoldenMismatches()
{
    return {};
}

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

/// The time series of the branch @p branch of a simulation document (an entry of its "branches"):
/// the name of the file its "csv" gives, which is in the document's directory, or nullopt when
/// "csv" is null. A document without time series is what a stable-step set holds that is written
/// as documents alone (GoldenManifest::hasStableTimeSeries()): the branch then still has its
/// number of rows, its columns with their minima and maxima, its events, its optimum altitude
/// and its separation time. Fails with ErrorCode::PARSE when @p branch is not an object or its
/// "csv" is missing or neither a string nor null.
[[nodiscard]] Result<std::optional<std::string>> goldenBranchCsv(const nlohmann::json& branch);

/// Parses branch CSV text: a header line of column keys, then one line of values per row
/// (Java's Double.toString, "NaN" and "Infinity" included), every line ended by '\n'. Fails with
/// ErrorCode::PARSE on an empty text, a row whose length differs from the header's, or a value
/// that is not a number.
[[nodiscard]] Result<GoldenTable> parseGoldenCsv(std::string_view text);

/// One simulation of a golden input, as listed in manifest.json.
struct GoldenSimulation
{
    std::string name;
    std::string json;  ///< sim_<sim>.json, relative to goldensDir()
    /// The branch CSV files. None for a skipped simulation, and none for a simulation of a
    /// stable-step set without time series (GoldenManifest::hasStableTimeSeries()), whose
    /// document alone says how many branches it has.
    std::vector<std::string> branches;
};

/// One input design (an example file or a TestRockets factory), as listed in manifest.json.
/// File names are relative to goldensDir().
struct GoldenInput
{
    std::string name;    ///< e.g. "example-a-simple-model-rocket"
    std::string kind;    ///< "example" or "testrocket"
    std::string source;  ///< the data/examples file or the TestRockets method
    /// The SHA-256 of the data/examples file the goldens were made from, as 64 hexadecimal
    /// digits ("sourceSha256"); empty for a test rocket, which is not made from a file.
    std::string                   sourceSha256;
    std::string                   geometry;
    std::string                   mass;
    std::string                   aero;
    std::string                   resave;  ///< OpenRocket's re-save of the design (XML)
    std::vector<GoldenSimulation> simulations;
    /// The stable-step set (<input>/stable/): the same simulations, in the same order, run with
    /// GoldenManifest::stableTimeStep. Empty for an input that has none (its entry has no
    /// "stableSimulations"; GoldenManifest::hasStableSimulations() says which inputs have them).
    std::vector<GoldenSimulation> stableSimulations;
};

/// manifest.json: where the goldens came from and which files they are.
struct GoldenManifest
{
    int         schemaVersion{};
    std::string openrocketCommit;
    std::string openrocketVersion;
    /// The time step of the stable-step set, in s ("settings.stableTimeStep").
    double stableTimeStep{};
    /// The kinds of the inputs that have a stable-step set ("settings.stableSimulationsOf").
    std::vector<std::string> stableSimulationsOf;
    /// The kinds of stableSimulationsOf whose stable-step set has the time series of its branches
    /// ("settings.stableTimeSeriesOf"). The set of the other kinds is the simulation documents
    /// alone: no branch CSV file, and "csv" of every branch is null (goldenBranchCsv()).
    std::vector<std::string> stableTimeSeriesOf;
    /// The salt of the component ids of a dump made with UUID_SALT ("settings.uuidSalt"); empty
    /// when the manifest has no such key. The committed data is never such a dump
    /// (GoldenSchema.ManifestDescribesItsSource).
    std::string uuidSalt;
    /// The pattern of a dump whose simulation runs were perturbed in the last bit, made with
    /// LAST_BIT ("settings.lastBitPerturbation"); empty when the manifest has no such key. The
    /// committed data is never such a dump either.
    std::string              lastBitPerturbation;
    std::vector<GoldenInput> inputs;

    /// The input named @p name, or null.
    [[nodiscard]] const GoldenInput* find(std::string_view name) const;

    /// Whether the inputs of the kind of @p input have a stable-step set.
    [[nodiscard]] bool hasStableSimulations(const GoldenInput& input) const;

    /// Whether the stable-step set of the inputs of the kind of @p input has the time series of
    /// its branches (false also for a kind without a stable-step set).
    [[nodiscard]] bool hasStableTimeSeries(const GoldenInput& input) const;
};

/// Reads goldensDir()/manifest.json.
[[nodiscard]] Result<GoldenManifest> loadGoldenManifest();

/// Interprets parsed manifest JSON (ErrorCode::PARSE when a required field is missing or has the
/// wrong type, or when "settings.stableTimeSeriesOf" names a kind that has no stable-step set).
[[nodiscard]] Result<GoldenManifest> parseGoldenManifest(const nlohmann::json& manifest);

/// The results of configuration @p index of an aero.json document: that configuration's own
/// entry, or, when its "sameResultsAs" names an earlier configuration (one that differs only in
/// its motors), that configuration's entry. Null when @p index is out of range or the reference
/// is broken.
[[nodiscard]] const nlohmann::json* aeroResults(const nlohmann::json& aero, std::size_t index);

/// Compares computed values with the entries of a golden JSON object (a component of a
/// geometry.json, say) and collects the mismatches, so that a test reports them all at once with
/// EXPECT_EQ(check.failures(), std::vector<std::string>{}). The comparisons are plain code, which
/// keeps them out of the test's cognitive complexity. Entries are addressed by JSON pointer
/// ("/componentMass", "/details/radius"); a missing entry, or one of the wrong type, is a
/// mismatch. Numbers (goldenNumber()) match within the relative tolerance, plus 1e-18 absolute
/// so that zeros compare; NaN matches NaN only and an infinity the same infinity only.
class GoldenCheck
{
public:
    /// Checks against @p golden, which must outlive this object.
    explicit GoldenCheck(const nlohmann::json& golden, double relativeTolerance = 1e-12) noexcept;

    /// @p actual against the golden number at @p pointer.
    void number(std::string_view pointer, double actual);

    /// @p actual against the golden list of numbers at @p pointer: the same length, and each
    /// element as number().
    void numbers(std::string_view pointer, std::span<const double> actual);

    /// @p actual against the golden [x, y, z] or [x, y, z, weight] at @p pointer.
    void coordinate(std::string_view pointer, const Coordinate& actual);

    /// @p actual against the golden list of coordinates at @p pointer: the same length, and
    /// each element as coordinate().
    void coordinates(std::string_view pointer, std::span<const Coordinate> actual);

    /// @p actual against the golden string at @p pointer.
    void string(std::string_view pointer, std::string_view actual);

    /// @p actual against the golden boolean at @p pointer.
    void boolean(std::string_view pointer, bool actual);

    /// @p actual against the golden integer at @p pointer.
    void integer(std::string_view pointer, long long actual);

    /// The mismatches found so far, one line each: "<pointer>: <actual> differs from the golden
    /// <golden>".
    [[nodiscard]] const std::vector<std::string>& failures() const noexcept { return m_failures; }

private:
    /// The entry at @p pointer, or null when there is none.
    [[nodiscard]] const nlohmann::json* find(std::string_view pointer) const;

    /// Whether @p actual matches @p expected (a golden number entry).
    [[nodiscard]] bool matches(double actual, const nlohmann::json& expected) const;

    /// Records a mismatch at @p pointer.
    void fail(std::string_view pointer, std::string_view actual, const nlohmann::json* golden);

    const nlohmann::json*    m_golden;
    double                   m_relativeTolerance;
    std::vector<std::string> m_failures;
};

}  // namespace QtRocket::Test
