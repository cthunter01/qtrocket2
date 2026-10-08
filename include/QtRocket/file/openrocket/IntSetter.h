#pragma once

#include <functional>
#include <optional>
#include <string_view>

#include "QtRocket/file/openrocket/Setter.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class RocketComponent;

/// Sets a whole number (OpenRocket's file/openrocket/importt/IntSetter).
///
/// The text is read as Integer.parseInt reads it, and nothing is trimmed: one optional sign and
/// digits within the range of an int, so "4", "+5", "-3" and "007" are numbers and " 4", "4 ",
/// "3.0", "" and "2147483648" are not. A text that is no number adds
/// Warning::kFileInvalidParameter and sets nothing.
///
/// Deviations from OpenRocket:
/// - A setter may have a maximum (decision L6 of the loader): a number above it is refused
///   with Warning::kFileInvalidParameter. OpenRocket has no bound, and a component that does
///   not clamp its count itself (the instances of a launch lug, a rail button, a ring, a pod
///   set or a booster set, the lines of a parachute) keeps any number, 2147483647 included.
///   The rocket's events being on while a file loads, every later change would then make an
///   array of that many instances. DocumentConfig::kMaxCount is the maximum the table gives
///   those setters.
/// - Integer.parseInt also takes the decimal digits of other scripts (the Arabic-Indic digit
///   four is 4 there); here only ASCII digits are digits (Strings::parseInt()).
class IntSetter final : public Setter
{
public:
    /// What sets the number on the component (Java: the setter method).
    using SetFunction = std::function<void(RocketComponent&, int)>;

    /// Sets any int, as OpenRocket does: for a number the component bounds itself.
    /// @throws BugError when @p set is empty (also the other constructor).
    explicit IntSetter(SetFunction set);

    /// Sets an int up to @p maximum and refuses a larger one with a warning.
    IntSetter(SetFunction set, int maximum);

    [[nodiscard]] Result<void> set(RocketComponent& component, std::string_view value,
                                   const Attributes& attributes, WarningSet& warnings,
                                   const DocumentLoadingContext& context) const override;

private:
    SetFunction        m_set;
    std::optional<int> m_maximum;
};

}  // namespace QtRocket
