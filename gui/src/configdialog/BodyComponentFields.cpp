#include "QtRocket/gui/configdialog/BodyComponentFields.h"

#include <memory>

#include <QGridLayout>
#include <QLabel>
#include <QString>

#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/FieldEnabling.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui::BodyComponentFields
{

namespace
{

/// The step of the shape parameter's spin box (Java: getSpinnerModel(0.1)).
constexpr double kShapeParameterStep = 0.1;

/// The valid selection of @p scope, whose main component is a C.
/// @throws BugError when it is none
template <class C>
[[nodiscard]] const std::shared_ptr<ComponentSelection>& selectionOf(const BindingScope& scope,
                                                                     const char*         what)
{
    const std::shared_ptr<ComponentSelection>& selection = scope.selection();
    if (selection == nullptr || !selection->isValid() ||
        dynamic_cast<const C*>(&selection->getMain()) == nullptr)
    {
        bug(what);
    }
    return selection;
}

}  // namespace

DoubleSource lengthSource()
{
    return DoubleSource::of<SymmetricComponent>({
        .get = [](const SymmetricComponent& component) { return component.getLength(); },
        .set = [](SymmetricComponent& component,
                  const double&       length) { component.setLength(length); },
    });
}

DoubleSource thicknessSource()
{
    return DoubleSource::of<SymmetricComponent>({
        .get = [](const SymmetricComponent& component) { return component.getThickness(); },
        .set = [](SymmetricComponent& component,
                  const double&       thickness) { component.setThickness(thickness); },
    });
}

DoubleSource maxRadiusSource()
{
    return DoubleSource::of<SymmetricComponent>({
        .get = [](const SymmetricComponent& component) { return component.getMaxRadius(); },
    });
}

BoolSource filledSource()
{
    return BoolSource::of<SymmetricComponent>({
        .get = [](const SymmetricComponent& component) { return component.isFilled(); },
        .set = [](SymmetricComponent& component,
                  const bool&         filled) { component.setFilled(filled); },
    });
}

DoubleSource shapeParameterSource()
{
    return DoubleSource::of<Transition>({
        .get = [](const Transition& transition) { return transition.getShapeParameter(); },
        .set = [](Transition&   transition,
                  const double& parameter) { transition.setShapeParameter(parameter); },
    });
}

DoubleSource shapeParameterMinSource()
{
    return DoubleSource::of<Transition>({
        .get = [](const Transition& transition) { return transition.getShapeParameterMin(); },
    });
}

DoubleSource shapeParameterMaxSource()
{
    return DoubleSource::of<Transition>({
        .get = [](const Transition& transition) { return transition.getShapeParameterMax(); },
    });
}

ConfigFields::NumberWidgets addShapeParameterRow(BindingScope& scope, QGridLayout& grid, int row,
                                                 const QString& label)
{
    const std::shared_ptr<ComponentSelection>& selection =
        selectionOf<Transition>(scope, "The shape parameter needs a transition or a nose cone");
    const auto parameter = scope.make<DoubleBinding>(
        selection, shapeParameterSource(), unitGroup(UnitGroupId::SHAPE_PARAMETER),
        DoubleBinding::Options{.min = 0.0, .max = 1.0});
    const UnitGroup& none    = unitGroup(UnitGroupId::NONE);
    const auto       minimum = scope.make<DoubleBinding>(selection, shapeParameterMinSource(), none,
                                                         DoubleBinding::Options{});
    const auto       maximum = scope.make<DoubleBinding>(selection, shapeParameterMaxSource(), none,
                                                         DoubleBinding::Options{});
    ConfigFields::NumberWidgets widgets = ConfigFields::addNumberRow(
        grid, row, label, parameter, QStringLiteral("ShapeParameter"),
        {.slider       = SliderMapping::linear(minimum->value(), maximum->value()),
         .bounds       = {.min = minimum, .max = maximum},
         .unitSelector = false});
    widgets.spinBox->setFixedStep(kShapeParameterStep);
    return widgets;
}

void setShapeParameterEnabled(const ConfigFields::NumberWidgets& widgets, bool enabled)
{
    if (widgets.label != nullptr)
    {
        widgets.label->setEnabled(enabled);
    }
    if (widgets.spinBox != nullptr)
    {
        FieldEnabling::setFieldEnabled(*widgets.spinBox, enabled);
    }
    if (widgets.slider != nullptr)
    {
        FieldEnabling::setFieldEnabled(*widgets.slider, enabled);
    }
}

ConfigFields::NumberWidgets addThicknessRow(BindingScope& scope, QGridLayout& grid, int row,
                                            const QString& label)
{
    const std::shared_ptr<ComponentSelection>& selection =
        selectionOf<SymmetricComponent>(scope, "The wall thickness needs a body component");
    const UnitGroup& length    = unitGroup(UnitGroupId::LENGTH);
    const auto       thickness = scope.make<DoubleBinding>(selection, thicknessSource(), length,
                                                           DoubleBinding::Options{.min = 0.0});
    const auto       maxRadius =
        scope.make<DoubleBinding>(selection, maxRadiusSource(), length, DoubleBinding::Options{});
    return ConfigFields::addNumberRow(grid, row, label, thickness, QStringLiteral("Thickness"),
                                      {.slider = SliderMapping::linear(0.0, maxRadius->value()),
                                       .bounds = {.min = nullptr, .max = maxRadius}});
}

void setAutomaticState(BoolCheckBox& automatic, bool enabled, const QString& toolTip)
{
    FieldEnabling::setFieldEnabled(automatic, enabled);
    automatic.setToolTip(toolTip);
}

}  // namespace QtRocket::Gui::BodyComponentFields
