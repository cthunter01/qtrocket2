#include "QtRocket/rocket/InsideColorComponent.h"

#include "QtRocket/rocket/InsideColorComponentHandler.h"

namespace QtRocket
{

InsideColorComponent::InsideColorComponent() noexcept : m_handler(*this) { }

InsideColorComponent::InsideColorComponent(const InsideColorComponent& other) : m_handler(*this)
{
    m_handler.copyFrom(other.m_handler);
}

InsideColorComponent::~InsideColorComponent() = default;

void InsideColorComponent::setInsideColorComponentHandler(
    const InsideColorComponentHandler& handler)
{
    m_handler.copyFrom(handler);
}

}  // namespace QtRocket
