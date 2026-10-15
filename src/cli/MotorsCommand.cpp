#include "QtRocket/cli/MotorsCommand.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/cli/CliOutput.h"
#include "QtRocket/cli/ExitCode.h"
#include "QtRocket/file/motor/MotorDatabaseLoader.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The header line of the table.
constexpr std::string_view kHeader =
    "Manufacturer\tDesignation\tCommon name\tType\tDiameter (m)\tLength (m)\tTotal impulse (Ns)\t"
    "Average thrust (N)\tBurn time (s)\tDelays (s)\tDigest";

/// Whether @p c is whitespace to Java's regex \s.
[[nodiscard]] bool isJavaWhitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\x0B' || c == '\f' || c == '\r';
}

/// @p text as a field of the table: valid UTF-8, a tab or a line break as a space.
[[nodiscard]] std::string cell(std::string_view text)
{
    std::string valid = Strings::toValidUtf8(text);
    for (char& c : valid)
    {
        if (c == '\t' || c == '\n' || c == '\r')
        {
            c = ' ';
        }
    }
    return valid;
}

/// The delays of @p motor as the table shows them.
[[nodiscard]] std::string delays(const ThrustCurveMotor& motor)
{
    std::string text;
    for (const double delay : motor.getStandardDelays())
    {
        if (!text.empty())
        {
            text += ',';
        }
        text += ThrustCurveMotor::getDelayString(delay);
    }
    return text;
}

/// Prints the line of @p motor.
void printMotor(const ThrustCurveMotor& motor, CliOutput& output)
{
    output.outLine("{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}",
                   cell(motor.getManufacturer().getDisplayName()), cell(motor.getDesignation()),
                   cell(motor.getCommonName()), name(motor.getMotorType()), motor.getDiameter(),
                   motor.getLength(), motor.getTotalImpulseEstimate(),
                   motor.getAverageThrustEstimate(), motor.getBurnTimeEstimate(), delays(motor),
                   cell(motor.getDigest()));
}

/// Prints the table of the sets of @p database for which @p keep is true.
template <class Keep>
void printTable(const ThrustCurveMotorSetDatabase& database, const Keep& keep, CliOutput& output)
{
    output.outLine("{}", kHeader);
    for (const ThrustCurveMotorSet& set : database.getMotorSets())
    {
        if (!keep(set))
        {
            continue;
        }
        for (const std::shared_ptr<const ThrustCurveMotor>& motor : set.getMotors())
        {
            printMotor(*motor, output);
        }
    }
}

}  // namespace

ExitCode MotorsCommand::list(const ThrustCurveMotorSetDatabase& database,
                             const std::optional<std::string>& search, CliOutput& output)
{
    const std::vector<std::string> terms =
        search.has_value() ? searchTerms(*search) : std::vector<std::string>{};
    printTable(
        database, [&terms](const ThrustCurveMotorSet& set) { return matches(set, terms); }, output);
    return ExitCode::OK;
}

ExitCode MotorsCommand::load(std::string_view file, CliOutput& output)
{
    MotorDatabaseLoader                        loader;
    const std::array<std::filesystem::path, 1> files{pathFromUtf8(file)};
    loader.loadUserDefinedMotors(files);
    for (const MotorDatabaseLoader::Problem& problem : loader.getProblems())
    {
        output.errLine("QtRocket_cli: cannot load '{}': {}", pathToUtf8(problem.file),
                       problem.error.message);
    }
    printTable(loader.getDatabase(), [](const ThrustCurveMotorSet&) { return true; }, output);
    return loader.getProblems().empty() ? ExitCode::OK : ExitCode::LOAD;
}

std::vector<std::string> MotorsCommand::searchTerms(std::string_view text)
{
    std::vector<std::string> terms;
    std::size_t              start = 0;
    while (start < text.size())
    {
        while (start < text.size() && isJavaWhitespace(text[start]))
        {
            start++;
        }
        std::size_t end = start;
        while (end < text.size() && !isJavaWhitespace(text[end]))
        {
            end++;
        }
        if (end > start)
        {
            terms.push_back(Strings::javaCaseFold(text.substr(start, end - start)));
        }
        start = end;
    }
    return terms;
}

bool MotorsCommand::matches(const ThrustCurveMotorSet& set, std::span<const std::string> terms)
{
    const Manufacturer*              manufacturer = set.getManufacturer();
    const std::array<std::string, 4> texts{
        Strings::javaCaseFold(manufacturer != nullptr ? manufacturer->getDisplayName() : ""),
        Strings::javaCaseFold(set.getCommonName()), Strings::javaCaseFold(set.getDesignation()),
        Strings::javaCaseFold(set.getCaseInfo())};
    return std::ranges::all_of(terms, [&texts](const std::string& term) {
        return std::ranges::any_of(
            texts, [&term](const std::string& text) { return text.contains(term); });
    });
}

}  // namespace QtRocket
