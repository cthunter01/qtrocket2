#pragma once

#include <functional>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

#include "QtRocket/file/openrocket/Setter.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class RocketComponent;

/// Sets a parameter to the constant of an enum that the text names (OpenRocket's
/// file/openrocket/importt/EnumSetter).
///
/// The constant is found as DocumentConfig.findEnum() finds it: the text, trimmed as
/// String.trim() trims, must equal the constant's Java name in lower case without its
/// underscores, so "upperignition" and " ogive " name a constant and "UPPER_IGNITION",
/// "upper_ignition" and "Ogive" do not. A text that names none adds
/// Warning::kFileInvalidParameter and sets nothing.
///
/// Deviations from OpenRocket:
/// - One class serves every enum (Java: a generic class, EnumSetter<T>); the constructor takes
///   the enum from the lookup function it is given.
/// - The enum's lookup is a function (Java: the enum's class, which findEnum() reflects on).
///   Every enum of the setter table has one that matches as findEnum() does:
///   finishFromOrkName(), transitionShapeFromOrkName(), lineStyleFromOrkName(), ...;
///   DocumentConfig::findEnum() is the general form.
/// - Java's form for the default of a parameter set (a getter of the set, then the setter of
///   its default value) is part of the function the table hands over.
class EnumSetter final : public Setter
{
public:
    /// A setter for the enum E: @p find gives the constant a text names, or nullopt (Java:
    /// DocumentConfig.findEnum() for the enum's class), and @p set sets it on the component
    /// (Java: the setter method).
    /// @throws BugError when @p find is null or @p set is empty.
    template <class E>
        requires std::is_enum_v<E>
    EnumSetter(std::optional<E> (*find)(std::string_view),
               std::function<void(RocketComponent&, std::type_identity_t<E>)> set)
    {
        QTROCKET_ASSERT(find != nullptr && set != nullptr);
        m_apply = [find, set = std::move(set)](RocketComponent& component, std::string_view text) {
            const std::optional<E> constant = find(text);
            if (!constant.has_value())
            {
                return false;
            }
            set(component, *constant);
            return true;
        };
    }

    [[nodiscard]] Result<void> set(RocketComponent& component, std::string_view value,
                                   const Attributes& attributes, WarningSet& warnings,
                                   const DocumentLoadingContext& context) const override;

private:
    /// Sets the constant @p text names on the component; false when it names none.
    std::function<bool(RocketComponent&, std::string_view)> m_apply;
};

}  // namespace QtRocket
