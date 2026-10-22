#include "QtRocket/gui/configdialog/LaunchLugConfig.h"

#include <memory>
#include <numbers>
#include <utility>

#include <QGridLayout>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/InstancesPanel.h"
#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/configdialog/PlacementPanel.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The length slider: 0, 2 cm at the middle, 10 cm (Java: getSliderModel(0, 0.02, 0.1)).
constexpr double kLengthSliderMid = 0.02;
constexpr double kLengthSliderMax = 0.1;
/// The diameter slider: 0, 4 cm, 20 cm (Java: getSliderModel(0, 0.04, 0.2)).
constexpr double kDiameterSliderMid = 0.04;
constexpr double kDiameterSliderMax = 0.2;
/// The thickness slider: 0 to 1 cm (Java: getSliderModel(0, 0.01)).
constexpr double kThicknessSliderMax = 0.01;
/// The gap before the right column (Java: "gapright 20lp").
constexpr int kColumnGap = 20;

/// The rows of the General tab, and the row below them, which takes the extra height.
constexpr int kLengthRow        = 0;
constexpr int kOuterDiameterRow = 1;
constexpr int kInnerDiameterRow = 2;
constexpr int kThicknessRow     = 3;
constexpr int kInstancesRow     = 4;
constexpr int kRowCount         = 5;

/// The columns: ConfigFields' four, a gap, the right column.
constexpr int kSliderColumn = 3;
constexpr int kColumnCount  = 4;
constexpr int kGapColumn    = 4;
constexpr int kRightColumn  = 5;

/// @p selection, whose main component must be a LaunchLug.
/// @throws BugError when it is not one (or the selection is null or invalid)
[[nodiscard]] std::shared_ptr<ComponentSelection> launchLugSelection(
    std::shared_ptr<ComponentSelection> selection)
{
    if (selection != nullptr && selection->isValid() &&
        dynamic_cast<const LaunchLug*>(&selection->getMain()) == nullptr)
    {
        bug("The launch lug configuration needs a launch lug");
    }
    return selection;  // a null or invalid one is RocketComponentConfig's BugError
}

/// The length (Java: DoubleModel(component, "Length", UNITS_LENGTH, 0)).
[[nodiscard]] DoubleSource lengthSource()
{
    return DoubleSource::of<LaunchLug>({
        .get = [](const LaunchLug& lug) { return lug.getLength(); },
        .set = [](LaunchLug& lug, const double& length) { lug.setLength(length); },
    });
}

/// The outer radius (Java: DoubleModel(component, "OuterRadius", 2, UNITS_LENGTH, 0)).
[[nodiscard]] DoubleSource outerRadiusSource()
{
    return DoubleSource::of<LaunchLug>({
        .get = [](const LaunchLug& lug) { return lug.getOuterRadius(); },
        .set = [](LaunchLug& lug, const double& radius) { lug.setOuterRadius(radius); },
    });
}

/// The inner radius (Java: DoubleModel(component, "InnerRadius", 2, UNITS_LENGTH, 0)).
[[nodiscard]] DoubleSource innerRadiusSource()
{
    return DoubleSource::of<LaunchLug>({
        .get = [](const LaunchLug& lug) { return lug.getInnerRadius(); },
        .set = [](LaunchLug& lug, const double& radius) { lug.setInnerRadius(radius); },
    });
}

/// The wall thickness (Java: DoubleModel(component, "Thickness", UNITS_LENGTH, 0)).
[[nodiscard]] DoubleSource thicknessSource()
{
    return DoubleSource::of<LaunchLug>({
        .get = [](const LaunchLug& lug) { return lug.getThickness(); },
        .set = [](LaunchLug& lug, const double& thickness) { lug.setThickness(thickness); },
    });
}

/// The angle around the parent's axis (Java: DoubleModel(component, "AngleOffset",
/// UNITS_ANGLE, -Math.PI, Math.PI)).
[[nodiscard]] DoubleSource angleOffsetSource()
{
    return DoubleSource::of<LaunchLug>({
        .get = [](const LaunchLug& lug) { return lug.getAngleOffset(); },
        .set = [](LaunchLug& lug, const double& angle) { lug.setAngleOffset(angle); },
    });
}

}  // namespace

LaunchLugConfig::LaunchLugConfig(OpenRocketDocument& document, const GuiServices& services,
                                 std::shared_ptr<ComponentSelection> selection, QWidget* parent)
  : RocketComponentConfig(document, services, launchLugSelection(std::move(selection)), parent)
{
    //: LaunchLugCfg.tab.General
    insertTab(0, makeGeneralTab(), tr("General"),
              //: LaunchLugCfg.tab.Generalprop
              tr("General properties"));
    setSelectedTabIndex(0);
}

QWidget& LaunchLugConfig::makeGeneralTab()
{
    auto* page = makeChild<QWidget>(this);
    auto* grid = makeChild<QGridLayout>(page);
    addTubeRows(*page, *grid);
    grid->setColumnMinimumWidth(kGapColumn, kColumnGap);
    grid->addWidget(&makeRightColumn(*page), 0, kRightColumn, kRowCount + 1, 1, Qt::AlignTop);
    grid->setColumnStretch(kSliderColumn, 1);
    grid->setRowStretch(kRowCount, 1);
    return *page;
}

void LaunchLugConfig::addTubeRows(QWidget& page, QGridLayout& grid)
{
    BindingScope&                              bindings    = scope();
    const std::shared_ptr<ComponentSelection>& selected    = selection();
    const UnitGroup&                           lengthUnits = unitGroup(UnitGroupId::LENGTH);

    // The length.
    const auto length = bindings.make<DoubleBinding>(selected, lengthSource(), lengthUnits,
                                                     DoubleBinding::Options{.min = 0.0});
    m_length          = ConfigFields::addNumberRow(
        grid, kLengthRow,
        //: LaunchLugCfg.lbl.Length
        tr("Length:"), length, QStringLiteral("Length"),
        {.slider = SliderMapping::fromRange(0.0, kLengthSliderMid, kLengthSliderMax)});

    // The outer diameter (the radius times 2).
    const auto outer =
        bindings.make<DoubleBinding>(selected, outerRadiusSource(), lengthUnits,
                                     DoubleBinding::Options{.multiplier = 2.0, .min = 0.0});
    m_outerDiameter = ConfigFields::addNumberRow(
        grid, kOuterDiameterRow,
        //: LaunchLugCfg.lbl.Outerdiam
        tr("Outer diameter:"), outer, QStringLiteral("OuterRadius"),
        {.slider = SliderMapping::fromRange(0.0, kDiameterSliderMid, kDiameterSliderMax)});

    // The inner diameter, its slider up to the outer diameter.
    const auto inner =
        bindings.make<DoubleBinding>(selected, innerRadiusSource(), lengthUnits,
                                     DoubleBinding::Options{.multiplier = 2.0, .min = 0.0});
    m_innerDiameter =
        ConfigFields::addNumberRow(grid, kInnerDiameterRow,
                                   //: LaunchLugCfg.lbl.Innerdiam
                                   tr("Inner diameter:"), inner, QStringLiteral("InnerRadius"),
                                   {.slider = SliderMapping::linear(0.0, outer->value()),
                                    .bounds = {.min = nullptr, .max = outer}});

    // The wall thickness.
    const auto thickness = bindings.make<DoubleBinding>(selected, thicknessSource(), lengthUnits,
                                                        DoubleBinding::Options{.min = 0.0});
    m_thickness =
        ConfigFields::addNumberRow(grid, kThicknessRow,
                                   //: LaunchLugCfg.lbl.Thickness
                                   tr("Thickness:"), thickness, QStringLiteral("Thickness"),
                                   {.slider = SliderMapping::linear(0.0, kThicknessSliderMax)});

    // The instances.
    m_instances = makeChild<InstancesPanel>(&page, bindings);
    grid.addWidget(m_instances, kInstancesRow, 0, 1, kColumnCount);
}

QWidget& LaunchLugConfig::makeRightColumn(QWidget& page)
{
    auto* column = makeChild<QWidget>(&page);
    auto* layout = makeChild<QVBoxLayout>(column);
    layout->setContentsMargins(0, 0, 0, 0);

    // The placement, with the rotation below the offset (Java: placementPanel.add()).
    m_placement = makeChild<PlacementPanel>(column, scope());
    layout->addWidget(m_placement);
    const auto angle = scope().make<DoubleBinding>(
        selection(), angleOffsetSource(), unitGroup(UnitGroupId::ANGLE),
        DoubleBinding::Options{.min = -std::numbers::pi, .max = std::numbers::pi});
    m_angle = ConfigFields::addNumberRow(
        m_placement->grid(), m_placement->nextRow(),
        //: LaunchLugCfg.lbl.Angle
        tr("Rotation:"), angle, QStringLiteral("AngleOffset"),
        {.slider = SliderMapping::linear(-std::numbers::pi, std::numbers::pi)});

    // The material.
    m_material = makeChild<MaterialPanel>(column, scope(), document(), services().materials(),
                                          Material::Type::BULK);
    layout->addWidget(m_material);
    layout->addStretch(1);
    return *column;
}

}  // namespace QtRocket::Gui
