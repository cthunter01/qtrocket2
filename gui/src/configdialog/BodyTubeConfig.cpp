#include "QtRocket/gui/configdialog/BodyTubeConfig.h"

#include <memory>
#include <utility>

#include <QGridLayout>
#include <QString>
#include <QWidget>
#include <Qt>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/BoolBinding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/BodyComponentFields.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/configdialog/MotorConfig.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The length slider: 0, 50 cm at the middle, 2 m (Java: getSliderModel(0, 0.5, maxLength), the
/// maximum a DoubleModel(2.0), which is a ValueSliderModel(min, 0.5, mid, max): quadratic beyond
/// the middle position even where mid is the mean of min and max).
constexpr double kLengthSliderPosition = 0.5;
constexpr double kLengthSliderMid      = 0.5;
constexpr double kLengthSliderMax      = 2.0;
/// The diameter slider: 0, 4 cm, 20 cm (Java: getSliderModel(0, 0.04, 0.2)).
constexpr double kDiameterSliderMid = 0.04;
constexpr double kDiameterSliderMax = 0.2;
/// The wall thickness slider: 0 to 1 cm.
constexpr double kThicknessSliderMax = 0.01;
/// The gap before the material panel (Java: "gapleft 20lp").
constexpr int kMaterialGap = 20;

/// The rows of the General tab, and the row below them, which takes the extra height.
constexpr int kLengthRow        = 0;
constexpr int kOuterDiameterRow = 1;
constexpr int kAutomaticRow     = 2;
constexpr int kInnerDiameterRow = 3;
constexpr int kThicknessRow     = 4;
constexpr int kFilledRow        = 5;
constexpr int kRowCount         = 6;

/// The columns of the General tab after ConfigFields' four: a gap, the material panel.
constexpr int kSliderColumn   = 3;
constexpr int kGapColumn      = 4;
constexpr int kMaterialColumn = 5;

/// @p selection, whose main component must be a BodyTube.
/// @throws BugError when it is not one (or the selection is null or invalid)
[[nodiscard]] std::shared_ptr<ComponentSelection> tubeSelection(
    std::shared_ptr<ComponentSelection> selection)
{
    if (selection != nullptr && selection->isValid() &&
        dynamic_cast<const BodyTube*>(&selection->getMain()) == nullptr)
    {
        bug("The body tube configuration needs a body tube");
    }
    return selection;  // a null or invalid one is RocketComponentConfig's BugError
}

/// The outer radius with its automatic state (Java: DoubleModel(component, "OuterRadius", 2,
/// UNITS_LENGTH, 0), which finds isOuterRadiusAutomatic() and setOuterRadiusAutomatic()).
[[nodiscard]] DoubleSource outerRadiusSource()
{
    return DoubleSource::of<BodyTube>({
        .get          = [](const BodyTube& tube) { return tube.getOuterRadius(); },
        .set          = [](BodyTube& tube, const double& radius) { tube.setOuterRadius(radius); },
        .isAutomatic  = [](const BodyTube& tube) { return tube.isOuterRadiusAutomatic(); },
        .setAutomatic = [](BodyTube& tube,
                           bool      automatic) { tube.setOuterRadiusAutomatic(automatic); },
    });
}

/// The inner radius (Java: DoubleModel(component, "InnerRadius", 2, UNITS_LENGTH, 0)).
[[nodiscard]] DoubleSource innerRadiusSource()
{
    return DoubleSource::of<BodyTube>({
        .get = [](const BodyTube& tube) { return tube.getInnerRadius(); },
        .set = [](BodyTube& tube, const double& radius) { tube.setInnerRadius(radius); },
    });
}

}  // namespace

BodyTubeConfig::BodyTubeConfig(OpenRocketDocument& document, const GuiServices& services,
                               std::shared_ptr<ComponentSelection> selection, QWidget* parent)
  : RocketComponentConfig(document, services, tubeSelection(std::move(selection)), parent)
{
    //: BodyTubecfg.tab.General
    insertTab(0, makeGeneralTab(), tr("General"),
              //: BodyTubecfg.tab.Generalproperties
              tr("General properties"));
    setSelectedTabIndex(0);

    // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer): after General (Java's order)
    m_motor = makeChild<MotorConfig>(this, scope());
    //: BodyTubecfg.tab.Motor
    insertTab(1, *m_motor, tr("Motor"),
              //: BodyTubecfg.tab.Motormountconf
              tr("Motor mount configuration"));
}

QWidget& BodyTubeConfig::makeGeneralTab()
{
    auto*                                      page        = makeChild<QWidget>(this);
    auto*                                      grid        = makeChild<QGridLayout>(page);
    BindingScope&                              bindings    = scope();
    const std::shared_ptr<ComponentSelection>& selected    = selection();
    const UnitGroup&                           lengthUnits = unitGroup(UnitGroupId::LENGTH);

    // The length.
    const auto length =
        bindings.make<DoubleBinding>(selected, BodyComponentFields::lengthSource(), lengthUnits,
                                     DoubleBinding::Options{.min = 0.0});
    m_length = ConfigFields::addNumberRow(
        *grid, kLengthRow,
        //: BodyTubecfg.lbl.Bodytubelength
        tr("Length:"), length, QStringLiteral("Length"),
        {.slider = SliderMapping::quadratic(0.0, kLengthSliderPosition, kLengthSliderMid,
                                            kLengthSliderMax)});
    // It takes the focus once a preset is chosen (Java: focusElement = spin).
    setPresetFocusField(*m_length.spinBox);

    // The outer diameter (the radius times 2) and its automatic state.
    const auto outer =
        bindings.make<DoubleBinding>(selected, outerRadiusSource(), lengthUnits,
                                     DoubleBinding::Options{.multiplier = 2.0, .min = 0.0});
    m_outerDiameter = ConfigFields::addNumberRow(
        *grid, kOuterDiameterRow,
        //: BodyTubecfg.lbl.Outerdiameter
        tr("Outer diameter:"), outer, QStringLiteral("OuterRadius"),
        {.slider = SliderMapping::fromRange(0.0, kDiameterSliderMid, kDiameterSliderMax)});
    //: BodyTubecfg.checkbox.Automatic
    m_automatic = makeChild<BoolCheckBox>(page, outer, tr("Automatic"));
    m_automatic->setObjectName(QStringLiteral("OuterRadiusAutomatic"));
    grid->addWidget(m_automatic, kAutomaticRow, 1, 1, 2);
    updateAutomaticCheckBox();

    // The inner diameter, its slider up to the outer diameter.
    const auto inner =
        bindings.make<DoubleBinding>(selected, innerRadiusSource(), lengthUnits,
                                     DoubleBinding::Options{.multiplier = 2.0, .min = 0.0});
    m_innerDiameter =
        ConfigFields::addNumberRow(*grid, kInnerDiameterRow,
                                   //: BodyTubecfg.lbl.Innerdiameter
                                   tr("Inner diameter:"), inner, QStringLiteral("InnerRadius"),
                                   {.slider = SliderMapping::linear(0.0, outer->value()),
                                    .bounds = {.min = nullptr, .max = outer}});

    // The wall thickness and "Filled".
    const auto thickness =
        bindings.make<DoubleBinding>(selected, BodyComponentFields::thicknessSource(), lengthUnits,
                                     DoubleBinding::Options{.min = 0.0});
    m_thickness =
        ConfigFields::addNumberRow(*grid, kThicknessRow,
                                   //: BodyTubecfg.lbl.Wallthickness
                                   tr("Wall thickness:"), thickness, QStringLiteral("Thickness"),
                                   {.slider = SliderMapping::linear(0.0, kThicknessSliderMax)});
    m_filled = makeChild<BoolCheckBox>(
        page, bindings.make<BoolBinding>(selected, BodyComponentFields::filledSource()),
        //: BodyTubecfg.checkbox.Filled
        tr("Filled"));
    m_filled->setObjectName(QStringLiteral("Filled"));
    //: BodyTubecfg.checkbox.Filled.ttip
    m_filled->setToolTip(tr("Changes the body tube from a hollow tube to a solid rod."));
    grid->addWidget(m_filled, kFilledRow, 1, 1, 2);

    // The material, to the right of the fields.
    m_material = makeChild<MaterialPanel>(page, bindings, document(), services().materials(),
                                          Material::Type::BULK);
    grid->setColumnMinimumWidth(kGapColumn, kMaterialGap);
    grid->addWidget(m_material, 0, kMaterialColumn, kRowCount, 1, Qt::AlignTop);
    grid->setColumnStretch(kSliderColumn, 1);
    grid->setRowStretch(kRowCount, 1);
    return *page;
}

void BodyTubeConfig::updateAutomaticCheckBox()
{
    const auto&               tube      = dynamic_cast<const BodyTube&>(selection()->getMain());
    const SymmetricComponent* previous  = tube.getPreviousSymmetricComponent();
    const SymmetricComponent* next      = tube.getNextSymmetricComponent();
    const auto                switchOff = [this] {
        selection()->apply<BodyTube>(
            [](BodyTube& selected) { selected.setOuterRadiusAutomatic(false); });
    };
    if (previous == nullptr && next == nullptr)
    {
        BodyComponentFields::setAutomaticState(
            *m_automatic, false,
            //: BodyTubecfg.checkbox.ttip.Automatic_noReferenceComponent
            tr("There is no previous/next component to take the diameter of."));
        switchOff();
        return;
    }
    // Java's three cases in which the neighbours already take their diameter from this tube.
    const bool previousUsesThis =
        previous != nullptr && next == nullptr && previous->usesNextCompAutomatic();
    const bool nextUsesThis =
        next != nullptr && previous == nullptr && next->usesPreviousCompAutomatic();
    const bool bothUseThis = next != nullptr && previous != nullptr &&
                             previous->usesNextCompAutomatic() && next->usesPreviousCompAutomatic();
    if (!previousUsesThis && !nextUsesThis && !bothUseThis)
    {
        BodyComponentFields::setAutomaticState(
            *m_automatic, true,
            //: BodyTubecfg.checkbox.ttip.Automatic
            tr("Uses the diameter of the previous/next component."));
    }
    else
    {
        BodyComponentFields::setAutomaticState(
            *m_automatic, false,
            //: BodyTubecfg.checkbox.ttip.Automatic_alreadyAuto
            tr("The previous/next component already has its auto setting turned on."));
        switchOff();
    }
}

}  // namespace QtRocket::Gui
