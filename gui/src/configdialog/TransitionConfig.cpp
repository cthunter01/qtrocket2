#include "QtRocket/gui/configdialog/TransitionConfig.h"

#include <memory>
#include <utility>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/BoolBinding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/binding/TransitionShapeBinding.h"
#include "QtRocket/gui/configdialog/BodyComponentFields.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/configdialog/ShoulderPanel.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The length slider: 0, 5 cm at the middle, 30 cm.
constexpr double kLengthSliderMid = 0.05;
constexpr double kLengthSliderMax = 0.3;
/// The diameter sliders: 0, 4 cm, 20 cm.
constexpr double kDiameterSliderMid = 0.04;
constexpr double kDiameterSliderMax = 0.2;
/// The rows of the description area (Java: new DescriptionArea(5)) and its least width (Java:
/// "wmin 250lp").
constexpr int kDescriptionRows     = 5;
constexpr int kDescriptionMinWidth = 250;
/// The gap before the description column (Java: "gapleft 40lp").
constexpr int kColumnGap = 40;

/// The rows of the General tab, and the row below them, which takes the extra height.
constexpr int kShapeRow          = 0;
constexpr int kShapeParameterRow = 1;
constexpr int kLengthRow         = 2;
constexpr int kForeDiameterRow   = 3;
constexpr int kForeAutomaticRow  = 4;
constexpr int kAftDiameterRow    = 5;
constexpr int kAftAutomaticRow   = 6;
constexpr int kThicknessRow      = 7;
constexpr int kFilledRow         = 8;
constexpr int kRowCount          = 9;

/// The columns after ConfigFields' four: a gap, the description column.
constexpr int kSliderColumn      = 3;
constexpr int kGapColumn         = 4;
constexpr int kDescriptionColumn = 5;

/// @p selection, whose main component must be a Transition.
/// @throws BugError when it is not one (or the selection is null or invalid)
[[nodiscard]] std::shared_ptr<ComponentSelection> transitionSelection(
    std::shared_ptr<ComponentSelection> selection)
{
    if (selection != nullptr && selection->isValid() &&
        dynamic_cast<const Transition*>(&selection->getMain()) == nullptr)
    {
        bug("The transition configuration needs a transition");
    }
    return selection;  // a null or invalid one is RocketComponentConfig's BugError
}

/// The fore radius with its automatic state (Java: DoubleModel(component, "ForeRadius", 2,
/// UNITS_LENGTH, 0), which finds setForeRadius(double) and setForeRadiusAutomatic(boolean)).
[[nodiscard]] DoubleSource foreRadiusSource()
{
    return DoubleSource::of<Transition>({
        .get = [](const Transition& transition) { return transition.getForeRadius(); },
        .set = [](Transition&   transition,
                  const double& radius) { transition.setForeRadius(radius); },
        .isAutomatic =
            [](const Transition& transition) { return transition.isForeRadiusAutomatic(); },
        .setAutomatic = [](Transition& transition,
                           bool        automatic) { transition.setForeRadiusAutomatic(automatic); },
    });
}

/// The aft radius with its automatic state (Java: DoubleModel(component, "AftRadius", 2,
/// UNITS_LENGTH, 0)).
[[nodiscard]] DoubleSource aftRadiusSource()
{
    return DoubleSource::of<Transition>({
        .get = [](const Transition& transition) { return transition.getAftRadius(); },
        .set = [](Transition&   transition,
                  const double& radius) { transition.setAftRadius(radius); },
        .isAutomatic =
            [](const Transition& transition) { return transition.isAftRadiusAutomatic(); },
        .setAutomatic = [](Transition& transition,
                           bool        automatic) { transition.setAftRadiusAutomatic(automatic); },
    });
}

/// Whether the profile is clipped, enabled for a shape that can be clipped (Java:
/// BooleanModel(component, "Clipped"), which finds isClippedEnabled()).
[[nodiscard]] BoolSource clippedSource()
{
    return BoolSource::of<Transition>({
        .get = [](const Transition& transition) { return transition.isClipped(); },
        .set = [](Transition& transition, const bool& clipped) { transition.setClipped(clipped); },
        .isEnabled = [](const Transition& transition) { return transition.isClippedEnabled(); },
    });
}

}  // namespace

TransitionConfig::TransitionConfig(OpenRocketDocument& document, const GuiServices& services,
                                   std::shared_ptr<ComponentSelection> selection, QWidget* parent)
  : RocketComponentConfig(document, services, transitionSelection(std::move(selection)), parent)
{
    //: TransitionCfg.tab.General
    insertTab(0, makeGeneralTab(), tr("General"),
              //: TransitionCfg.tab.Generalproperties
              tr("General properties"));
    // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer): after General (Java's order)
    m_shoulder = makeChild<ShoulderPanel>(this, scope());
    //: TransitionCfg.tab.Shoulder
    insertTab(1, *m_shoulder, tr("Shoulder"),
              //: TransitionCfg.tab.Shoulderproperties
              tr("Shoulder properties"));
    setSelectedTabIndex(0);
}

QWidget& TransitionConfig::makeGeneralTab()
{
    auto*                                      page     = makeChild<QWidget>(this);
    auto*                                      grid     = makeChild<QGridLayout>(page);
    BindingScope&                              bindings = scope();
    const std::shared_ptr<ComponentSelection>& selected = selection();

    addShapeRows(*page, *grid);

    // The length.
    const auto length = bindings.make<DoubleBinding>(selected, BodyComponentFields::lengthSource(),
                                                     unitGroup(UnitGroupId::LENGTH),
                                                     DoubleBinding::Options{.min = 0.0});
    m_length          = ConfigFields::addNumberRow(
        *grid, kLengthRow,
        //: TransitionCfg.lbl.Transitionlength
        tr("Length:"), length, QStringLiteral("Length"),
        {.slider = SliderMapping::fromRange(0.0, kLengthSliderMid, kLengthSliderMax)});

    addDiameterRows(*page, *grid);

    // The wall thickness and "Filled".
    m_thickness = BodyComponentFields::addThicknessRow(bindings, *grid, kThicknessRow,
                                                       //: TransitionCfg.lbl.Wallthickness
                                                       tr("Wall thickness:"));
    m_filled    = makeChild<BoolCheckBox>(
        page, bindings.make<BoolBinding>(selected, BodyComponentFields::filledSource()),
        //: TransitionCfg.checkbox.Filled
        tr("Filled"));
    m_filled->setObjectName(QStringLiteral("Filled"));
    //: TransitionCfg.checkbox.Filled.ttip
    m_filled->setToolTip(tr("Makes this component solid instead of hollow."));
    grid->addWidget(m_filled, kFilledRow, 1, 1, 2);

    // The description and the material, to the right.
    grid->setColumnMinimumWidth(kGapColumn, kColumnGap);
    grid->addWidget(&makeDescriptionColumn(*page), 0, kDescriptionColumn, kRowCount, 1,
                    Qt::AlignTop);
    grid->setColumnStretch(kSliderColumn, 1);
    grid->setRowStretch(kRowCount, 1);
    return *page;
}

void TransitionConfig::addShapeRows(QWidget& page, QGridLayout& grid)
{
    //: TransitionCfg.lbl.Transitionshape
    grid.addWidget(makeChild<QLabel>(&page, tr("Transition shape:")), kShapeRow, 0);
    // The shape and "Clipped" share the row's three columns (Java: "span 3, split 2").
    auto* shapeRow    = makeChild<QWidget>(&page);
    auto* shapeLayout = makeChild<QHBoxLayout>(shapeRow);
    shapeLayout->setContentsMargins(0, 0, 0, 0);
    m_shapeBinding = scope().keep(makeTransitionShapeBinding(selection()));
    m_shape        = makeChild<EnumComboBox>(shapeRow, m_shapeBinding);
    m_shape->setObjectName(QStringLiteral("ShapeType"));
    shapeLayout->addWidget(m_shape);
    scope().keepConnection(m_shapeBinding->addChangeListener([this] { updateShape(); }));

    //: TransitionCfg.checkbox.Clipped
    m_clipped = makeChild<BoolCheckBox>(
        shapeRow, scope().make<BoolBinding>(selection(), clippedSource()), tr("Clipped"));
    m_clipped->setObjectName(QStringLiteral("Clipped"));
    //: TransitionCfg.checkbox.Clipped.ttip
    m_clipped->setToolTip(tr("Changes the transition end tangency point from rounded to sharp."));
    shapeLayout->addWidget(m_clipped);
    shapeLayout->addStretch(1);
    grid.addWidget(shapeRow, kShapeRow, 1, 1, 3);

    m_shapeParameter = BodyComponentFields::addShapeParameterRow(scope(), grid, kShapeParameterRow,
                                                                 //: TransitionCfg.lbl.Shapeparam
                                                                 tr("Shape parameter:"));
}

void TransitionConfig::addDiameterRows(QWidget& page, QGridLayout& grid)
{
    BindingScope&                              bindings    = scope();
    const std::shared_ptr<ComponentSelection>& selected    = selection();
    const UnitGroup&                           lengthUnits = unitGroup(UnitGroupId::LENGTH);
    const auto                                 diameterSlider =
        SliderMapping::fromRange(0.0, kDiameterSliderMid, kDiameterSliderMax);

    // The fore diameter (the radius times 2) and its automatic state.
    const auto fore =
        bindings.make<DoubleBinding>(selected, foreRadiusSource(), lengthUnits,
                                     DoubleBinding::Options{.multiplier = 2.0, .min = 0.0});
    m_foreDiameter = ConfigFields::addNumberRow(
        grid, kForeDiameterRow,
        //: TransitionCfg.lbl.Forediam
        tr("Fore diameter:"), fore, QStringLiteral("ForeRadius"), {.slider = diameterSlider});
    //: TransitionCfg.checkbox.Automatic
    m_foreAutomatic = makeChild<BoolCheckBox>(&page, fore, tr("Automatic"));
    m_foreAutomatic->setObjectName(QStringLiteral("ForeRadiusAutomatic"));
    grid.addWidget(m_foreAutomatic, kForeAutomaticRow, 1, 1, 2);
    updateForeAutomaticCheckBox();

    // The aft diameter and its automatic state.
    const auto aft =
        bindings.make<DoubleBinding>(selected, aftRadiusSource(), lengthUnits,
                                     DoubleBinding::Options{.multiplier = 2.0, .min = 0.0});
    m_aftDiameter = ConfigFields::addNumberRow(
        grid, kAftDiameterRow,
        //: TransitionCfg.lbl.Aftdiam
        tr("Aft diameter:"), aft, QStringLiteral("AftRadius"), {.slider = diameterSlider});
    //: TransitionCfg.checkbox.Automatic
    m_aftAutomatic = makeChild<BoolCheckBox>(&page, aft, tr("Automatic"));
    m_aftAutomatic->setObjectName(QStringLiteral("AftRadiusAutomatic"));
    grid.addWidget(m_aftAutomatic, kAftAutomaticRow, 1, 1, 2);
    updateAftAutomaticCheckBox();
}

QWidget& TransitionConfig::makeDescriptionColumn(QWidget& page)
{
    auto* column = makeChild<QWidget>(&page);
    auto* layout = makeChild<QVBoxLayout>(column);
    layout->setContentsMargins(0, 0, 0, 0);
    m_description = makeChild<DescriptionArea>(column, kDescriptionRows);
    m_description->setMinimumWidth(kDescriptionMinWidth);
    layout->addWidget(m_description);
    m_material = makeChild<MaterialPanel>(column, scope(), document(), services().materials(),
                                          Material::Type::BULK);
    layout->addWidget(m_material);
    layout->addStretch(1);
    updateShape();
    return *column;
}

void TransitionConfig::updateShape()
{
    if (m_shapeBinding == nullptr || !m_shapeBinding->isValid())
    {
        return;  // the dialog closes
    }
    const TransitionShape shape = m_shapeBinding->value();
    if (m_description != nullptr)
    {
        m_description->setDescription(
            translatedEnumText(transitionDescriptionKey(shape), transitionDescription(shape)));
    }
    BodyComponentFields::setShapeParameterEnabled(m_shapeParameter, usesParameter(shape));
}

void TransitionConfig::updateForeAutomaticCheckBox()
{
    const auto& transition = dynamic_cast<const Transition&>(selection()->getMain());
    if (transition.canUsePreviousCompAutomatic())
    {
        BodyComponentFields::setAutomaticState(
            *m_foreAutomatic, true,
            //: TransitionCfg.checkbox.ttip.Automatic
            tr("Uses the diameter of the previous/next component."));
        return;
    }
    BodyComponentFields::setAutomaticState(
        *m_foreAutomatic, false,
        transition.getPreviousSymmetricComponent() == nullptr
            //: TransitionCfg.checkbox.ttip.Automatic_noReferenceComponent
            ? tr("There is no previous/next component to take the diameter of.")
            //: TransitionCfg.checkbox.ttip.Automatic_alreadyAuto
            : tr("The previous/next component already has its auto setting turned on."));
    selection()->apply<Transition>(
        [](Transition& selected) { selected.setForeRadiusAutomatic(false); });
}

void TransitionConfig::updateAftAutomaticCheckBox()
{
    const auto& transition = dynamic_cast<const Transition&>(selection()->getMain());
    if (transition.canUseNextCompAutomatic())
    {
        BodyComponentFields::setAutomaticState(
            *m_aftAutomatic, true,
            //: TransitionCfg.checkbox.ttip.Automatic
            tr("Uses the diameter of the previous/next component."));
        return;
    }
    BodyComponentFields::setAutomaticState(
        *m_aftAutomatic, false,
        transition.getNextSymmetricComponent() == nullptr
            //: TransitionCfg.checkbox.ttip.Automatic_noReferenceComponent
            ? tr("There is no previous/next component to take the diameter of.")
            //: TransitionCfg.checkbox.ttip.Automatic_alreadyAuto
            : tr("The previous/next component already has its auto setting turned on."));
    selection()->apply<Transition>(
        [](Transition& selected) { selected.setAftRadiusAutomatic(false); });
}

}  // namespace QtRocket::Gui
