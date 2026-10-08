#include "goldens/AutomaticDimensions.h"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/RingComponent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/util/Error.h"

namespace QtRocket::Test
{

namespace
{

/// A boolean as a value of a pass.
[[nodiscard]] double flagValue(bool flag)
{
    return flag ? 1.0 : 0.0;
}

/// Whether @p a and @p b are equal as Java's Double.equals() says: both NaN, or the same bits.
[[nodiscard]] bool sameValue(double a, double b)
{
    if (std::isnan(a) || std::isnan(b))
    {
        return std::isnan(a) && std::isnan(b);
    }
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

/// The part of AutomaticDimensions.snapshot() for one component: appends to @p values what the
/// refreshing getters of @p component return. Java tests the classes one after the other, not
/// one instead of the other (a body tube is a symmetric component too), in this order, and
/// calls the getters in this order.
void appendDimensions(std::vector<double>& values, const RocketComponent& component)
{
    if (const auto* tube = dynamic_cast<const BodyTube*>(&component))
    {
        values.push_back(tube->getOuterRadius());
        values.push_back(tube->getInnerRadius());
    }
    if (const auto* symmetric = dynamic_cast<const SymmetricComponent*>(&component))
    {
        values.push_back(symmetric->getForeRadius());
        values.push_back(symmetric->getAftRadius());
        values.push_back(flagValue(symmetric->usesPreviousCompAutomatic()));
        values.push_back(flagValue(symmetric->usesNextCompAutomatic()));
    }
    if (const auto* ring = dynamic_cast<const RingComponent*>(&component))
    {
        values.push_back(ring->getOuterRadius());
        values.push_back(ring->getInnerRadius());
    }
    if (const auto* massObject = dynamic_cast<const MassObject*>(&component))
    {
        values.push_back(massObject->getRadius());
        values.push_back(massObject->getLength());
    }
}

}  // namespace

std::vector<double> automaticDimensions(const Rocket& rocket)
{
    std::vector<double> values;
    // ComponentIndex.components(): every component in depth-first pre-order, the rocket first.
    for (const RocketComponent& component : rocket.subtree())
    {
        appendDimensions(values, component);
    }
    return values;
}

bool samePass(const std::vector<double>& a, const std::vector<double>& b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); i++)
    {
        if (!sameValue(a[i], b[i]))
        {
            return false;
        }
    }
    return true;
}

Result<int> settleBy(const std::function<std::vector<double>()>& pass, std::string_view name)
{
    std::vector<double> previous = pass();
    for (int passes = 0; passes < kMaxSettlingPasses; passes++)
    {
        std::vector<double> current = pass();
        if (samePass(current, previous))
        {
            return passes;
        }
        previous = std::move(current);
    }
    return fail(ErrorCode::UNKNOWN,
                std::format("The automatic dimensions of {} did not settle in {} passes", name,
                            kMaxSettlingPasses));
}

Result<int> settleAutomaticDimensions(const Rocket& rocket)
{
    return settleBy([&rocket] { return automaticDimensions(rocket); }, rocket.getName());
}

}  // namespace QtRocket::Test
