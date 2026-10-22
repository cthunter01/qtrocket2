#include "QtRocket/gui/configdialog/RailButtonConfig.h"

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
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The end of the diameter, total height and screw height sliders: 2 cm (Java:
/// getSliderModel(0, 0.02) and getSliderModel(DoubleModel(component, "MinTotalHeight"), 0.02)).
constexpr double kSliderMax = 0.02;
/// The bounds of the rotation: -180 and 180, in radians (Java's DoubleModel(component,
/// "AngleOffset", UNITS_ANGLE, -180, +180); see the class comment).
constexpr double kAngleMin = -180.0;
constexpr double kAngleMax = 180.0;
/// The gap before the right column (Java: "gapright 20lp").
constexpr int kColumnGap = 20;

/// The rows of the General tab, and the row below them, which takes the extra height.
constexpr int kOuterDiameterRow = 0;
constexpr int kInnerDiameterRow = 1;
constexpr int kBaseHeightRow    = 2;
constexpr int kFlangeHeightRow  = 3;
constexpr int kTotalHeightRow   = 4;
constexpr int kScrewHeightRow   = 5;
constexpr int kInstancesRow     = 6;
constexpr int kRowCount         = 7;

/// The columns: ConfigFields' four, a gap, the right column.
constexpr int kSliderColumn = 3;
constexpr int kColumnCount  = 4;
constexpr int kGapColumn    = 4;
constexpr int kRightColumn  = 5;

/// @p selection, whose main component must be a RailButton.
/// @throws BugError when it is not one (or the selection is null or invalid)
[[nodiscard]] std::shared_ptr<ComponentSelection> railButtonSelection(
    std::shared_ptr<ComponentSelection> selection)
{
    if (selection != nullptr && selection->isValid() &&
        dynamic_cast<const RailButton*>(&selection->getMain()) == nullptr)
    {
        bug("The rail button configuration needs a rail button");
    }
    return selection;  // a null or invalid one is RocketComponentConfig's BugError
}

/// The outer diameter (Java: DoubleModel(component, "OuterDiameter", UNITS_LENGTH, 0)).
[[nodiscard]] DoubleSource outerDiameterSource()
{
    return DoubleSource::of<RailButton>({
        .get = [](const RailButton& button) { return button.getOuterDiameter(); },
        .set = [](RailButton&   button,
                  const double& diameter) { button.setOuterDiameter(diameter); },
    });
}

/// The inner diameter (Java: DoubleModel(component, "InnerDiameter", UNITS_LENGTH, 0)).
[[nodiscard]] DoubleSource innerDiameterSource()
{
    return DoubleSource::of<RailButton>({
        .get = [](const RailButton& button) { return button.getInnerDiameter(); },
        .set = [](RailButton&   button,
                  const double& diameter) { button.setInnerDiameter(diameter); },
    });
}

/// The base height (Java: DoubleModel(component, "BaseHeight", UNITS_LENGTH, 0)).
[[nodiscard]] DoubleSource baseHeightSource()
{
    return DoubleSource::of<RailButton>({
        .get = [](const RailButton& button) { return button.getBaseHeight(); },
        .set = [](RailButton& button, const double& height) { button.setBaseHeight(height); },
    });
}

/// The flange height (Java: DoubleModel(component, "FlangeHeight", UNITS_LENGTH, 0)).
[[nodiscard]] DoubleSource flangeHeightSource()
{
    return DoubleSource::of<RailButton>({
        .get = [](const RailButton& button) { return button.getFlangeHeight(); },
        .set = [](RailButton& button, const double& height) { button.setFlangeHeight(height); },
    });
}

/// The total height without the screw (Java: DoubleModel(component, "TotalHeight", UNITS_LENGTH,
/// 0)).
[[nodiscard]] DoubleSource totalHeightSource()
{
    return DoubleSource::of<RailButton>({
        .get = [](const RailButton& button) { return button.getTotalHeight(); },
        .set = [](RailButton& button, const double& height) { button.setTotalHeight(height); },
    });
}

/// The height of the screw head (Java: DoubleModel(component, "ScrewHeight", UNITS_LENGTH, 0)).
[[nodiscard]] DoubleSource screwHeightSource()
{
    return DoubleSource::of<RailButton>({
        .get = [](const RailButton& button) { return button.getScrewHeight(); },
        .set = [](RailButton& button, const double& height) { button.setScrewHeight(height); },
    });
}

/// The largest base height, read-only: the end of the base height's slider (Java:
/// DoubleModel(component, "MaxBaseHeight", UNITS_LENGTH)).
[[nodiscard]] DoubleSource maxBaseHeightSource()
{
    return DoubleSource::of<RailButton>({
        .get = [](const RailButton& button) { return button.getMaxBaseHeight(); },
    });
}

/// The largest flange height, read-only (Java: DoubleModel(component, "MaxFlangeHeight",
/// UNITS_LENGTH)).
[[nodiscard]] DoubleSource maxFlangeHeightSource()
{
    return DoubleSource::of<RailButton>({
        .get = [](const RailButton& button) { return button.getMaxFlangeHeight(); },
    });
}

/// The smallest total height, read-only: the start of the total height's slider (Java:
/// DoubleModel(component, "MinTotalHeight", UNITS_LENGTH)).
[[nodiscard]] DoubleSource minTotalHeightSource()
{
    return DoubleSource::of<RailButton>({
        .get = [](const RailButton& button) { return button.getMinTotalHeight(); },
    });
}

/// The angle around the parent's axis (Java: DoubleModel(component, "AngleOffset", UNITS_ANGLE,
/// -180, +180)).
[[nodiscard]] DoubleSource angleOffsetSource()
{
    return DoubleSource::of<RailButton>({
        .get = [](const RailButton& button) { return button.getAngleOffset(); },
        .set = [](RailButton& button, const double& angle) { button.setAngleOffset(angle); },
    });
}

}  // namespace

RailButtonConfig::RailButtonConfig(OpenRocketDocument& document, const GuiServices& services,
                                   std::shared_ptr<ComponentSelection> selection, QWidget* parent)
  : RocketComponentConfig(document, services, railButtonSelection(std::move(selection)), parent)
{
    //: RailBtnCfg.tab.General
    insertTab(0, makeGeneralTab(), tr("General"),
              //: RailBtnCfg.tab.GeneralProp
              tr("General properties"));
    setSelectedTabIndex(0);
}

QWidget& RailButtonConfig::makeGeneralTab()
{
    auto* page = makeChild<QWidget>(this);
    auto* grid = makeChild<QGridLayout>(page);
    addDimensionRows(*page, *grid);
    grid->setColumnMinimumWidth(kGapColumn, kColumnGap);
    grid->addWidget(&makeRightColumn(*page), 0, kRightColumn, kRowCount + 1, 1, Qt::AlignTop);
    grid->setColumnStretch(kSliderColumn, 1);
    grid->setRowStretch(kRowCount, 1);
    return *page;
}

void RailButtonConfig::addDimensionRows(QWidget& page, QGridLayout& grid)
{
    BindingScope&                              bindings    = scope();
    const std::shared_ptr<ComponentSelection>& selected    = selection();
    const UnitGroup&                           lengthUnits = unitGroup(UnitGroupId::LENGTH);
    const DoubleBinding::Options               atLeastZero{.min = 0.0};

    // The diameters.
    m_outerDiameter = ConfigFields::addNumberRow(
        grid, kOuterDiameterRow,
        //: RailBtnCfg.lbl.OuterDiam
        tr("Outer Diameter:"),
        bindings.make<DoubleBinding>(selected, outerDiameterSource(), lengthUnits, atLeastZero),
        QStringLiteral("OuterDiameter"), {.slider = SliderMapping::linear(0.0, kSliderMax)});
    m_innerDiameter = ConfigFields::addNumberRow(
        grid, kInnerDiameterRow,
        //: RailBtnCfg.lbl.InnerDiam
        tr("Inner Diameter:"),
        bindings.make<DoubleBinding>(selected, innerDiameterSource(), lengthUnits, atLeastZero),
        QStringLiteral("InnerDiameter"), {.slider = SliderMapping::linear(0.0, kSliderMax)});

    // The base and flange heights, their sliders up to what the other heights leave.
    const auto maxBase = bindings.make<DoubleBinding>(selected, maxBaseHeightSource(), lengthUnits,
                                                      DoubleBinding::Options{});
    m_baseHeight       = ConfigFields::addNumberRow(
        grid, kBaseHeightRow,
        //: RailBtnCfg.lbl.BaseHeight
        tr("Base Height:"),
        bindings.make<DoubleBinding>(selected, baseHeightSource(), lengthUnits, atLeastZero),
        QStringLiteral("BaseHeight"),
        {.slider = SliderMapping::linear(0.0, maxBase->value()),
         .bounds = {.min = nullptr, .max = maxBase}});
    const auto maxFlange = bindings.make<DoubleBinding>(selected, maxFlangeHeightSource(),
                                                        lengthUnits, DoubleBinding::Options{});
    m_flangeHeight       = ConfigFields::addNumberRow(
        grid, kFlangeHeightRow,
        //: RailBtnCfg.lbl.FlangeHeight
        tr("Flange Height:"),
        bindings.make<DoubleBinding>(selected, flangeHeightSource(), lengthUnits, atLeastZero),
        QStringLiteral("FlangeHeight"),
        {.slider = SliderMapping::linear(0.0, maxFlange->value()),
         .bounds = {.min = nullptr, .max = maxFlange}});

    // The total height, its slider from the base and flange heights.
    const auto minTotal = bindings.make<DoubleBinding>(selected, minTotalHeightSource(),
                                                       lengthUnits, DoubleBinding::Options{});
    m_totalHeight       = ConfigFields::addNumberRow(
        grid, kTotalHeightRow,
        //: RailBtnCfg.lbl.TotalHeight
        tr("Total Height (excl. screw):"),
        bindings.make<DoubleBinding>(selected, totalHeightSource(), lengthUnits, atLeastZero),
        QStringLiteral("TotalHeight"),
        {.slider = SliderMapping::linear(minTotal->value(), kSliderMax),
         .bounds = {.min = minTotal, .max = nullptr}});

    // The screw.
    m_screwHeight = ConfigFields::addNumberRow(
        grid, kScrewHeightRow,
        //: RailBtnCfg.lbl.ScrewHeight
        tr("Screw Height:"),
        bindings.make<DoubleBinding>(selected, screwHeightSource(), lengthUnits, atLeastZero),
        QStringLiteral("ScrewHeight"), {.slider = SliderMapping::linear(0.0, kSliderMax)});

    // The instances.
    m_instances = makeChild<InstancesPanel>(&page, bindings);
    grid.addWidget(m_instances, kInstancesRow, 0, 1, kColumnCount);
}

QWidget& RailButtonConfig::makeRightColumn(QWidget& page)
{
    auto* column = makeChild<QWidget>(&page);
    auto* layout = makeChild<QVBoxLayout>(column);
    layout->setContentsMargins(0, 0, 0, 0);

    // The placement, with the rotation below the offset (Java: placementPanel.add()).
    m_placement = makeChild<PlacementPanel>(column, scope());
    layout->addWidget(m_placement);
    // OpenRocket's bounds are -180 and 180 radians (see the class comment); the slider spans
    // -pi to pi.
    const auto angle =
        scope().make<DoubleBinding>(selection(), angleOffsetSource(), unitGroup(UnitGroupId::ANGLE),
                                    DoubleBinding::Options{.min = kAngleMin, .max = kAngleMax});
    m_angle = ConfigFields::addNumberRow(
        m_placement->grid(), m_placement->nextRow(),
        //: RailBtnCfg.lbl.Angle
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
