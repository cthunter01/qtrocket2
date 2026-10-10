#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/file/openrocket/Setter.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/SimulationInputRanges.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class RocketComponent;

/// Sets a number, or switches a flag on when the text is a special word such as "auto"
/// (OpenRocket's file/openrocket/importt/DoubleSetter).
///
/// set() trims the text (String.trim()) and reads a word and a number from it:
/// - Without a separator both are the whole text.
/// - With a separator the text is split at it as String.split() splits (empty parts at the end
///   are dropped). More than one part: the word is the first part and the number the others
///   joined with the separator again, so "auto 0.0125" is the word "auto" and the number
///   "0.0125", and "auto  0.03" the number " 0.03". One part: both are the whole text.
/// - The number is read unless the word is the special word and the text was one part. It is
///   read as Double.parseDouble reads ("1e-1d" and "0x1p-3" are numbers, white space around it
///   is dropped). A text that is no number adds the warning below and the setter goes on. A
///   NaN or an infinity adds the same warning and ends the setter. Any other number is
///   multiplied by the multiplier and set (unless the setter has a range, see "Deviations").
/// - Last, when the word is the special word, the flag is set to true.
/// The special word is compared as String.equalsIgnoreCase() compares ("AUTO", "Auto").
///
/// So the order is 'value, then flag', and what a text does is: "auto 0.0125" sets the value
/// and then the flag; "auto" and "AUTO" set the flag only; "auto abc" warns and still sets the
/// flag; "auto NaN" warns and sets nothing; "0.01 auto" warns about 'auto' and sets nothing,
/// the word being "0.01". Without a separator "filled 0.001" is no special word and no number:
/// a warning.
///
/// The warning is OpenRocket's: Warning::kFileInvalidParameter's text, then " data: '", the
/// text of the number, "' - " and the name of the component, e.g.
/// "Invalid parameter encountered, ignoring. data: 'abc' - Body Tube".
///
/// Deviations from OpenRocket:
/// - Java's extra arguments of the setter method (the `false` of setThickness(double, boolean))
///   and its form for the default of a parameter set (a getter of the set, then the setter of
///   its default value) are part of the function the table hands over. The checks come first
///   in both, so nothing changes in what a text does.
/// - The separator is one character where Java takes a regular expression; the only separator
///   OpenRocket uses is a blank.
/// - Java's form with a special word and no flag method (setForeShoulderRadius(double,
///   boolean) is registered that way, for its extra argument) is the plain form here.
/// - A number whose product with the multiplier is not finite is refused like an infinity
///   (decision L3 of the loader). Java checks the number as read and would set the infinity
///   that "1e308" gives with a multiplier of 2. No multiplier of the setter table is above 1.
/// - A setter may have a range (the constructor with a SimulationInputRanges::Range): a
///   finite number outside it is refused like a NaN, and nothing is set, but with a warning
///   of QtRocket's own that names the range, LoaderWarnings::numberOutsideRange() ("A number
///   outside its range, from 0 to 1000000 s, was refused; the component keeps what it had."):
///   OpenRocket's text is given only where OpenRocket gives it, for a NaN, an infinity and a
///   text that is no number. Java has no such form. The four numbers of a component that a
///   simulation holds to a range have it: the deployment delay and altitude of a recovery
///   device and the separation delay and altitude of a stage, as older files give them
///   (decision U9 of tier 9; OpenRocket sets any number).
class DoubleSetter final : public Setter
{
public:
    /// What sets the number on the component (Java: the setter method).
    using SetFunction = std::function<void(RocketComponent&, double)>;
    /// What sets the flag of the special word on the component (Java: the special method).
    using SpecialFunction = std::function<void(RocketComponent&, bool)>;

    /// Sets the number.
    /// @throws BugError when @p set is empty (also the other constructors).
    explicit DoubleSetter(SetFunction set);

    /// Sets the number times @p multiplier (pi / 180 for a parameter the file holds in
    /// degrees).
    DoubleSetter(SetFunction set, double multiplier);

    /// Sets the number when it lies in @p range, and refuses any other with a warning that
    /// names the range (see the class comment, "Deviations").
    DoubleSetter(SetFunction set, const SimulationInputRanges::Range& range);

    /// Sets the number, or the flag when the text is @p special.
    /// @throws BugError when @p specialSet is empty (also the next constructor).
    DoubleSetter(SetFunction set, std::string special, SpecialFunction specialSet);

    /// Sets the number, or the flag when the text is @p special, or both when the text is
    /// @p special, @p separator and the number.
    DoubleSetter(SetFunction set, std::string special, char separator, SpecialFunction specialSet);

    [[nodiscard]] Result<void> set(RocketComponent& component, std::string_view value,
                                   const Attributes& attributes, WarningSet& warnings,
                                   const DocumentLoadingContext& context) const override;

private:
    SetFunction                m_set;
    std::optional<std::string> m_special;
    SpecialFunction            m_specialSet;
    double                     m_multiplier{1.0};
    std::optional<char>        m_separator;
    /// The range the number has to lie in, when it has one.
    std::optional<SimulationInputRanges::Range> m_range;
};

}  // namespace QtRocket
