#include "QtRocket/gui/configdialog/RadialPositionPanel.h"

#include <memory>
#include <numbers>

#include <QGridLayout>
#include <QPushButton>
#include <QString>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The selection of @p scope, which must be valid and edit a mass object.
[[nodiscard]] const std::shared_ptr<ComponentSelection>& massObjectSelection(
    const BindingScope& scope)
{
    const std::shared_ptr<ComponentSelection>& selection = scope.selection();
    if (selection == nullptr || !selection->isValid())
    {
        bug("The radial position tab needs a valid selection");
    }
    if (dynamic_cast<const MassObject*>(&selection->getMain()) == nullptr)
    {
        bug("The radial position tab needs a mass object");
    }
    return selection;
}

}  // namespace

DoubleSource RadialPositionPanel::radialPositionSource()
{
    return DoubleSource::of<MassObject>({
        .get = [](const MassObject& object) { return object.getRadialPosition(); },
        .set = [](MassObject&   object,
                  const double& position) { object.setRadialPosition(position); },
    });
}

DoubleSource RadialPositionPanel::radialDirectionSource()
{
    return DoubleSource::of<MassObject>({
        .get = [](const MassObject& object) { return object.getRadialDirection(); },
        .set = [](MassObject&   object,
                  const double& direction) { object.setRadialDirection(direction); },
    });
}

RadialPositionPanel::RadialPositionPanel(BindingScope& scope, QWidget* parent)
  : QWidget(parent), m_selection(massObjectSelection(scope)), m_grid(makeChild<QGridLayout>(this))
{
    // The distance from the parent's axis, at least 0.
    const auto position = scope.make<DoubleBinding>(m_selection, radialPositionSource(),
                                                    unitGroup(UnitGroupId::LENGTH),
                                                    DoubleBinding::Options{.min = 0.0});
    m_position          = ConfigFields::addNumberRow(
        *m_grid, 0,
        //: MassComponentCfg.lbl.Radialdistance (ShockCordCfg., ParachuteCfg., StreamerCfg.)
        tr("Radial distance:"), position, QStringLiteral("RadialPosition"),
        {.slider = SliderMapping::fromRange(0, 0.1, 1.0)});

    // The direction, any angle (the core reduces it to -pi ... pi).
    const auto direction =
        scope.make<DoubleBinding>(m_selection, radialDirectionSource(),
                                  unitGroup(UnitGroupId::ANGLE), DoubleBinding::Options{});
    m_direction = ConfigFields::addNumberRow(
        *m_grid, 1,
        //: MassComponentCfg.lbl.Radialdirection (ShockCordCfg., ParachuteCfg., StreamerCfg.)
        tr("Radial direction:"), direction, QStringLiteral("RadialDirection"),
        {.slider = SliderMapping::linear(-std::numbers::pi, std::numbers::pi)});

    //: MassComponentCfg.but.Reset (ShockCordCfg., ParachuteCfg., StreamerCfg.)
    m_reset = makeChild<QPushButton>(this, tr("Reset"));
    m_reset->setObjectName(QStringLiteral("resetRadialButton"));
    connect(m_reset, &QPushButton::clicked, this,
            guarded([this] { resetRadialPosition(); }, QStringLiteral("resetRadialButton")));
    m_grid->addWidget(m_reset, 2, 0, 1, 4, Qt::AlignRight);
    m_grid->setColumnStretch(3, 1);
    m_grid->setRowStretch(3, 1);
}

RadialPositionPanel& RadialPositionPanel::addTab(RocketComponentConfig& panel)
{
    auto* tab = makeChild<RadialPositionPanel>(&panel, panel.scope());
    panel.insertTab(1, *tab,
                    //: MassComponentCfg.tab.Radialpos (ShockCordCfg., ParachuteCfg., StreamerCfg.)
                    tr("Radial position"),
                    //: MassComponentCfg.tab.ttip.Radialpos (ShockCordCfg., ParachuteCfg.,
                    //: StreamerCfg.)
                    tr("Radial position configuration"));
    return *tab;
}

RadialPositionPanel& RadialPositionPanel::of(const RocketComponentConfig& panel)
{
    auto* tab = panel.findChild<RadialPositionPanel*>();
    if (tab == nullptr)
    {
        bug("The configuration has no radial position tab");
    }
    return *tab;
}

void RadialPositionPanel::resetRadialPosition()
{
    if (!m_selection->isValid())
    {
        return;
    }
    m_selection->apply<MassObject>([](MassObject& object) {
        object.setRadialDirection(0.0);
        object.setRadialPosition(0.0);
    });
}

}  // namespace QtRocket::Gui
