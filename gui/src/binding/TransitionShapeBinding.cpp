#include "QtRocket/gui/binding/TransitionShapeBinding.h"

#include <memory>
#include <utility>

#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"

namespace QtRocket::Gui
{

EnumSource<TransitionShape> transitionShapeSource()
{
    return EnumSource<TransitionShape>::of<Transition>({
        .get = [](const Transition& transition) { return transition.getShapeType(); },
        .set = [](Transition&     transition,
                  TransitionShape shape) { transition.setShapeType(shape); },
    });
}

std::shared_ptr<EnumBinding<TransitionShape>> makeTransitionShapeBinding(
    std::shared_ptr<ComponentSelection> selection)
{
    return std::make_shared<EnumBinding<TransitionShape>>(
        std::move(selection), transitionShapeSource(), kAllTransitionShapes,
        EnumBinding<TransitionShape>::SameValue::SET);
}

}  // namespace QtRocket::Gui
