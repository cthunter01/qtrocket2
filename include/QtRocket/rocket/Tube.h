#pragma once

#include "QtRocket/rocket/Coaxial.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"

namespace QtRocket
{

/// An external component shaped as a tube (OpenRocket's Tube): the abstract base of launch lugs
/// and tube fin sets, an ExternalComponent that is Coaxial. (A body tube is a SymmetricComponent
/// that is Coaxial on its own, as in OpenRocket.)
class Tube : public ExternalComponent, public virtual Coaxial
{
protected:
    /// A tube positioned by @p relativePosition.
    explicit Tube(AxialMethod relativePosition) : ExternalComponent(relativePosition) { }
};

}  // namespace QtRocket
