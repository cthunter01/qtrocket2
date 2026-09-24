#pragma once

#include <string_view>

namespace QtRocket
{

/// Orders motor designations (OpenRocket's DesignationComparator): first by impulse class
/// (1/8A < 1/4A < 1/2A < A < B ...), then by average thrust, then by whatever follows the thrust
/// ("G80-10" < "G80-4" < "G80T"). Designations that do not have that form sort after all that
/// do, by the collator. Letters and extras compare as Strings::javaPrimaryCollatorCompare() does,
/// so case is ignored ("g80t" equals "G80T").
///
/// The accepted forms, OpenRocket's pattern `^([0-9]+-?|1/([1-8]))?([a-zA-Z])([0-9,]+)(.*?)$`:
/// an optional prefix (a number such as a total impulse, "132G100", "132-G100", or a fraction
/// "1/2" ... "1/8"), the class letter, the average thrust (digits, commas ignored), and the rest,
/// which must not contain a line break ('.' does not match one; a single line break at the very
/// end is allowed by '$' and not part of the rest).
///
/// Deviation: where OpenRocket's Integer.parseInt throws on a thrust with no digits ("A,") or
/// beyond the int range, aborting the sort, such a designation counts here as one that does not
/// have the form.
class DesignationComparator
{
public:
    /// Negative, zero or positive as @p a sorts before, with or after @p b; for two designations
    /// of the same class the difference of their thrusts.
    [[nodiscard]] static int compare(std::string_view a, std::string_view b);

    /// compare(a, b) < 0: a strict weak ordering for std::sort.
    [[nodiscard]] bool operator()(std::string_view a, std::string_view b) const
    {
        return compare(a, b) < 0;
    }
};

}  // namespace QtRocket
