#include "QtRocket/gui/figure/FinPointSelection.h"

#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

/// Runs @p change, inside one freeze of @p rocket when @p freeze and the rocket is not frozen
/// already; thawed also when @p change throws.
template <class F>
void inFreezeIf(Rocket& rocket, bool freeze, const F& change)
{
    const bool own = freeze && !rocket.isFrozen();
    if (own)
    {
        rocket.freeze();
    }
    try
    {
        change();
    }
    catch (...)
    {
        if (own)
        {
            rocket.thaw();
        }
        throw;
    }
    if (own)
    {
        rocket.thaw();
    }
}

}  // namespace

FinPointSelection::FinPointSelection(Rocket& rocket, const Uuid& main, std::vector<Uuid> others)
  : m_rocket(&rocket), m_main(main), m_others(std::move(others))
{
}

FinPointSelection FinPointSelection::of(const ComponentSelection& selection)
{
    if (!selection.isValid())
    {
        bug("The fin point editor needs a valid selection");
    }
    if (dynamic_cast<const FreeformFinSet*>(&selection.getMain()) == nullptr)
    {
        bug("The fin point editor needs a freeform fin set");
    }
    std::vector<Uuid> others;
    for (const RocketComponent* other : selection.getOthers())
    {
        if (dynamic_cast<const FreeformFinSet*>(other) != nullptr)
        {
            others.push_back(other->getId());
        }
    }
    return {selection.getRocket(), selection.getMain().getId(), std::move(others)};
}

FreeformFinSet* FinPointSelection::main() const
{
    return dynamic_cast<FreeformFinSet*>(m_rocket->findComponent(m_main));
}

std::vector<FreeformFinSet*> FinPointSelection::others() const
{
    std::vector<FreeformFinSet*> found;
    found.reserve(m_others.size());
    for (const Uuid& id : m_others)
    {
        if (auto* fins = dynamic_cast<FreeformFinSet*>(m_rocket->findComponent(id)))
        {
            found.push_back(fins);
        }
    }
    return found;
}

Result<void> FinPointSelection::setPoint(std::size_t index, double x, double y) const
{
    FreeformFinSet* const fins = main();
    // Java returns before its loop without a parent and throws for an index out of range.
    if (fins == nullptr || fins->getParent() == nullptr)
    {
        return {};
    }
    if (index >= fins->getPointCount())
    {
        return fins->setPoint(index, x, y);
    }
    Result<void> result;
    inFreezeIf(*m_rocket, isMultiEdit(), [&] {
        for (FreeformFinSet* other : others())
        {
            if (const std::optional<std::size_t> otherIndex =
                    FreeformFinSet::getConfigListenerPointIdx(*fins, *other, index))
            {
                // Java ignores the listener's IllegalFinPointException
                [[maybe_unused]] const Result<void> ignored = other->setPoint(*otherIndex, x, y);
            }
        }
        result = fins->setPoint(index, x, y);
    });
    return result;
}

Result<void> FinPointSelection::addPoint(std::size_t index, Point2D location) const
{
    FreeformFinSet* const fins = main();
    if (fins == nullptr)
    {
        return fail(ErrorCode::NOT_FOUND, "The fin set is no longer in the rocket");
    }
    // Java throws before its loop for these.
    if (index < 1 || index + 1 > fins->getPointCount())
    {
        return fins->addPoint(index, location);
    }
    Result<void> result;
    inFreezeIf(*m_rocket, isMultiEdit(), [&] {
        for (FreeformFinSet* other : others())
        {
            if (const std::optional<std::size_t> otherIndex =
                    FreeformFinSet::getConfigListenerPointIdx(*fins, *other, index))
            {
                // Java ignores the listener's IllegalFinPointException
                [[maybe_unused]] const Result<void> ignored =
                    other->addPoint(*otherIndex, location);
            }
        }
        result = fins->addPoint(index, location);
    });
    return result;
}

Result<void> FinPointSelection::removePoint(std::size_t index) const
{
    FreeformFinSet* const fins = main();
    if (fins == nullptr)
    {
        return fail(ErrorCode::NOT_FOUND, "The fin set is no longer in the rocket");
    }
    // Java throws before its loop for these.
    if (index == 0 || index + 1 >= fins->getPointCount())
    {
        return fins->removePoint(index);
    }
    Result<void> result;
    inFreezeIf(*m_rocket, isMultiEdit(), [&] {
        for (FreeformFinSet* other : others())
        {
            if (const std::optional<std::size_t> otherIndex =
                    FreeformFinSet::getConfigListenerPointIdx(*fins, *other, index))
            {
                // Java ignores the listener's IllegalFinPointException
                [[maybe_unused]] const Result<void> ignored = other->removePoint(*otherIndex);
            }
        }
        result = fins->removePoint(index);
    });
    return result;
}

void FinPointSelection::setPoints(std::span<const Coordinate> points) const
{
    if (points.empty())
    {
        bug("A fin needs at least one point");
    }
    FreeformFinSet* const fins = main();
    if (fins == nullptr)
    {
        return;
    }
    inFreezeIf(*m_rocket, isMultiEdit(), [&] {
        for (FreeformFinSet* other : others())
        {
            other->setPoints(points);
        }
        fins->setPoints(points);
    });
}

}  // namespace QtRocket::Gui
