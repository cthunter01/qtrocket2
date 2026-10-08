#pragma once

#include <string_view>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class RocketComponent;

/// What sets one parameter of a component from the element an .ork file holds for it, such as
/// `<length>0.3</length>` (OpenRocket's file/openrocket/importt/Setter). DocumentConfig keeps
/// one setter per parameter in its table, and the handler of a component's parameters applies
/// the one the table has for the element that closes.
///
/// A setter is immutable and shared by every load: set() is const and keeps nothing.
///
/// Deviations from OpenRocket:
/// - set() also takes the environment of the load. Java's setters ask globals for what they
///   need (the application's materials, the preset database); here those come with the
///   DocumentLoadingContext.
/// - set() returns a failure where a Java setter lets an IllegalArgumentException leave it,
///   which fails the whole load (the id of a component is the one case among the setters of
///   the table). Everything else a file can hold is a warning, as in Java.
/// - Java's setters call a component's method through reflection. Here they hold a function,
///   and the table hands each one a function that casts the component to its class; a setter
///   that meets a component of another class is a mistake in the table and throws BugError
///   (Java: a BugException out of Reflection.Method.invoke()).
class Setter
{
public:
    /// The attributes of the element (ElementHandler::Attributes).
    using Attributes = ElementHandler::Attributes;

    Setter(const Setter&)            = delete;
    Setter(Setter&&)                 = delete;
    Setter& operator=(const Setter&) = delete;
    Setter& operator=(Setter&&)      = delete;
    virtual ~Setter()                = default;

    /// Sets the parameter of @p component from the text @p value of the element and its
    /// @p attributes. What cannot be read or applied adds a warning to @p warnings and leaves
    /// the component as it was, unless the setter says otherwise; a failure ends the load.
    [[nodiscard]] virtual Result<void> set(RocketComponent& component, std::string_view value,
                                           const Attributes& attributes, WarningSet& warnings,
                                           const DocumentLoadingContext& context) const = 0;

protected:
    Setter() = default;
};

}  // namespace QtRocket
