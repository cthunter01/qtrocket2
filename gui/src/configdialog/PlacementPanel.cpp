#include "QtRocket/gui/configdialog/PlacementPanel.h"

#include <memory>
#include <span>

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QString>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The selection of @p scope, which must be valid.
[[nodiscard]] ComponentSelection& validSelection(const BindingScope& scope)
{
    if (scope.selection() == nullptr || !scope.selection()->isValid())
    {
        bug("A configuration panel needs a valid selection");
    }
    return *scope.selection();
}

}  // namespace

EnumSource<AxialMethod> PlacementPanel::axialMethodSource()
{
    return EnumSource<AxialMethod>::of<RocketComponent>({
        .get = [](const RocketComponent& component) { return component.getAxialMethod(); },
        .set = [](RocketComponent&   component,
                  const AxialMethod& method) { component.setAxialMethod(method); },
    });
}

DoubleSource PlacementPanel::axialOffsetSource()
{
    return DoubleSource::of<RocketComponent>({
        .get = [](const RocketComponent& component) { return component.getAxialOffset(); },
        .set = [](RocketComponent& component,
                  const double&    offset) { component.setAxialOffset(offset); },
    });
}

DoubleSource PlacementPanel::parentLengthSource()
{
    return DoubleSource::of<RocketComponent>({
        .get =
            [](const RocketComponent& component) {
                const RocketComponent* parent = component.getParent();
                return parent != nullptr ? parent->getLength() : 0.0;
            },
    });
}

PlacementPanel::PlacementPanel(BindingScope& scope, QWidget* parent)
  //: PlacementPanel.title.Placement
  : QGroupBox(tr("Placement"), parent), m_grid(makeChild<QGridLayout>(this))
{
    const std::shared_ptr<ComponentSelection>& selection = scope.selection();
    if (validSelection(scope).getMain().getParent() == nullptr)
    {
        bug("The placement panel needs a component that has a parent");
    }

    //: PlacementPanel.lbl.PosRelativeTo
    m_grid->addWidget(makeChild<QLabel>(this, tr("Position relative to:")), 0, 0);
    const auto methodBinding = scope.make<EnumBinding<AxialMethod>>(
        selection, axialMethodSource(), std::span<const AxialMethod>(kAxialOffsetMethods));
    m_method = makeChild<EnumComboBox>(this, methodBinding);
    m_method->setObjectName(QStringLiteral("AxialMethod"));
    m_grid->addWidget(m_method, 0, 1, 1, 3);

    // The slider runs from minus to plus the parent's length, both following it (Java:
    // DoubleModel(parent, "Length", -1.0, UNITS_NONE) and DoubleModel(parent, "Length")).
    const UnitGroup& none = unitGroup(UnitGroupId::NONE);
    const auto minLength  = scope.make<DoubleBinding>(selection, parentLengthSource(), none,
                                                      DoubleBinding::Options{.multiplier = -1.0});
    const auto maxLength =
        scope.make<DoubleBinding>(selection, parentLengthSource(), none, DoubleBinding::Options{});
    const auto offset = scope.make<DoubleBinding>(
        selection, axialOffsetSource(), unitGroup(UnitGroupId::LENGTH), DoubleBinding::Options{});
    //: PlacementPanel.lbl.plus
    m_offset = ConfigFields::addNumberRow(
        *m_grid, 1, tr("plus"), offset, QStringLiteral("AxialOffset"),
        {.slider = SliderMapping::linear(minLength->value(), maxLength->value()),
         .bounds = {.min = minLength, .max = maxLength}});
    m_offset.label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_grid->setColumnStretch(3, 1);
}

int PlacementPanel::nextRow() const
{
    return m_grid->rowCount();
}

}  // namespace QtRocket::Gui
