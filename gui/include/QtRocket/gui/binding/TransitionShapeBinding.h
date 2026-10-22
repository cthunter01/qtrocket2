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
/// shape changed). Picking a shape sets it on every selected transition; picking the shape the
/// main component has already sets nothing, so that the others keep their shapes, shape
/// parameters and clipping (Java: the dialogs show the model in a plain JComboBox, whose
/// setSelectedItem() never calls the model for the item it shows; TransitionShapeModel itself has
/// no such check).
///
/// Deviations from OpenRocket:
/// - Java's model calls setShapeType() with the main component's shape when it is made
///   (TransitionShapeModel's constructor: setSelectedItem(previousType)), and the main's setter
///   forwards it to the other selected transitions: opening a multi-component nose cone or
///   transition dialog silently gives every other selected transition the main's shape (with that
///   shape's default parameter and clipping), without an event and before the dialog's undo
///   position, so that it cannot even be undone. Here making the binding sets nothing: a
///   deliberate difference, in the spirit of the user's decision U4 (OpenRocket's GUI bugs fixed).
/// - Java's model shows no shape and sets none for a component that is not a Transition; here
///   such a main component is a BugError (EnumSource: the main component must be of the source's
///   class), and the dialogs that show the combo box are those of a nose cone and a transition.
/// @throws BugError when @p selection is null or its main component is not a Transition
[[nodiscard]] std::shared_ptr<EnumBinding<TransitionShape>> makeTransitionShapeBinding(
    std::shared_ptr<ComponentSelection> selection);

}  // namespace QtRocket::Gui
