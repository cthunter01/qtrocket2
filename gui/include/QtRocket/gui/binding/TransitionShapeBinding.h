#pragma once

#include <memory>

#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/rocket/TransitionShape.h"

namespace QtRocket::Gui
{

/// The source of a transition's (or nose cone's) shape: Transition::getShapeType() and
/// setShapeType().
[[nodiscard]] EnumSource<TransitionShape> transitionShapeSource();

/// The binding of the shape combo box of the nose cone and transition dialogs (Java:
/// TransitionShapeModel, swing gui/adaptors/TransitionShapeModel.java): every shape, in Java's
/// order (kAllTransitionShapes, Shape.values()), the main component's shape shown and re-read on
/// every event of the rocket (Java: on every change of the component, telling its combo when the
/// shape changed). Picking a shape sets it on every selected transition, also the shape the main
/// component has already (EnumBinding::SameValue::SET: TransitionShapeModel calls setShapeType()
/// every time, and the main's setter forwards it to the others).
///
/// Deviation from OpenRocket: Java's model shows no shape and sets none for a component that is
/// not a Transition; here such a main component is a BugError (EnumSource: the main component
/// must be of the source's class), and the dialogs that show the combo box are those of a nose
/// cone and a transition.
/// @throws BugError when @p selection is null or its main component is not a Transition
[[nodiscard]] std::shared_ptr<EnumBinding<TransitionShape>> makeTransitionShapeBinding(
    std::shared_ptr<ComponentSelection> selection);

}  // namespace QtRocket::Gui
