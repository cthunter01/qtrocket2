#include "QtRocket/rocket/InsideColorComponentHandler.h"

#include <optional>
#include <utility>

#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

InsideColorComponentHandler::InsideColorComponentHandler(InsideColorComponent& owner) noexcept
  : m_owner(&owner)
{
}

void InsideColorComponentHandler::setInsideAppearance(std::optional<Appearance> appearance)
{
    m_insideAppearance = std::move(appearance);
    component().fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

void InsideColorComponentHandler::setEdgesSameAsInside(bool newState)
{
    if (m_edgesSameAsInside == newState)
    {
        return;
    }
    m_edgesSameAsInside = newState;
    component().fireComponentChangeEvent(ComponentChangeEvent::kGraphicChange);
}

void InsideColorComponentHandler::setSeparateInsideOutside(bool newState)
{
    if (m_separateInsideOutside == newState)
    {
        return;
    }
    m_separateInsideOutside = newState;
    component().fireComponentChangeEvent(ComponentChangeEvent::kGraphicChange);
}

void InsideColorComponentHandler::copyFrom(const InsideColorComponentHandler& source)
{
    m_insideAppearance      = source.m_insideAppearance;
    m_separateInsideOutside = source.m_separateInsideOutside;
    m_edgesSameAsInside     = source.m_edgesSameAsInside;
}

RocketComponent& InsideColorComponentHandler::component() const
{
    // A cross cast from the mixin to the component class of the same object; it is done at the
    // time of use, since during construction the object is not yet a RocketComponent.
    auto* rocketComponent = dynamic_cast<RocketComponent*>(m_owner);
    if (rocketComponent == nullptr)
    {
        bug("an InsideColorComponent must also be a RocketComponent");
    }
    return *rocketComponent;
}

}  // namespace QtRocket
