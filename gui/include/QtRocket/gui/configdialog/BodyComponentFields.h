#pragma once

#include <QString>

#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"

class QGridLayout;

namespace QtRocket::Gui
{

class BindingScope;
class BoolCheckBox;

/// What the panels of the body components share (OpenRocket's BodyTubeConfig, NoseConeConfig and
/// TransitionConfig, swing gui/configdialog/, each make these fields themselves on the same
/// models): the sources of the properties all three bind, the shape parameter's row of the nose
/// cone and the transition, the wall thickness row whose slider ends at the largest radius, and
/// the state of an "Automatic" check box of a radius.
namespace BodyComponentFields
{

/// The length of a body component (Java: DoubleModel(component, "Length", UNITS_LENGTH, 0)):
/// SymmetricComponent::getLength() and setLength(), which Transition overrides.
[[nodiscard]] DoubleSource lengthSource();

/// The wall thickness (Java: DoubleModel(component, "Thickness", ...)): getThickness() and the
/// one-argument setThickness(), which clamps to the largest radius.
[[nodiscard]] DoubleSource thicknessSource();

/// The largest radius, read-only: the maximum of the wall thickness slider of a nose cone and a
/// transition (Java: DoubleModel(component, "MaxRadius", UNITS_LENGTH)).
[[nodiscard]] DoubleSource maxRadiusSource();

/// Whether the component is solid (Java: BooleanModel(component, "Filled")).
[[nodiscard]] BoolSource filledSource();

/// A transition's shape parameter (Java: DoubleModel(component, "ShapeParameter",
/// UNITS_SHAPE_PARAMETER, 0, 1)) and its range for the shape, read-only (Java: DoubleModel(
/// component, "ShapeParameterMin") and "ShapeParameterMax", the slider's bounds).
[[nodiscard]] DoubleSource shapeParameterSource();
[[nodiscard]] DoubleSource shapeParameterMinSource();
[[nodiscard]] DoubleSource shapeParameterMaxSource();

/// Adds the shape parameter row of a nose cone or a transition to @p row of @p grid (Java:
/// NoseConeConfig and TransitionConfig): the label @p label, a spin box stepping by 0.1
/// (getSpinnerModel(0.1)) between 0 and 1, no unit selector, and a slider from the shape's
/// smallest to its largest parameter, which follow the shape (getSliderModel(min, max)). The
/// widgets are named "ShapeParameter".
/// @throws BugError when the scope has no valid selection or its main component is not a
///         Transition
ConfigFields::NumberWidgets addShapeParameterRow(BindingScope& scope, QGridLayout& grid, int row,
                                                 const QString& label);

/// Enables or disables the label, the spin box and the slider of the shape parameter's row
/// @p widgets (Java: updateEnabled(), whether the shape uses the parameter).
void setShapeParameterEnabled(const ConfigFields::NumberWidgets& widgets, bool enabled);

/// Adds the wall thickness row of a nose cone or a transition to @p row of @p grid: at least 0,
/// its slider from 0 to the largest radius, which it follows (Java: getSliderModel(0, new
/// DoubleModel(component, "MaxRadius", UNITS_LENGTH))). The widgets are named "Thickness".
/// @throws BugError when the scope has no valid selection or its main component is not a
///         SymmetricComponent
ConfigFields::NumberWidgets addThicknessRow(BindingScope& scope, QGridLayout& grid, int row,
                                            const QString& label);

/// Puts the "Automatic" check box @p automatic of a radius into the state its panel decided
/// (Java: checkAuto...Radius.setEnabled() and setToolTipText(), once the panel has looked at the
/// neighbours the radius would come from): enabled or not, with the tooltip @p toolTip.
void setAutomaticState(BoolCheckBox& automatic, bool enabled, const QString& toolTip);

}  // namespace BodyComponentFields

}  // namespace QtRocket::Gui
