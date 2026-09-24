#include "QtRocket/file/motor/RaspMotorLoader.h"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorDigest.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

constexpr std::string_view kHeaderFieldsMessage =
    "Illegal file format. Motor header line must contain 7 fields:<br>"
    "&nbsp designation diameter length delays propellantWeight totalWeight manufacturer";
constexpr std::string_view kDataFieldsMessage =
    "Illegal file format.<br>Data should only have 2 entries: a time and thrust value.";
constexpr std::string_view kNumberMessage =
    "Illegal file format. Could not convert value to a number.<br>"
    "Verify that each number is correctly formatted.";
constexpr std::string_view kTooShortMessage = "Illegal file format, too short thrust-curve.";

/// The delays at and above this are placeholders, not delays ("Many RASP files have "100" as an
/// only delay").
constexpr double kDelayLimit = 99;

/// Java's s.matches("[0-9]+").
[[nodiscard]] bool isDigits(std::string_view text) noexcept
{
    return !text.empty() && std::ranges::all_of(text, [](char c) { return c >= '0' && c <= '9'; });
}

/// The NumberFormatException OpenRocket turns into its "could not convert" message.
[[nodiscard]] std::unexpected<Error> numberError()
{
    return fail(ErrorCode::PARSE, std::string(kNumberMessage));
}

/// The delays of the header's delay pieces (split at runs of '-' and ','): "P" or "plugged" is
/// plugged, digits below 99 a delay, anything else nothing; sorted.
[[nodiscard]] std::vector<double> parseDelays(const std::vector<std::string>& pieces)
{
    std::vector<double> delays;
    for (const std::string& s : pieces)
    {
        if (Strings::javaEqualsIgnoreCase(s, "P") || Strings::javaEqualsIgnoreCase(s, "plugged"))
        {
            delays.push_back(Motor::kPluggedDelay);
        }
        else if (isDigits(s))
        {
            // Many RASP files have "100" as an only delay
            const double d = Strings::javaParseDouble(s).value_or(kDelayLimit);
            if (d < kDelayLimit)
            {
                delays.push_back(d);
            }
        }
    }
    // Collections.sort: the delays are non-negative integers or the plugged infinity, so plain <
    // gives Double.compareTo's order.
    std::ranges::sort(delays);
    return delays;
}

/// Reads the comment lines from @p lines[@p index] (lines starting with ';', and empty lines):
/// each trimmed without its ';' and followed by a newline, the whole trimmed. Leaves @p index at
/// the first other line; nullopt when the lines end first.
[[nodiscard]] std::optional<std::string> readComment(std::span<const std::string_view> lines,
                                                     std::size_t&                      index)
{
    std::string comment;
    for (; index < lines.size(); index++)
    {
        const std::string_view line = lines[index];
        if (!line.empty() && line.front() != ';')
        {
            return std::string(Strings::trim(comment));
        }
        if (!line.empty())
        {
            comment += Strings::trim(line.substr(1));
            comment += '\n';
        }
    }
    return std::nullopt;
}

}  // namespace

/// One motor's header line.
struct RaspMotorLoader::Header
{
    std::string         designation;
    double              diameter{0};
    double              length{0};
    std::vector<double> delays;
    double              propW{0};
    double              totalW{0};
    std::string         manufacturer;
};

Result<std::vector<ThrustCurveMotor::Builder>> RaspMotorLoader::load(
    std::span<const std::byte> data, std::string_view filename,
    bool removeDelayFromDesignation) const
{
    return loadText(decode(data, getDefaultCharset()), filename, removeDelayFromDesignation);
}

Result<std::vector<ThrustCurveMotor::Builder>> RaspMotorLoader::loadText(
    std::string_view text, std::string_view filename) const
{
    return loadText(text, filename, true);
}

Result<std::vector<ThrustCurveMotor::Builder>> RaspMotorLoader::loadText(
    std::string_view text, std::string_view filename, bool removeDelayFromDesignation)
{
    std::vector<ThrustCurveMotor::Builder> motors;
    const std::vector<std::string_view>    lines = readLines(text);
    std::size_t                            index = 0;
    while (index < lines.size())  // Until EOF
    {
        std::optional<std::string> comment = readComment(lines, index);
        if (!comment.has_value())
        {
            break;
        }

        // Parse header line, example:
        // F32 24 124 5-10-15-P .0377 .0695 RV
        // desig diam len delays prop.w tot.w manufacturer
        Result<Header> header = parseHeader(lines[index], filename);
        if (!header)
        {
            return std::unexpected(std::move(header.error()));
        }
        index++;

        // Read the data
        std::vector<double> time;
        std::vector<double> thrust;
        if (Result<void> read = readData(lines, index, time, thrust); !read)
        {
            return std::unexpected(std::move(read.error()));
        }

        // Comment or EOF encountered, marks the start of the next motor
        if (time.size() < 2)
        {
            return fail(ErrorCode::PARSE, std::string(kTooShortMessage));
        }
        Result<ThrustCurveMotor::Builder> motor =
            createRaspMotor(std::move(*header), std::move(*comment), std::move(time),
                            std::move(thrust), removeDelayFromDesignation);
        if (!motor)
        {
            return std::unexpected(std::move(motor.error()));
        }
        motors.push_back(std::move(*motor));
    }
    return motors;
}

Result<RaspMotorLoader::Header> RaspMotorLoader::parseHeader(std::string_view line,
                                                             std::string_view filename)
{
    const std::vector<std::string> pieces = split(line);
    if (pieces.size() != 7)
    {
        return fail(ErrorCode::PARSE, std::string(kHeaderFieldsMessage));
    }

    Header header;
    header.designation                   = pieces[0];
    const std::optional<double> diameter = Strings::javaParseDouble(pieces[1]);
    if (!diameter.has_value())
    {
        return numberError();
    }
    header.diameter                    = *diameter / 1000.0;
    const std::optional<double> length = Strings::javaParseDouble(pieces[2]);
    if (!length.has_value())
    {
        return numberError();
    }
    header.length = *length / 1000.0;

    if (!Strings::javaEqualsIgnoreCase(pieces[3], "None"))
    {
        header.delays = parseDelays(split(pieces[3], "-,"));
    }

    const std::optional<double> propW = Strings::javaParseDouble(pieces[4]);
    if (!propW.has_value())
    {
        return numberError();
    }
    const std::optional<double> totalW = Strings::javaParseDouble(pieces[5]);
    if (!totalW.has_value())
    {
        return numberError();
    }
    header.propW        = *propW;
    header.totalW       = *totalW;
    header.manufacturer = pieces[6];

    if (header.propW > header.totalW)
    {
        return fail(ErrorCode::PARSE,
                    "Propellant weight exceeds total weight in RASP file " + std::string(filename));
    }
    return header;
}

Result<void> RaspMotorLoader::readData(std::span<const std::string_view> lines, std::size_t& index,
                                       std::vector<double>& time, std::vector<double>& thrust)
{
    for (; index < lines.size() && (lines[index].empty() || lines[index].front() != ';'); index++)
    {
        const std::vector<std::string> buf = split(lines[index]);
        if (buf.empty())
        {
            continue;
        }
        if (buf.size() != 2)
        {
            return fail(ErrorCode::PARSE, std::string(kDataFieldsMessage));
        }
        const std::optional<double> t = Strings::javaParseDouble(buf[0]);
        if (!t.has_value())
        {
            return numberError();
        }
        const std::optional<double> f = Strings::javaParseDouble(buf[1]);
        if (!f.has_value())
        {
            return numberError();
        }
        time.push_back(*t);
        thrust.push_back(*f);
    }
    return {};
}

Result<ThrustCurveMotor::Builder> RaspMotorLoader::createRaspMotor(Header              header,
                                                                   std::string         comment,
                                                                   std::vector<double> time,
                                                                   std::vector<double> thrust,
                                                                   bool removeDelayFromDesignation)
{
    // Add zero time/thrust if necessary
    sortLists(time, {thrust});
    if (Result<void> finished = finalizeThrustCurve(time, thrust); !finished)
    {
        return std::unexpected(std::move(finished.error()));
    }
    const std::vector<double> mass = calculateMass(time, thrust, header.totalW, header.propW);

    std::vector<Coordinate> cg;
    cg.reserve(time.size());
    for (std::size_t i = 0; i < time.size(); i++)
    {
        cg.emplace_back(header.length / 2, 0, 0, mass[i]);
    }

    if (removeDelayFromDesignation)
    {
        header.designation = removeDelay(header.designation);
    }

    // Create the motor digest from data available in RASP files
    MotorDigest motorDigest;
    motorDigest.update(MotorDigest::DataType::TIME_ARRAY, time);
    motorDigest.update(MotorDigest::DataType::MASS_SPECIFIC,
                       {header.totalW, header.totalW - header.propW});
    motorDigest.update(MotorDigest::DataType::FORCE_PER_TIME, thrust);

    const Manufacturer&       m = Manufacturer::getManufacturer(header.manufacturer);
    ThrustCurveMotor::Builder builder;
    builder.setManufacturer(m)
        .setDesignation(std::move(header.designation))
        .setDescription(std::move(comment))
        .setMotorType(m.getMotorType())
        .setStandardDelays(std::move(header.delays))
        .setDiameter(header.diameter)
        .setLength(header.length)
        .setTimePoints(std::move(time))
        .setThrustPoints(std::move(thrust))
        .setCGPoints(std::move(cg))
        .setDigest(motorDigest.getDigest());
    return builder;
}

}  // namespace QtRocket
