#pragma once

#include <memory>
#include <optional>

#include <QString>

#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/widgets/ValueSlider.h"

class QGridLayout;
class QLabel;

namespace QtRocket::Gui
{

class DoubleBinding;
class UnitSelector;
class UnitSpinBox;

/// The rows the configuration panels are made of, in OpenRocket's column layout (the MigLayout
/// columns "[][65lp::][30lp::][]" of its panels): a label in column 0, the spin box in column 1,
/// the unit selector in column 2 and the slider in column 3 of a QGridLayout. The widgets are made
/// with makeChild() as children of the grid's parent widget, which must exist (the layout is set
/// on its widget first), and are named after the property they edit (objectName, the Java property
/// name: "OverrideMass"), as the GUI's tests find them.
namespace ConfigFields
{

/// The widgets of a number field (Java: a JSpinner on the DoubleModel's spinner model, a
/// UnitSelector and a BasicSlider on one of its slider models); a part the row has not is null.
struct NumberWidgets
{
    QLabel*       label        = nullptr;
    UnitSpinBox*  spinBox      = nullptr;
    UnitSelector* unitSelector = nullptr;
    ValueSlider*  slider       = nullptr;
};

/// What a number row shows besides its spin box.
struct NumberRowOptions
{
    /// The slider's mapping; nullopt for no slider.
    std::optional<SliderMapping> slider = std::nullopt;
    /// The bindings the slider's bounds follow (ValueSlider::DynamicBounds).
    ValueSlider::DynamicBounds bounds{.min = nullptr, .max = nullptr};
    /// Whether the row has a unit selector (Java: no UnitSelector for a coefficient).
    bool unitSelector = true;
};

/// Adds a number row to @p row of @p grid: the label @p label (none when it is empty), the spin
/// box, the unit selector and the slider of @p binding, each named @p property.
/// @throws BugError when @p binding is null or @p grid has no parent widget
NumberWidgets addNumberRow(QGridLayout& grid, int row, const QString& label,
                           const std::shared_ptr<DoubleBinding>& binding, const QString& property,
                           const NumberRowOptions& options);

}  // namespace ConfigFields

}  // namespace QtRocket::Gui
