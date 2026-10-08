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
/// - A setter of the number of instances of its component (InstanceCount) also refuses, with
///   the same warning, a number that would give the rocket more instances than
///   DocumentConfig::kMaxInstances over all its flight configurations
///   (DocumentConfig::instanceCountFits()): the counts of nested components multiply, so the
///   maximum of each count by itself bounds nothing (a pod set of 10000 instances with a
///   launch lug of 10000). The component then keeps the count it has. OpenRocket has no such
///   bound and runs out of memory.
/// - Integer.parseInt also takes the decimal digits of other scripts (the Arabic-Indic digit
///   four is 4 there); here only ASCII digits are digits (Strings::parseInt()).
class IntSetter final : public Setter
{
public:
    /// What sets the number on the component (Java: the setter method).
    using SetFunction = std::function<void(RocketComponent&, int)>;

    /// Says that the number of a setter is how many instances its component has
    /// (RocketComponent::getInstanceCount(): the instances of a launch lug, the fins of a fin
    /// set).
    struct InstanceCount
    {
        /// The most the component keeps of a number it is given: DocumentConfig::kMaxCount for
        /// a component that keeps any number, 8 for a fin set, which clamps.
        int kept;
        /// Whether a number above `kept` is refused with a warning (decision L6: the component
        /// has no bound of its own); else it is set, and the component clamps it.
        bool refuseAbove;
    };

    /// Sets any int, as OpenRocket does: for a number the component bounds itself.
    /// @throws BugError when @p set is empty (also the other constructors).
    explicit IntSetter(SetFunction set);

    /// Sets an int up to @p maximum and refuses a larger one with a warning.
    IntSetter(SetFunction set, int maximum);

    /// Sets the number of instances of the component, within the bounds @p instances says and
    /// within the rocket's instance budget (see the class comment).
    IntSetter(SetFunction set, InstanceCount instances);

    [[nodiscard]] Result<void> set(RocketComponent& component, std::string_view value,
                                   const Attributes& attributes, WarningSet& warnings,
                                   const DocumentLoadingContext& context) const override;

private:
    SetFunction        m_set;
    std::optional<int> m_maximum;
    /// For a setter of an instance count: the most the component keeps.
    std::optional<int> m_instancesKept;
};

}  // namespace QtRocket
