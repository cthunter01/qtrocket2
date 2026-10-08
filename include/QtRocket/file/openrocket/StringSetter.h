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

/// Sets a parameter to the text of its element (OpenRocket's
/// file/openrocket/importt/StringSetter): the text as it is, nothing trimmed, so a name keeps
/// its blanks and a comment its line breaks. It adds no warning of its own.
///
/// Deviations from OpenRocket:
/// - The function returns a Result, and what it fails with fails the load. In Java the method
///   is called through reflection and an IllegalArgumentException it throws leaves the loader
///   (RocketComponent.setID(String) for a text that is no UUID).
/// - The function may be one that is handed the warnings of the load (WarningFunction), for a
///   text it refuses without failing the load: the table's setter of a component's id does,
///   for an id another component has (see DocumentConfig). Java's setter has no such form.
class StringSetter final : public Setter
{
public:
    /// What sets the text on the component (Java: the setter method).
    using SetFunction = std::function<Result<void>(RocketComponent&, std::string_view)>;

    /// What sets the text on the component and may add a warning for a text it does not set.
    using WarningFunction =
        std::function<Result<void>(RocketComponent&, std::string_view, WarningSet&)>;

    /// @throws BugError when @p set is empty (also the other constructor).
    explicit StringSetter(SetFunction set);

    explicit StringSetter(WarningFunction set);

    [[nodiscard]] Result<void> set(RocketComponent& component, std::string_view value,
                                   const Attributes& attributes, WarningSet& warnings,
                                   const DocumentLoadingContext& context) const override;

private:
    WarningFunction m_set;
};

}  // namespace QtRocket
