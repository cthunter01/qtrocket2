#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/file/motor/AbstractMotorLoader.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// Reads RASP engine files, .eng (OpenRocket's RASPMotorLoader). A file holds one or more motors,
/// each an optional block of comment lines (starting with ';', which become the description,
/// trimmed and joined with newlines), a header line of seven whitespace-separated fields
///
///     F32 24 124 5-10-15-P .0377 .0695 RV
///     designation diameter(mm) length(mm) delays propellant(kg) total(kg) manufacturer
///
/// and a time and a thrust on each following line, up to the next comment line or the end.
/// Delays are separated by '-' or ','; "P" or "plugged" (any case) is a plugged motor, a number
/// below 99 a delay ("100" is a common placeholder), anything else is ignored, and "None" means
/// none; they are sorted. The curve is sorted and finished with AbstractMotorLoader's helpers, the
/// CG is placed at half the length, and the mass comes from calculateMass(). The digest covers
/// the times, the total and burnout masses (MASS_SPECIFIC) and the thrusts.
///
/// Failures (ErrorCode::PARSE, OpenRocket's messages, which contain HTML for its dialogs): a
/// header without seven fields, a data line without two, a value that is not a number (Java's
/// Double.parseDouble), a propellant weight above the total weight, a curve of fewer than two
/// points. Empty lines are skipped everywhere.
class RaspMotorLoader final : public AbstractMotorLoader
{
public:
    /// The character set of RASP files (CHARSET_NAME).
    static constexpr std::string_view kCharsetName = "ISO-8859-1";

    RaspMotorLoader() = default;

    using AbstractMotorLoader::load;

    /// load(), with the delay kept in the designation ("B6-0" instead of "B6") when
    /// @p removeDelayFromDesignation is false; the plain load() removes it.
    [[nodiscard]] Result<std::vector<ThrustCurveMotor::Builder>> load(
        std::span<const std::byte> data, std::string_view filename,
        bool removeDelayFromDesignation) const;

    /// loadText() with the choice of load(data, filename, removeDelayFromDesignation), for text
    /// that is already decoded.
    [[nodiscard]] static Result<std::vector<ThrustCurveMotor::Builder>> loadText(
        std::string_view text, std::string_view filename, bool removeDelayFromDesignation);

protected:
    [[nodiscard]] Result<std::vector<ThrustCurveMotor::Builder>> loadText(
        std::string_view text, std::string_view filename) const override;

    [[nodiscard]] Charset getDefaultCharset() const noexcept override
    {
        return Charset::ISO_8859_1;
    }

private:
    /// A motor's header line, read; defined in the source file.
    struct Header;

    /// The header line @p line, or the failure OpenRocket reports for it.
    [[nodiscard]] static Result<Header> parseHeader(std::string_view line,
                                                    std::string_view filename);

    /// Reads the data lines from @p lines[@p index] up to the next comment line or the end into
    /// @p time and @p thrust, leaving @p index at that comment line (or the end).
    [[nodiscard]] static Result<void> readData(std::span<const std::string_view> lines,
                                               std::size_t& index, std::vector<double>& time,
                                               std::vector<double>& thrust);

    /// createRASPMotor(): the motor of @p header and its curve, with the CG at the centre of the
    /// casing and the mass from the thrust curve.
    [[nodiscard]] static Result<ThrustCurveMotor::Builder> createRaspMotor(
        Header header, std::string comment, std::vector<double> time, std::vector<double> thrust,
        bool removeDelayFromDesignation);
};

}  // namespace QtRocket
