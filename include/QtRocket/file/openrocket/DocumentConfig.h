#pragma once

#include <array>
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

class RocketComponent;
class Setter;

/// What every handler of the .ork loader shares (OpenRocket's
/// file/openrocket/importt/DocumentConfig): the format versions the loader knows, the table of
/// the components a file can hold, the table of the setters of their parameters, and the
/// helpers that read an enum constant and a number as an .ork file writes them.
///
/// The component table (Java: constructors) makes a component for an element name:
/// createComponent(). It has 22 names for 21 classes, "boosterset" being the old name of a
/// "parallelstage". "rocket" is not among them: a file's one rocket is the document's, and a
/// second <rocket> below it is an unknown element.
///
/// The setter table (Java: setters) has one entry per parameter of a class, under the key
/// "Class:element" with the class's Java name: a setter ("BodyTube:radius"), or the refusal of
/// the element for that class and the classes below it (Java: a null entry; a nose cone has no
/// fore shoulder, so "NoseCone:foreradius" and four more refuse what "Transition:foreradius"
/// would set). findSetter() walks the Java superclasses of a component's kind, nearest first,
/// as ComponentParameterHandler does, and stops at the first class that has an entry. So an
/// entry of a class hides the entry of the same element higher up ("Parachute:preset" before
/// "RocketComponent:preset"), and a booster set finds the entries of AxialStage.
///
/// The loader has no version switch: every element of every format version from 1.0 on is in
/// the one table, the old names next to the new ones (position and axialoffset, fincount and
/// instancecount, rotation and angleoffset, overridesubcomponents and the three flags it sets).
///
/// HOOK(R2): 22 of Java's 135 entries are not in the table yet, those of the setters that part
/// R2 of run 9b adds (the position, material, preset, fin tab and cluster setters). Each has a
/// HOOK(R2) line at its place in DocumentConfig.cpp. Until then their elements are unknown.
///
/// Deviations from OpenRocket:
/// - The tables are private and reached through createComponent(), componentElements(),
///   findSetter(), setterKeys() and refusedKeys() (Java: two package-private HashMaps). The
///   setters call the components' methods through functions the table makes, not through
///   reflection, so a class with no entry of its own needs none: the walk is over the names
///   componentClassChain() gives.
/// - A setter that is applied to a component of another class than the one of its key throws
///   BugError (Java: a BugException out of Reflection.Method.invoke()). The walk cannot bring
///   that about; only a caller that picks a setter by hand can.
/// - The setters of the instance counts that no component bounds, and of a parachute's line
///   count, refuse a number above kMaxCount (decision L6 of the loader; see IntSetter).
/// - "RocketComponent:id" fails the load for a text that is no UUID, as in Java, with
///   ErrorCode::INVALID_ARGUMENT and the message of Java's exception (Uuid::javaFromString()).
/// - attribute() has no counterpart: it is HashMap.get() for an element's attributes, which
///   every handler and setter that reads one needs.
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

    /// The largest count a file may set where the component has no bound of its own (decision
    /// L6 of the loader): the instances of a launch lug, a rail button, a centering ring or
    /// bulkhead, a pod set and a booster set, and the lines of a parachute. A larger number is
    /// refused with Warning::kFileInvalidParameter. OpenRocket has no such bound; with the
    /// rocket's events on while a file loads, a count of two thousand million would make the
    /// loader build that many instances at every later change.
    static constexpr int kMaxCount = 10000;

    /// The element names of the components a file can hold (the keys of Java's constructors),
    /// sorted: the 22 names createComponent() knows.
    [[nodiscard]] static std::span<const std::string_view> componentElements() noexcept;

    /// A new component of the class the component table has for @p element, as its default
    /// constructor makes it, or null when the table has no such element (Java:
    /// constructors.get(element), then newInstance()). The name is compared exactly. "stage"
    /// is an AxialStage, "boosterset" and "parallelstage" a ParallelStage; "rocket" has no
    /// entry, although componentKindFromXmlName() knows it.
    ///
    /// Java's constructors take a new component's materials from the application's
    /// preferences; here a new component has the built-in defaults (see ExternalComponent).
    [[nodiscard]] static std::unique_ptr<RocketComponent> createComponent(std::string_view element);

    /// What the walk along a component's classes found for an element.
    struct SetterLookup
    {
        /// The setter to apply; null when the element is refused or no class knows it.
        const Setter* setter{nullptr};
        /// The Java name of the class whose entry ended the walk ("Transition" for the shape
        /// of a nose cone); empty when no class of the chain has an entry for the element.
        std::string_view owner;

        /// Whether a class of the chain refuses the element (Java: a null entry).
        [[nodiscard]] bool isRefused() const noexcept
        {
            return setter == nullptr && !owner.empty();
        }
    };

    /// The setter of the parameter element @p element for a component of @p kind (the search
    /// of ComponentParameterHandler.closeElement()): for each class of
    /// componentClassChain(kind), nearest first, the entry "Class:element" of the setter table;
    /// the first class that has one ends the walk, with its setter or with its refusal. The
    /// name is compared exactly.
    ///
    /// The caller applies the setter, and for a null setter, refused or unknown alike, adds
    /// OpenRocket's warning "Unknown parameter type '<element>' for <component name>,
    /// ignoring.".
    [[nodiscard]] static SetterLookup findSetter(ComponentKind kind, std::string_view element);

    /// The keys of the setter table that have a setter ("BodyTube:radius"), sorted.
    [[nodiscard]] static std::vector<std::string_view> setterKeys();

    /// The keys of the setter table that refuse their element (Java: the null entries), sorted:
    /// the five of NoseCone.
    [[nodiscard]] static std::vector<std::string_view> refusedKeys();

    /// The attribute @p name of an element, or nullopt when the element has none (Java:
    /// attributes.get(name), null when absent). The name is compared exactly. The view is into
    /// @p attributes.
    [[nodiscard]] static std::optional<std::string_view> attribute(
        const ElementHandler::Attributes& attributes, std::string_view name);

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
