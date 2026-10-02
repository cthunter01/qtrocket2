#include "QtRocket/rocket/EngineBlock.h"

#include <memory>

#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket
{

EngineBlock::EngineBlock()
{
    setOuterRadiusAutomatic(true);
    setThickness(0.005);
    setLength(0.005);
    setDisplayOrderSide(9);   // Order for displaying the component in the 2D side view
    setDisplayOrderBack(15);  // Order for displaying the component in the 2D back view
}

std::unique_ptr<RocketComponent> EngineBlock::cloneShallow() const
{
    return std::make_unique<EngineBlock>(*this);
}

bool EngineBlock::allowsChildren() const
{
    return false;
}

bool EngineBlock::isCompatible(ComponentKind /*kind*/) const
{
    return false;
}

}  // namespace QtRocket
