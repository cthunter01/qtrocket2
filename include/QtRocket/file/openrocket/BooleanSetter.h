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

/// Sets a truth value (OpenRocket's file/openrocket/importt/BooleanSetter).
///
/// The text is trimmed (String.trim()) and must then be "true" or "false", compared as
/// String.equalsIgnoreCase() compares ("TRUE", " false "). Anything else ("yes", "1", "") adds
/// Warning::kFileInvalidParameter and sets nothing.
///
/// Deviation from OpenRocket: Java's extra arguments of the setter method (the `false` of
/// NoseCone.setFlipped(boolean, boolean)) are part of the function the table hands over.
class BooleanSetter final : public Setter
{
public:
    /// What sets the value on the component (Java: the setter method).
    using SetFunction = std::function<void(RocketComponent&, bool)>;

    /// @throws BugError when @p set is empty.
    explicit BooleanSetter(SetFunction set);

    [[nodiscard]] Result<void> set(RocketComponent& component, std::string_view value,
                                   const Attributes& attributes, WarningSet& warnings,
                                   const DocumentLoadingContext& context) const override;

private:
    SetFunction m_set;
};

}  // namespace QtRocket
