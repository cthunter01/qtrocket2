#include "QtRocket/gui/configdialog/RingComponentConfig.h"

#include <memory>
#include <utility>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/configdialog/PlacementPanel.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/Bulkhead.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/RingComponent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TubeCoupler.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// Sets the automatic outer radius of @p ring when its class makes the setter public (Java's
/// listener loop in RingComponent.setOuterRadiusAutomatic() reaches each selected ring through
/// its own class's override).
void setOuterRadiusAutomaticOf(RingComponent& ring, bool automatic)
{
    if (auto* coupler = dynamic_cast<TubeCoupler*>(&ring))
    {
        coupler->setOuterRadiusAutomatic(automatic);
    }
    else if (auto* block = dynamic_cast<EngineBlock*>(&ring))
    {
        block->setOuterRadiusAutomatic(automatic);
    }
    else if (auto* centering = dynamic_cast<CenteringRing*>(&ring))
    {
        centering->setOuterRadiusAutomatic(automatic);
    }
    else if (auto* bulkhead = dynamic_cast<Bulkhead*>(&ring))
    {
        bulkhead->setOuterRadiusAutomatic(automatic);
    }
}

/// The selection of @p panel, which must be valid and edit a ring component.
[[nodiscard]] const std::shared_ptr<ComponentSelection>& ringSelection(
    const RocketComponentConfig& panel)
{
    const std::shared_ptr<ComponentSelection>& selection = panel.selection();
    if (selection == nullptr || !selection->isValid())
    {
        bug("The General tab of a ring component needs a valid selection");
    }
    if (dynamic_cast<const RingComponent*>(&selection->getMain()) == nullptr)
    {
        bug("The General tab of a ring component needs a ring component");
    }
    return selection;
}

/// Adds the "Automatic" check box of @p binding to @p row of @p grid (Java: "skip, spanx 2").
[[nodiscard]] BoolCheckBox* addAutomaticRow(QGridLayout& grid, int row,
                                            const std::shared_ptr<DoubleBinding>& binding,
                                            const QString& property, const QString& toolTip)
{
    //: ringcompcfg.Automatic
    auto* check =
        makeChild<BoolCheckBox>(grid.parentWidget(), binding, RingComponentConfig::tr("Automatic"));
    check->setObjectName(property + QStringLiteral("Automatic"));
    check->setToolTip(toolTip);
    grid.addWidget(check, row, 1, 1, 2);
    return check;
}

}  // namespace

RingComponentConfig::RingComponentConfig(OpenRocketDocument& document, const GuiServices& services,
                                         std::shared_ptr<ComponentSelection> selection,
                                         QWidget*                            parent)
  : RocketComponentConfig(document, services, std::move(selection), parent)
{
}

bool RingComponentConfig::hasAutomaticOuterRadius(const RocketComponent& component) noexcept
{
    return dynamic_cast<const TubeCoupler*>(&component) != nullptr ||
           dynamic_cast<const EngineBlock*>(&component) != nullptr ||
           dynamic_cast<const CenteringRing*>(&component) != nullptr ||
           dynamic_cast<const Bulkhead*>(&component) != nullptr;
}

bool RingComponentConfig::hasAutomaticInnerRadius(const RocketComponent& component) noexcept
{
    return dynamic_cast<const CenteringRing*>(&component) != nullptr;
}

DoubleSource RingComponentConfig::lengthSource()
{
    return DoubleSource::of<RingComponent>({
        .get = [](const RingComponent& ring) { return ring.getLength(); },
        .set = [](RingComponent& ring, const double& length) { ring.setLength(length); },
    });
}

DoubleSource RingComponentConfig::outerRadiusSource(const RocketComponent& main)
{
    DoubleSource::Accessors<RingComponent> accessors{
        .get = [](const RingComponent& ring) { return ring.getOuterRadius(); },
        .set = [](RingComponent& ring, const double& radius) { ring.setOuterRadius(radius); },
    };
    if (hasAutomaticOuterRadius(main))
    {
        accessors.isAutomatic = [](const RingComponent& ring) {
            return ring.isOuterRadiusAutomatic();
        };
        accessors.setAutomatic = [](RingComponent& ring, bool automatic) {
            setOuterRadiusAutomaticOf(ring, automatic);
        };
    }
    return DoubleSource::of<RingComponent>(std::move(accessors));
}

DoubleSource RingComponentConfig::innerRadiusSource(const RocketComponent& main)
{
    DoubleSource::Accessors<RingComponent> accessors{
        .get = [](const RingComponent& ring) { return ring.getInnerRadius(); },
        .set = [](RingComponent& ring, const double& radius) { ring.setInnerRadius(radius); },
    };
    if (hasAutomaticInnerRadius(main))
    {
        accessors.isAutomatic = [](const RingComponent& ring) {
            return ring.isInnerRadiusAutomatic();
        };
        accessors.setAutomatic = [](RingComponent& ring, bool automatic) {
            if (auto* centering = dynamic_cast<CenteringRing*>(&ring))
            {
                centering->setInnerRadiusAutomatic(automatic);
            }
        };
    }
    return DoubleSource::of<RingComponent>(std::move(accessors));
}

DoubleSource RingComponentConfig::thicknessSource()
{
    return DoubleSource::of<RingComponent>({
        .get = [](const RingComponent& ring) { return ring.getThickness(); },
        .set = [](RingComponent& ring, const double& thickness) { ring.setThickness(thickness); },
    });
}

RingComponentConfig::GeneralTab RingComponentConfig::makeGeneralTab(RocketComponentConfig& panel,
                                                                    const GeneralTabTexts& texts)
{
    const std::shared_ptr<ComponentSelection>& selection = ringSelection(panel);
    const RocketComponent&                     main      = selection->getMain();
    BindingScope&                              scope     = panel.scope();
    const UnitGroup&                           length    = unitGroup(UnitGroupId::LENGTH);

    GeneralTab tab;
    tab.page      = makeChild<QWidget>(&panel);
    auto* primary = makeChild<QHBoxLayout>(tab.page);
    auto* left    = makeChild<QWidget>(tab.page);
    auto* grid    = makeChild<QGridLayout>(left);
    grid->setContentsMargins(0, 0, 0, 0);
    primary->addWidget(left, 0, Qt::AlignTop);
    int row = 0;

    // The length.
    if (!texts.length.isEmpty())
    {
        const auto binding = scope.make<DoubleBinding>(selection, lengthSource(), length,
                                                       DoubleBinding::Options{.min = 0.0});
        tab.length = ConfigFields::addNumberRow(*grid, row++, texts.length, binding,
                                                QStringLiteral("Length"),
                                                {.slider = SliderMapping::fromRange(0, 0.1, 1.0)});
    }

    // The outer diameter.
    std::shared_ptr<DoubleBinding> outer;
    if (!texts.outer.isEmpty())
    {
        outer = scope.make<DoubleBinding>(selection, outerRadiusSource(main), length,
                                          DoubleBinding::Options{.multiplier = 2.0, .min = 0.0});
        tab.outerRadius = ConfigFields::addNumberRow(
            *grid, row++, texts.outer, outer, QStringLiteral("OuterRadius"),
            {.slider = SliderMapping::fromRange(0, 0.04, 0.2)});
        if (outer->isAutomaticAvailable())
        {
            tab.outerRadiusAutomatic = addAutomaticRow(
                *grid, row++, outer, QStringLiteral("OuterRadius"),
                //: ringcompcfg.AutomaticOuter.ttip
                tr("Matches outer diameter to the inner diameter of the parent component."));
        }
    }

    // The inner diameter: its slider ends at the outer diameter (Java: getSliderModel(new
    // DoubleModel(0), od)).
    if (!texts.inner.isEmpty())
    {
        const auto inner =
            scope.make<DoubleBinding>(selection, innerRadiusSource(main), length,
                                      DoubleBinding::Options{.multiplier = 2.0, .min = 0.0});
        ConfigFields::NumberRowOptions options{.slider = SliderMapping::fromRange(0, 0.04, 0.2)};
        if (outer != nullptr)
        {
            options.slider = SliderMapping::linear(0.0, outer->value());
            options.bounds = ValueSlider::DynamicBounds{.min = nullptr, .max = outer};
        }
        tab.innerRadius = ConfigFields::addNumberRow(*grid, row++, texts.inner, inner,
                                                     QStringLiteral("InnerRadius"), options);
        if (inner->isAutomaticAvailable())
        {
            tab.innerRadiusAutomatic = addAutomaticRow(
                *grid, row++, inner, QStringLiteral("InnerRadius"),
                //: ringcompcfg.AutomaticInner.ttip
                tr("Matches inner diameter to the outer diameter of the child inner tube."));
        }
    }

    // The wall thickness.
    if (!texts.thickness.isEmpty())
    {
        const auto binding = scope.make<DoubleBinding>(selection, thicknessSource(), length,
                                                       DoubleBinding::Options{.min = 0.0});
        tab.thickness      = ConfigFields::addNumberRow(*grid, row, texts.thickness, binding,
                                                        QStringLiteral("Thickness"),
                                                        {.slider = SliderMapping::linear(0, 0.01)});
    }
    grid->setColumnStretch(3, 1);

    // The right side: the placement and the bulk material.
    auto* right       = makeChild<QWidget>(tab.page);
    auto* rightLayout = makeChild<QVBoxLayout>(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    tab.placement = makeChild<PlacementPanel>(right, scope);
    rightLayout->addWidget(tab.placement);
    tab.material = makeChild<MaterialPanel>(right, scope, panel.document(),
                                            panel.services().materials(), Material::Type::BULK);
    rightLayout->addWidget(tab.material);
    rightLayout->addStretch(1);
    primary->addWidget(right, 0, Qt::AlignTop);
    primary->addStretch(1);
    return tab;
}

void RingComponentConfig::addGeneralTab(const GeneralTabTexts& texts, const QString& title,
                                        const QString& toolTip)
{
    m_general = makeGeneralTab(*this, texts);
    insertTab(0, *m_general.page, title, toolTip);
    setSelectedTabIndex(0);
}

}  // namespace QtRocket::Gui
