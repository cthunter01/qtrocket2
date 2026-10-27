#include "QtRocket/gui/configdialog/TubeFinSetConfig.h"

#include <memory>
#include <numbers>
#include <utility>

#include <QGridLayout>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/IntBinding.h"
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
#include "QtRocket/gui/widgets/IntSpinBox.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The bounds of the fin count (Java: IntegerModel(component, "FinCount", 1, 8)).
constexpr int kMinFinCount = 1;
constexpr int kMaxFinCount = 8;
/// The length slider: 0, 2 cm at the middle, 10 cm (Java: getSliderModel(0, 0.02, 0.1)).
constexpr double kLengthSliderMid = 0.02;
constexpr double kLengthSliderMax = 0.1;
/// The diameter slider: 0, 4 cm, 20 cm (Java: getSliderModel(0, 0.04, 0.2)).
constexpr double kDiameterSliderMid = 0.04;
constexpr double kDiameterSliderMax = 0.2;
/// The thickness slider: 0 to 1 cm (Java: getSliderModel(0, 0.01)).
constexpr double kThicknessSliderMax = 0.01;
/// The gap before the right column (Java: "gapright 40lp").
constexpr int kColumnGap = 40;

/// The rows of the General tab, and the row below them, which takes the extra height.
constexpr int kFinCountRow      = 0;
constexpr int kLengthRow        = 1;
constexpr int kOuterDiameterRow = 2;
constexpr int kAutomaticRow     = 3;
constexpr int kInnerDiameterRow = 4;
constexpr int kThicknessRow     = 5;
constexpr int kRowCount         = 6;

/// The columns after ConfigFields' four: a gap, the right column.
constexpr int kSliderColumn = 3;
constexpr int kGapColumn    = 4;
constexpr int kRightColumn  = 5;

/// @p selection, whose main component must be a TubeFinSet.
/// @throws BugError when it is not one (or the selection is null or invalid)
[[nodiscard]] std::shared_ptr<ComponentSelection> tubeFinSetSelection(
    std::shared_ptr<ComponentSelection> selection)
{
    if (selection != nullptr && selection->isValid() &&
        dynamic_cast<const TubeFinSet*>(&selection->getMain()) == nullptr)
    {
        bug("The tube fin set configuration needs a tube fin set");
    }
    return selection;  // a null or invalid one is RocketComponentConfig's BugError
}

/// The number of tubes (Java: IntegerModel(component, "FinCount", 1, 8)).
[[nodiscard]] IntSource finCountSource()
{
    return IntSource::of<TubeFinSet>({
        .get = [](const TubeFinSet& fins) { return fins.getFinCount(); },
        .set = [](TubeFinSet& fins, const int& count) { fins.setFinCount(count); },
    });
}

/// The length of the tubes (Java: DoubleModel(component, "Length", UNITS_LENGTH, 0)).
[[nodiscard]] DoubleSource lengthSource()
{
    return DoubleSource::of<TubeFinSet>({
        .get = [](const TubeFinSet& fins) { return fins.getLength(); },
        .set = [](TubeFinSet& fins, const double& length) { fins.setLength(length); },
    });
}

/// The outer radius with its automatic state (Java: DoubleModel(component, "OuterRadius", 2,
/// UNITS_LENGTH, 0), which finds isOuterRadiusAutomatic() and setOuterRadiusAutomatic()).
[[nodiscard]] DoubleSource outerRadiusSource()
{
    return DoubleSource::of<TubeFinSet>({
        .get          = [](const TubeFinSet& fins) { return fins.getOuterRadius(); },
        .set          = [](TubeFinSet& fins, const double& radius) { fins.setOuterRadius(radius); },
        .isAutomatic  = [](const TubeFinSet& fins) { return fins.isOuterRadiusAutomatic(); },
        .setAutomatic = [](TubeFinSet& fins,
                           bool        automatic) { fins.setOuterRadiusAutomatic(automatic); },
    });
}

/// The inner radius (Java: DoubleModel(component, "InnerRadius", 2, UNITS_LENGTH, 0)).
[[nodiscard]] DoubleSource innerRadiusSource()
{
    return DoubleSource::of<TubeFinSet>({
        .get = [](const TubeFinSet& fins) { return fins.getInnerRadius(); },
        .set = [](TubeFinSet& fins, const double& radius) { fins.setInnerRadius(radius); },
    });
}

/// The wall thickness (Java: DoubleModel(component, "Thickness", UNITS_LENGTH, 0)).
[[nodiscard]] DoubleSource thicknessSource()
{
    return DoubleSource::of<TubeFinSet>({
        .get = [](const TubeFinSet& fins) { return fins.getThickness(); },
        .set = [](TubeFinSet& fins, const double& thickness) { fins.setThickness(thickness); },
    });
}

/// The angle of the first tube (Java: DoubleModel(component, "BaseRotation", UNITS_ANGLE)).
[[nodiscard]] DoubleSource baseRotationSource()
{
    return DoubleSource::of<TubeFinSet>({
        .get = [](const TubeFinSet& fins) { return fins.getBaseRotation(); },
        .set = [](TubeFinSet& fins, const double& angle) { fins.setBaseRotation(angle); },
    });
}

}  // namespace

TubeFinSetConfig::TubeFinSetConfig(OpenRocketDocument& document, const GuiServices& services,
                                   std::shared_ptr<ComponentSelection> selection, QWidget* parent)
  : RocketComponentConfig(document, services, tubeFinSetSelection(std::move(selection)), parent)
{
    // OpenRocket's TubeFinSetConfig takes the launch lug's keys for its tab (kept; the texts are
    // the same).
    //: LaunchLugCfg.tab.General
    insertTab(0, makeGeneralTab(), tr("General"),
              //: LaunchLugCfg.tab.Generalprop
              tr("General properties"));
    setSelectedTabIndex(0);
}

QWidget& TubeFinSetConfig::makeGeneralTab()
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

void TubeFinSetConfig::addTubeRows(QWidget& page, QGridLayout& grid)
{
    BindingScope&                              bindings    = scope();
    const std::shared_ptr<ComponentSelection>& selected    = selection();
    const UnitGroup&                           lengthUnits = unitGroup(UnitGroupId::LENGTH);

    // The number of tubes.
    //: TubeFinSetCfg.lbl.Nbroffins
    grid.addWidget(makeChild<QLabel>(&page, tr("Number of fins:")), kFinCountRow, 0);
    m_finCount = makeChild<IntSpinBox>(
        &page, bindings.make<IntBinding>(selected, finCountSource(), kMinFinCount, kMaxFinCount));
    m_finCount->setObjectName(QStringLiteral("FinCount"));
    grid.addWidget(m_finCount, kFinCountRow, 1);

    // The length.
    const auto length = bindings.make<DoubleBinding>(selected, lengthSource(), lengthUnits,
                                                     DoubleBinding::Options{.min = 0.0});
    m_length          = ConfigFields::addNumberRow(
        grid, kLengthRow,
        //: TubeFinSetCfg.lbl.Length
        tr("Length:"), length, QStringLiteral("Length"),
        {.slider = SliderMapping::fromRange(0.0, kLengthSliderMid, kLengthSliderMax)});
    // It takes the focus once a preset is chosen (Java: focusElement = spin).
    setPresetFocusField(*m_length.spinBox);

    // The outer diameter (the radius times 2) and its automatic state.
    const auto outer =
        bindings.make<DoubleBinding>(selected, outerRadiusSource(), lengthUnits,
                                     DoubleBinding::Options{.multiplier = 2.0, .min = 0.0});
    m_outerDiameter = ConfigFields::addNumberRow(
        grid, kOuterDiameterRow,
        //: TubeFinSetCfg.lbl.Outerdiam
        tr("Outer diameter:"), outer, QStringLiteral("OuterRadius"),
        {.slider = SliderMapping::fromRange(0.0, kDiameterSliderMid, kDiameterSliderMax)});
    //: TubeFinSetCfg.checkbox.Automatic
    m_automatic = makeChild<BoolCheckBox>(&page, outer, tr("Automatic"));
    m_automatic->setObjectName(QStringLiteral("OuterRadiusAutomatic"));
    grid.addWidget(m_automatic, kAutomaticRow, 1, 1, 2);

    // The inner diameter, its slider up to the outer diameter.
    const auto inner =
        bindings.make<DoubleBinding>(selected, innerRadiusSource(), lengthUnits,
                                     DoubleBinding::Options{.multiplier = 2.0, .min = 0.0});
    m_innerDiameter =
        ConfigFields::addNumberRow(grid, kInnerDiameterRow,
                                   //: TubeFinSetCfg.lbl.Innerdiam
                                   tr("Inner diameter:"), inner, QStringLiteral("InnerRadius"),
                                   {.slider = SliderMapping::linear(0.0, outer->value()),
                                    .bounds = {.min = nullptr, .max = outer}});

    // The wall thickness.
    const auto thickness = bindings.make<DoubleBinding>(selected, thicknessSource(), lengthUnits,
                                                        DoubleBinding::Options{.min = 0.0});
    m_thickness =
        ConfigFields::addNumberRow(grid, kThicknessRow,
                                   //: TubeFinSetCfg.lbl.Thickness
                                   tr("Thickness:"), thickness, QStringLiteral("Thickness"),
                                   {.slider = SliderMapping::linear(0.0, kThicknessSliderMax)});
}

QWidget& TubeFinSetConfig::makeRightColumn(QWidget& page)
{
    auto* column = makeChild<QWidget>(&page);
    auto* layout = makeChild<QVBoxLayout>(column);
    layout->setContentsMargins(0, 0, 0, 0);

    // The placement, with the fin rotation below the offset (Java: placementPanel.add()).
    m_placement = makeChild<PlacementPanel>(column, scope());
    layout->addWidget(m_placement);
    const auto rotation = scope().make<DoubleBinding>(
        selection(), baseRotationSource(), unitGroup(UnitGroupId::ANGLE), DoubleBinding::Options{});
    m_baseRotation = ConfigFields::addNumberRow(
        m_placement->grid(), m_placement->nextRow(),
        //: TubeFinSetCfg.lbl.Finrotation
        tr("Fin rotation:"), rotation, QStringLiteral("BaseRotation"),
        {.slider = SliderMapping::linear(-std::numbers::pi, std::numbers::pi)});
    //: TubeFinSetCfg.lbl.ttip.Finrotation
    m_baseRotation.label->setToolTip(tr("The angle of the first fin in the fin set."));

    // The material.
    m_material = makeChild<MaterialPanel>(column, scope(), document(), services().materials(),
                                          Material::Type::BULK);
    layout->addWidget(m_material);
    layout->addStretch(1);
    return *column;
}

}  // namespace QtRocket::Gui
