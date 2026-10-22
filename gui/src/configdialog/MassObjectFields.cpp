#include "QtRocket/gui/configdialog/MassObjectFields.h"

#include <memory>

#include <QGridLayout>
#include <QPointer>
#include <QString>

#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

DoubleSource MassObjectFields::lengthSource()
{
    return DoubleSource::of<MassObject>({
        .get = [](const MassObject& object) { return object.getLength(); },
        .set = [](MassObject& object, const double& length) { object.setLength(length); },
    });
}

DoubleSource MassObjectFields::radiusSource()
{
    return DoubleSource::of<MassObject>({
        .get          = [](const MassObject& object) { return object.getRadius(); },
        .set          = [](MassObject& object, const double& radius) { object.setRadius(radius); },
        .isAutomatic  = [](const MassObject& object) { return object.isRadiusAutomatic(); },
        .setAutomatic = [](MassObject& object,
                           bool        automatic) { object.setRadiusAutomatic(automatic); },
    });
}

bool MassObjectFields::isAutomaticRadiusEnabled(const RocketComponent& component)
{
    const auto* object = dynamic_cast<const MassObject*>(&component);
    return object != nullptr && object->getMaxParentRadius() > 0;
}

MassObjectFields::SizeWidgets MassObjectFields::addSizeRows(BindingScope& scope, QGridLayout& grid,
                                                            int row, const QString& lengthLabel,
                                                            const QString& diameterLabel)
{
    const std::shared_ptr<ComponentSelection>& selection = scope.selection();
    if (selection == nullptr || !selection->isValid())
    {
        bug("The size of a mass object needs a valid selection");
    }
    if (dynamic_cast<const MassObject*>(&selection->getMain()) == nullptr)
    {
        bug("The size of a mass object needs a mass object");
    }
    const UnitGroup& length = unitGroup(UnitGroupId::LENGTH);
    SizeWidgets      widgets;

    const auto lengthBinding = scope.make<DoubleBinding>(selection, lengthSource(), length,
                                                         DoubleBinding::Options{.min = 0.0});
    widgets.length =
        ConfigFields::addNumberRow(grid, row, lengthLabel, lengthBinding, QStringLiteral("Length"),
                                   {.slider = SliderMapping::fromRange(0, 0.1, 0.5)});

    const auto diameter = scope.make<DoubleBinding>(
        selection, radiusSource(), length, DoubleBinding::Options{.multiplier = 2.0, .min = 0.0});
    widgets.diameter =
        ConfigFields::addNumberRow(grid, row + 1, diameterLabel, diameter, QStringLiteral("Radius"),
                                   {.slider = SliderMapping::fromRange(0, 0.04, 0.2)});

    //: ParachuteCfg.checkbox.AutomaticPacked
    widgets.automatic = makeChild<BoolCheckBox>(grid.parentWidget(), diameter, tr("Automatic"));
    widgets.automatic->setObjectName(QStringLiteral("RadiusAutomatic"));
    //: ParachuteCfg.checkbox.AutomaticPacked.ttip
    widgets.automatic->setToolTip(
        tr("Matches the packed diameter to the inside diameter of the parent component."));
    grid.addWidget(widgets.automatic, row + 2, 1, 1, 2);

    // Enabled while the main component's parent offers a radius, decided again on every change
    // of the rocket.
    widgets.automatic->setFieldEnabled(isAutomaticRadiusEnabled(selection->getMain()));
    ComponentSelection* const    watched = selection.get();
    const QPointer<BoolCheckBox> box     = widgets.automatic;
    scope.keepConnection(watched->addListener([watched, box](ComponentSelection::Phase phase) {
        if (phase != ComponentSelection::Phase::NOTIFY || box.isNull())
        {
            return;
        }
        Guard::run(
            [watched, &box] { box->setFieldEnabled(isAutomaticRadiusEnabled(watched->getMain())); },
            QStringLiteral("RadiusAutomatic"));
    }));
    return widgets;
}

}  // namespace QtRocket::Gui
