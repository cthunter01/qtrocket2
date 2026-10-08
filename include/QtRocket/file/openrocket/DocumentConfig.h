#pragma once

#include <array>
#include <concepts>
#include <functional>
#include <optional>
#include <ranges>
#include <string_view>
#include <type_traits>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

/// What every handler of the .ork loader shares (OpenRocket's
/// file/openrocket/importt/DocumentConfig): the format versions the loader knows, and the
/// helpers that read an enum constant and a number as an .ork file writes them.
///
/// Not here yet: Java's DocumentConfig also holds the table of component constructors by
/// element name and the table of setters by "Class:element" (see the HOOK line in the class).
///
/// Deviations from OpenRocket:
/// - findEnum() takes the constants and a function that gives a constant's Java name, where
///   Java takes the enum's class and reflects on it.
/// - stringToDouble() returns a failure with the message of Java's NumberFormatException,
///   where Java throws it.
/// - parseDouble() and parseInt() are Java's own Double.parseDouble and Integer.parseInt, which
///   the handlers call directly: here they return a failure with the message of the
///   NumberFormatException, so that every handler that lets one fail the load says what Java
///   says. parseInt() reads ASCII digits only; Integer.parseInt also takes the decimal digits
///   of other scripts (the fullwidth "12" is 12 there and a failure here).
/// - parseFiniteDouble() and isSupportedVersion() have no counterpart: the first is the
///   loader's rule for a number it must not apply when it is not finite, the second the loop
///   OpenRocketHandler makes over SUPPORTED_VERSIONS.
class DocumentConfig final
{
public:
    DocumentConfig() = delete;

    /// The format versions the loader knows, oldest first (SUPPORTED_VERSIONS). A file of
    /// another version is read all the same, with a warning. (The saver writes the newest of
    /// them at most: keep the two in step.)
    static constexpr std::array<std::string_view, 12> kSupportedVersions{
        "1.0", "1.1", "1.2", "1.3", "1.4", "1.5", "1.6", "1.7", "1.8", "1.9", "1.10", "1.11"};

    /// What turns a version "major.minor" into the integer the loader compares
    /// (FILE_VERSION_DIVISOR): major * 100 + minor, so 101 is "1.1" and 110 is "1.10". The
    /// integer divided by it is the major version and the remainder the minor one. This is the
    /// one constant of its kind: a saver takes it from here (OpenRocketSaver has an equal one
    /// of its own, which a JUnit test compares with this).
    static constexpr int kFileVersionDivisor = 100;

    // HOOK(loader-registry): run 9b adds the constructor and setter registries

    /// Whether @p version is one of kSupportedVersions, compared exactly ("1.10" is, "1.1 "
    /// and "01.1" are not).
    [[nodiscard]] static bool isSupportedVersion(std::string_view version) noexcept;

    /// The constant of @p constants whose Java name, as @p javaName gives it ("NOT_SIMULATED"),
    /// is written @p name in an .ork file, or nullopt when there is none (findEnum()): the
    /// name with its ASCII letters in lower case and without its underscores equals @p name
    /// trimmed as String.trim() trims (see Strings::orkEnumNameMatches()), so "notsimulated"
    /// and " notsimulated " match and "NOT_SIMULATED" and "not_simulated" do not. The first such
    /// constant in the order of @p constants is returned, as Java returns the first in declaration
    /// order. No @p name (Java: null) finds nothing. @p javaName may return a view, a reference
    /// or a string of its own.
    template <std::ranges::forward_range Constants, class JavaName>
        requires std::convertible_to<
            std::invoke_result_t<JavaName&, const std::ranges::range_value_t<Constants>&>,
            std::string_view>
    [[nodiscard]] static std::optional<std::ranges::range_value_t<Constants>> findEnum(
        std::optional<std::string_view> name, const Constants& constants, JavaName javaName)
    {
        if (!name.has_value())
        {
            return std::nullopt;
        }
        for (const auto& constant : constants)
        {
            // The name is compared in the expression that makes it: a std::string that the
            // function returns by value lives exactly that long, and a view of it kept in a
            // variable would dangle.
            if (Strings::orkEnumNameMatches(*name, std::invoke(javaName, constant)))
            {
                return constant;
            }
        }
        return std::nullopt;
    }

    /// A number as the flight data of an .ork file writes it (stringToDouble()): "NaN", "Inf"
    /// and "-Inf", each compared ignoring case and without trimming, are a NaN and the two
    /// infinities; anything else is what parseDouble() makes of it, with its failure. No text
    /// (Java: null) fails with ErrorCode::INVALID_ARGUMENT and "null string".
    [[nodiscard]] static Result<double> stringToDouble(std::optional<std::string_view> text);

    /// Java's Double.parseDouble (Strings::javaParseDouble(): white space trimmed, "NaN",
    /// "Infinity", hexadecimal and "1.5d" included; "Inf" is no number here), for the handlers
    /// that call it without catching what it throws. The failure is
    /// ErrorCode::INVALID_ARGUMENT (a NumberFormatException is an IllegalArgumentException,
    /// which fails a load with "Exception loading stream: " and the message) with Java's
    /// message:
    /// - "empty String" for a text that trims to nothing;
    /// - "multiple points" when the digits and points that start the number, after one sign,
    ///   hold a second point ("1..2", "0.0.0", "1.2.3e5"), unless the number starts as "NaN",
    ///   "Infinity" or a hexadecimal one does;
    /// - else `For input string: "<the text, trimmed>"`.
    /// A handler that fails the load passes the message on under the error code its own rule
    /// asks for. A missing text is not this function's case: Double.parseDouble(null) is a
    /// NullPointerException in Java, which each handler answers for itself.
    [[nodiscard]] static Result<double> parseDouble(std::string_view text);

    /// Java's Integer.parseInt (Strings::parseInt(): one optional sign and ASCII digits, nothing
    /// trimmed, within the int range). The failure is ErrorCode::INVALID_ARGUMENT with Java's
    /// message: `For input string: "<the text as it is>"` for every text that is no int ("",
    /// " 4", "+", "1.0", "2147483648"), and "Cannot parse null string" for no text (Java: null,
    /// a missing attribute).
    [[nodiscard]] static Result<int> parseInt(std::optional<std::string_view> text);

    /// What Java's Double.parseDouble makes of @p text when that is a finite number, and
    /// nullopt for a text that is no number, a NaN or an infinity. This is how a handler reads
    /// a number that a simulation run takes as its input (a simulation option, a delay, an
    /// altitude): OpenRocket stores an infinity it reads there and then fails in the flight,
    /// while QtRocket's loader does not apply a value that is not finite; the handler adds the
    /// warning it has for an unreadable value. The one infinity a file may hold, the ejection
    /// delay of a plugged motor, is the motor handler's own business.
    [[nodiscard]] static std::optional<double> parseFiniteDouble(std::string_view text) noexcept;
};

}  // namespace QtRocket
