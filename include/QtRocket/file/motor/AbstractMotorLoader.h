#pragma once

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/file/motor/MotorLoader.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// The shared part of the text motor loaders (OpenRocket's AbstractMotorLoader): load() decodes
/// the file in the loader's character set and hands the text to loadText(), and the static
/// helpers finish a thrust curve the way every loader does.
///
/// Text is UTF-8 inside QtRocket, so decoding turns the file's bytes into UTF-8: an ISO-8859-1
/// file maps every byte to its code point, a UTF-8 file keeps its text and has each malformed
/// byte replaced by U+FFFD (see Strings::toValidUtf8), which is the String Java's
/// InputStreamReader makes of the same bytes.
class AbstractMotorLoader : public MotorLoader
{
public:
    /// The character sets motor files are read in.
    enum class Charset
    {
        ISO_8859_1,  ///< RASP (.eng) files
        UTF_8,       ///< RockSim (.rse) files
    };

    /// Decodes @p data in getDefaultCharset() and reads the text with loadText().
    [[nodiscard]] Result<std::vector<ThrustCurveMotor::Builder>> load(
        std::span<const std::byte> data, std::string_view filename) const override;

    /// @p designation without a trailing delay ("B6-5" gives "B6"): ThrustCurveMotor::removeDelay,
    /// which holds OpenRocket's removeDelay() so that the motor layer can use it too.
    [[nodiscard]] static std::string removeDelay(std::string_view designation);

    /// @p bytes decoded in @p charset, as UTF-8 (see the class comment).
    [[nodiscard]] static std::string decode(std::span<const std::byte> bytes, Charset charset);

protected:
    AbstractMotorLoader() = default;

    /// Reads the motors from @p text, the decoded file (OpenRocket's load(Reader, String), renamed
    /// so that it does not hide load()).
    [[nodiscard]] virtual Result<std::vector<ThrustCurveMotor::Builder>> loadText(
        std::string_view text, std::string_view filename) const = 0;

    /// The character set files of this type are written in (getDefaultCharset()).
    [[nodiscard]] virtual Charset getDefaultCharset() const noexcept = 0;

    /// The mass of the motor at each time point (calculateMass()), assuming a constant exhaust
    /// velocity so that the mass flow follows the thrust (F = m' v): the mass lost between two
    /// points is the trapezoidal impulse 0.5 * (f0 + f1) * (t1 - t0), scaled so that the losses add
    /// up to @p prop, subtracted from @p total point by point and floored at zero. @p time and
    /// @p thrust have the same length, at least one (OpenRocket's get(0) on an empty list throws;
    /// here it is a BugError). A curve with no impulse gives an infinite or NaN scale, as in
    /// OpenRocket.
    [[nodiscard]] static std::vector<double> calculateMass(std::span<const double> time,
                                                           std::span<const double> thrust,
                                                           double total, double prop);

    /// The whitespace-separated tokens of @p str (split(String)): Java's str.split("\\s+") with a
    /// leading empty token dropped, which comes to the non-empty runs of characters other than
    /// space, \t, \n, \x0B, \f and \r ("" and "  " give none).
    [[nodiscard]] static std::vector<std::string> split(std::string_view str);

    /// The tokens of @p str separated by runs of the characters in @p delimiters: Java's
    /// split(str, "[<delimiters>]+"), the only regex form OpenRocket passes (split(String,
    /// String) takes any regex there).
    [[nodiscard]] static std::vector<std::string> split(std::string_view str,
                                                        std::string_view delimiters);

    /// Sorts @p primary ascending and applies the same permutation to each of @p lists
    /// (sortLists()): OpenRocket's repeated swap of the first adjacent pair out of order, which
    /// comes to a stable insertion sort in which an element moves left past its neighbour only
    /// while it is smaller (so nothing moves past a NaN). Every list is at least as long as
    /// @p primary.
    static void sortLists(
        std::vector<double>&                                               primary,
        std::initializer_list<std::reference_wrapper<std::vector<double>>> lists = {});

    /// Repairs the common defects at the ends of a sorted thrust curve (finalizeThrustCurve()),
    /// in OpenRocket's order, times and thrusts compared with MathUtil::equals:
    /// - a curve that does not start at time 0 gets a point (0, 0) in front, and each of @p lists
    ///   a copy of its first value;
    /// - of two points at time 0, the first is dropped from @p time and @p thrust only (OpenRocket
    ///   leaves @p lists as they are, so they become one longer and shift by one);
    /// - of two neighbouring points with the same time and thrust, the first is dropped from every
    ///   list, until no such pair is left;
    /// - of two final points at the same time, the one with zero thrust is dropped (the one before
    ///   the last when both are zero), from every list at that index.
    /// OpenRocket's console messages for the last two are not printed. An empty curve is left
    /// alone. A curve that is down to one point where OpenRocket reads a second one (a single
    /// point at time 0, or two identical points at time 0) fails with ErrorCode::PARSE and the
    /// message of the IndexOutOfBoundsException OpenRocket throws ("Index 1 out of bounds for
    /// length 1").
    [[nodiscard]] static Result<void> finalizeThrustCurve(
        std::vector<double>& time, std::vector<double>& thrust,
        std::initializer_list<std::reference_wrapper<std::vector<double>>> lists = {});

    /// The lines of @p text as Java's BufferedReader.readLine() returns them: split at "\n", "\r"
    /// and "\r\n", without the terminators, and with no empty line after a final terminator. The
    /// views point into @p text.
    [[nodiscard]] static std::vector<std::string_view> readLines(std::string_view text);
};

}  // namespace QtRocket
