#include "QtRocket/gui/configdialog/MassComponentConfig.h"

#include <memory>
#include <span>
#include <utility>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/MassObjectFields.h"
#include "QtRocket/gui/configdialog/PlacementPanel.h"
#include "QtRocket/gui/configdialog/RadialPositionPanel.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using MassComponentType = MassComponent::MassComponentType;

/// The selection of @p panel, which must be valid and edit a mass component.
[[nodiscard]] const std::shared_ptr<ComponentSelection>& massSelection(
    const RocketComponentConfig& panel)
{
    const std::shared_ptr<ComponentSelection>& selection = panel.selection();
    if (selection == nullptr || !selection->isValid())
    {
        bug("The configuration of a mass component needs a valid selection");
    }
    if (dynamic_cast<const MassComponent*>(&selection->getMain()) == nullptr)
    {
        bug("The configuration of a mass component needs a mass component");
    }
    return selection;
}

}  // namespace

EnumSource<MassComponent::MassComponentType> MassComponentConfig::typeSource()
{
    return EnumSource<MassComponentType>::of<MassComponent>({
        .get = [](const MassComponent& mass) { return mass.getMassComponentType(); },
        .set = [](MassComponent&           mass,
                  const MassComponentType& type) { mass.setMassComponentType(type); },
    });
}

DoubleSource MassComponentConfig::massSource()
{
    return DoubleSource::of<MassComponent>({
        .get = [](const MassComponent& mass) { return mass.getComponentMass(); },
        .set = [](MassComponent& mass, const double& value) { mass.setComponentMass(value); },
    });
}

DoubleSource MassComponentConfig::densitySource()
{
    return DoubleSource::of<MassComponent>({
        .get = [](const MassComponent& mass) { return mass.getDensity(); },
        .set = [](MassComponent& mass, const double& density) { mass.setDensity(density); },
    });
}

MassComponentConfig::MassComponentConfig(OpenRocketDocument& document, const GuiServices& services,
                                         std::shared_ptr<ComponentSelection> selection,
                                         QWidget*                            parent)
  : RocketComponentConfig(document, services, std::move(selection), parent)
{
    const std::shared_ptr<ComponentSelection>& valid    = massSelection(*this);
    BindingScope&                              bindings = scope();

    auto* page    = makeChild<QWidget>(this);
    auto* primary = makeChild<QHBoxLayout>(page);
    auto* left    = makeChild<QWidget>(page);
    auto* grid    = makeChild<QGridLayout>(left);
    grid->setContentsMargins(0, 0, 0, 0);
    primary->addWidget(left, 0, Qt::AlignTop);

    // The type.
    //: MassComponentCfg.lbl.type
    grid->addWidget(makeChild<QLabel>(left, tr("Mass type:")), 0, 0);
    m_type = makeChild<EnumComboBox>(
        left, bindings.make<EnumBinding<MassComponentType>>(
                  valid, typeSource(),
                  std::span<const MassComponentType>(MassComponent::kAllMassComponentTypes)));
    m_type->setObjectName(QStringLiteral("MassComponentType"));
    grid->addWidget(m_type, 0, 1, 1, 3);

    // The mass and the density.
    m_mass = ConfigFields::addNumberRow(
        *grid, 1,
        //: MassComponentCfg.lbl.Mass
        tr("Mass:"),
        bindings.make<DoubleBinding>(valid, massSource(), unitGroup(UnitGroupId::MASS),
                                     DoubleBinding::Options{.min = 0.0}),
        QStringLiteral("ComponentMass"), {.slider = SliderMapping::fromRange(0, 0.05, 0.5)});
    m_density = ConfigFields::addNumberRow(
        *grid, 2,
        //: MassComponentCfg.lbl.Density
        tr("Approximate density:"),
        bindings.make<DoubleBinding>(valid, densitySource(), unitGroup(UnitGroupId::DENSITY_BULK),
                                     DoubleBinding::Options{.min = 0.0}),
        QStringLiteral("Density"), {.slider = SliderMapping::fromRange(500, 2000, 10000)});

    // The length, the diameter and its "Automatic".
    m_size = MassObjectFields::addSizeRows(bindings, *grid, 3,
                                           //: MassComponentCfg.lbl.Length
                                           tr("Length:"),
                                           //: MassComponentCfg.lbl.Diameter
                                           tr("Diameter:"));
    grid->setColumnStretch(3, 1);

    // The right side: the placement.
    auto* right       = makeChild<QWidget>(page);
    auto* rightLayout = makeChild<QVBoxLayout>(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    m_placement = makeChild<PlacementPanel>(right, bindings);
    rightLayout->addWidget(m_placement);
    rightLayout->addStretch(1);
    primary->addWidget(right, 0, Qt::AlignTop);
    primary->addStretch(1);

    insertTab(0, *page,
              //: MassComponentCfg.tab.General
              tr("General"),
              //: MassComponentCfg.tab.ttip.General
              tr("General properties"));
    RadialPositionPanel::addTab(*this);
    setSelectedTabIndex(0);
}

}  // namespace QtRocket::Gui
