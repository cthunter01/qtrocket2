#pragma once

#include <functional>
#include <string_view>

#include "QtRocket/file/openrocket/Setter.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class RocketComponent;

/// Sets an override value of a component (its mass, the position of its centre of gravity or
/// its drag coefficient) and switches the override on (OpenRocket's
/// file/openrocket/importt/OverrideSetter).
///
/// The text is read as Double.parseDouble reads it (white space around the number is dropped;
/// "0x1p-1" and "1.5d" are numbers). The value is set first, then the override is switched on:
/// the order matters, the component firing an event for each. A text that is no number adds
/// Warning::kFileInvalidParameter and changes nothing.
///
/// Deviation from OpenRocket (decision L3 of the loader): a NaN or an infinity is refused like
/// a text that is no number, with the same warning, and the override is not switched on.
/// OpenRocket stores what it reads: `<overridemass>NaN</overridemass>` gives a component with
/// an overridden mass of NaN, which it writes back, and "-Infinity" an overridden mass of 0
/// (the mass setter's own lower bound). A simulation of such a rocket computes with the NaN.
class OverrideSetter final : public Setter
{
public:
    /// What sets the override value on the component (Java: the setter method).
    using SetFunction = std::function<void(RocketComponent&, double)>;
    /// What switches the override on (Java: the "enabled" method).
    using EnabledFunction = std::function<void(RocketComponent&, bool)>;

    /// @throws BugError when @p set or @p enable is empty.
    OverrideSetter(SetFunction set, EnabledFunction enable);

    [[nodiscard]] Result<void> set(RocketComponent& component, std::string_view value,
                                   const Attributes& attributes, WarningSet& warnings,
                                   const DocumentLoadingContext& context) const override;

private:
    SetFunction     m_set;
    EnabledFunction m_enable;
};

}  // namespace QtRocket
