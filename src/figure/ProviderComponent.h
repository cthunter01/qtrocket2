#pragma once

// The cast with which a shape provider takes its component (internal to the figure/ subsystem).

#include <format>
#include <string_view>

#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Detail
{

/// @p component as the @p Component the provider @p provider draws: Java's cast at the top of
/// every getShapesSide and getShapesBack, whose ClassCastException is a BugError here (the
/// dispatch of RocketComponentShapeProvider never hands a provider another class).
template <class Component>
[[nodiscard]] const Component& providerComponent(const RocketComponent& component,
                                                 std::string_view       provider)
{
    const auto* const typed = dynamic_cast<const Component*>(&component);
    if (typed == nullptr)
    {
        bug(std::format("{} cannot draw component {} of class {}", provider, component.toString(),
                        className(component.kind())));
    }
    return *typed;
}

}  // namespace QtRocket::Detail
