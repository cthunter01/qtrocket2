#include "QtRocket/gui/configdialog/NoseConeConfig.h"

#include <memory>
#include <utility>

#include <QAbstractButton>
#include <QGridLayout>
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
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The length slider: 0, 10 cm at the middle, 70 cm.
constexpr double kLengthSliderMid = 0.1;
constexpr double kLengthSliderMax = 0.7;
/// The diameter slider: 0, 4 cm, 20 cm.
constexpr double kDiameterSliderMid = 0.04;
constexpr double kDiameterSliderMax = 0.2;
/// The rows of the description area (Java: new DescriptionArea(5)) and its least width (Java:
/// "wmin 250lp").
constexpr int kDescriptionRows     = 5;
constexpr int kDescriptionMinWidth = 250;
/// The gap before the description column (Java: "gapleft 40lp").
constexpr int kColumnGap = 40;

/// The rows of the General tab, and the row below them, which takes the extra height (Java: an
/// empty label growing in y).
constexpr int kShapeRow          = 0;
constexpr int kShapeParameterRow = 1;
constexpr int kLengthRow         = 2;
constexpr int kBaseDiameterRow   = 3;
constexpr int kAutomaticRow      = 4;
constexpr int kThicknessRow      = 5;
constexpr int kFilledRow         = 6;
constexpr int kFlipRow           = 7;
constexpr int kRowCount          = 8;

/// The columns after ConfigFields' four: a gap, the description column.
constexpr int kSliderColumn      = 3;
constexpr int kColumnCount       = 4;
constexpr int kGapColumn         = 4;
constexpr int kDescriptionColumn = 5;

/// @p selection, whose main component must be a NoseCone.
/// @throws BugError when it is not one (or the selection is null or invalid)
[[nodiscard]] std::shared_ptr<ComponentSelection> noseConeSelection(
    std::shared_ptr<ComponentSelection> selection)
{
    if (selection != nullptr && selection->isValid() &&
        dynamic_cast<const NoseCone*>(&selection->getMain()) == nullptr)
    {
        bug("The nose cone configuration needs a nose cone");
    }
    return selection;  // a null or invalid one is RocketComponentConfig's BugError
}

/// The base radius with its automatic state, whatever the orientation (Java: DoubleModel(
/// component, "BaseRadius", 2.0, UNITS_LENGTH, 0)).
[[nodiscard]] DoubleSource baseRadiusSource()
{
    return DoubleSource::of<NoseCone>({
        .get          = [](const NoseCone& nose) { return nose.getBaseRadius(); },
        .set          = [](NoseCone& nose, const double& radius) { nose.setBaseRadius(radius); },
        .isAutomatic  = [](const NoseCone& nose) { return nose.isBaseRadiusAutomatic(); },
        .setAutomatic = [](NoseCone& nose,
                           bool      automatic) { nose.setBaseRadiusAutomatic(automatic); },
    });
}

/// Whether the nose cone is a tail cone (Java: BooleanModel(component, "Flipped"), whose
/// setFlipped(boolean) is setFlipped(flipped, true)).
[[nodiscard]] BoolSource flippedSource()
{
    return BoolSource::of<NoseCone>({
        .get = [](const NoseCone& nose) { return nose.isFlipped(); },
        .set = [](NoseCone& nose, const bool& flipped) { nose.setFlipped(flipped); },
    });
}

}  // namespace

NoseConeConfig::NoseConeConfig(OpenRocketDocument& document, const GuiServices& services,
                               std::shared_ptr<ComponentSelection> selection, QWidget* parent)
  : RocketComponentConfig(document, services, noseConeSelection(std::move(selection)), parent)
{
    //: NoseConeCfg.tab.General
    insertTab(0, makeGeneralTab(), tr("General"),
              //: NoseConeCfg.tab.ttip.General
              tr("General properties"));
    // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer): after General (Java's order)
    m_shoulder = makeChild<ShoulderPanel>(this, scope());
    //: NoseConeCfg.tab.Shoulder
    insertTab(1, *m_shoulder, tr("Shoulder"),
              //: NoseConeCfg.tab.ttip.Shoulder
              tr("Shoulder properties"));
    setSelectedTabIndex(0);
}

QWidget& NoseConeConfig::makeGeneralTab()
{
    auto*                                      page        = makeChild<QWidget>(this);
    auto*                                      grid        = makeChild<QGridLayout>(page);
    BindingScope&                              bindings    = scope();
    const std::shared_ptr<ComponentSelection>& selected    = selection();
    const UnitGroup&                           lengthUnits = unitGroup(UnitGroupId::LENGTH);

    addShapeRows(*page, *grid);

    // The length.
    const auto length =
        bindings.make<DoubleBinding>(selected, BodyComponentFields::lengthSource(), lengthUnits,
                                     DoubleBinding::Options{.min = 0.0});
    m_length = ConfigFields::addNumberRow(
        *grid, kLengthRow,
        //: NoseConeCfg.lbl.Noseconelength
        tr("Length:"), length, QStringLiteral("Length"),
        {.slider = SliderMapping::fromRange(0.0, kLengthSliderMid, kLengthSliderMax)});

    // The base diameter (the radius times 2) and its automatic state.
    const auto base =
        bindings.make<DoubleBinding>(selected, baseRadiusSource(), lengthUnits,
                                     DoubleBinding::Options{.multiplier = 2.0, .min = 0.0});
    m_baseDiameter = ConfigFields::addNumberRow(
        *grid, kBaseDiameterRow,
        //: NoseConeCfg.lbl.Basediam
        tr("Base diameter:"), base, QStringLiteral("BaseRadius"),
        {.slider = SliderMapping::fromRange(0.0, kDiameterSliderMid, kDiameterSliderMax)});
    //: NoseConeCfg.checkbox.Automatic
    m_automatic = makeChild<BoolCheckBox>(page, base, tr("Automatic"));
    m_automatic->setObjectName(QStringLiteral("BaseRadiusAutomatic"));
    grid->addWidget(m_automatic, kAutomaticRow, 1, 1, 2);
    updateAutomaticCheckBox(dynamic_cast<const NoseCone&>(selected->getMain()).isFlipped());

    // The wall thickness and "Filled".
    m_thickness = BodyComponentFields::addThicknessRow(bindings, *grid, kThicknessRow,
                                                       //: NoseConeCfg.lbl.Wallthickness
                                                       tr("Wall thickness:"));
    m_filled    = makeChild<BoolCheckBox>(
        page, bindings.make<BoolBinding>(selected, BodyComponentFields::filledSource()),
        //: NoseConeCfg.checkbox.Filled
        tr("Filled"));
    m_filled->setObjectName(QStringLiteral("Filled"));
    //: NoseConeCfg.checkbox.Filled.ttip
    m_filled->setToolTip(tr("Makes this component solid instead of hollow."));
    grid->addWidget(m_filled, kFilledRow, 1, 1, 2);

    // "Flip to tail cone": "Automatic" is decided again for the new orientation.
    m_flipBinding = bindings.make<BoolBinding>(selected, flippedSource());
    //: NoseConeCfg.checkbox.Flip
    m_flip = makeChild<BoolCheckBox>(page, m_flipBinding, tr("Flip to tail cone"));
    m_flip->setObjectName(QStringLiteral("Flipped"));
    //: NoseConeCfg.checkbox.Flip.ttip
    m_flip->setToolTip(tr("Flips the nose cone direction to a tail cone."));
    grid->addWidget(m_flip, kFlipRow, 0, 1, kColumnCount);
    // Toggled comes before clicked, whose handler sets the nose cones (Java: the item listener
    // before the action).
    connect(m_flip, &QAbstractButton::toggled, this, [this](bool flipped) {
        Guard::run([this, flipped] { flipToggled(flipped); }, QStringLiteral("Flipped"));
    });

    // The description and the material, to the right.
    grid->setColumnMinimumWidth(kGapColumn, kColumnGap);
    grid->addWidget(&makeDescriptionColumn(*page), 0, kDescriptionColumn, kRowCount, 1,
                    Qt::AlignTop);
    grid->setColumnStretch(kSliderColumn, 1);
    grid->setRowStretch(kRowCount, 1);
    return *page;
}

void NoseConeConfig::addShapeRows(QWidget& page, QGridLayout& grid)
{
    //: NoseConeCfg.lbl.Noseconeshape
    grid.addWidget(makeChild<QLabel>(&page, tr("Nose cone shape:")), kShapeRow, 0);
    m_shapeBinding = scope().keep(makeTransitionShapeBinding(selection()));
    m_shape        = makeChild<EnumComboBox>(&page, m_shapeBinding);
    m_shape->setObjectName(QStringLiteral("ShapeType"));
    grid.addWidget(m_shape, kShapeRow, 1, 1, kColumnCount - 1);
    scope().keepConnection(m_shapeBinding->addChangeListener([this] { updateShape(); }));

    m_shapeParameter = BodyComponentFields::addShapeParameterRow(scope(), grid, kShapeParameterRow,
                                                                 //: NoseConeCfg.lbl.Shapeparam
                                                                 tr("Shape parameter:"));
}

QWidget& NoseConeConfig::makeDescriptionColumn(QWidget& page)
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

void NoseConeConfig::updateShape()
{
    if (m_shapeBinding == nullptr || !m_shapeBinding->isValid())
    {
        return;  // the dialog closes
    }
    const TransitionShape shape = m_shapeBinding->value();
    if (m_description != nullptr)
    {
        m_description->setDescription(
            translatedEnumText(noseConeDescriptionKey(shape), noseConeDescription(shape)));
    }
    BodyComponentFields::setShapeParameterEnabled(m_shapeParameter, usesParameter(shape));
}

void NoseConeConfig::flipToggled(bool flipped)
{
    // The user's toggle, before the binding sets it; not the binding's own refresh.
    if (m_flipBinding->isFiring() || !m_flipBinding->isValid())
    {
        return;
    }
    updateAutomaticCheckBox(flipped);
}

void NoseConeConfig::updateAutomaticCheckBox(bool flipped)
{
    const auto&               nose = dynamic_cast<const NoseCone&>(selection()->getMain());
    const SymmetricComponent* reference =
        flipped ? nose.getPreviousSymmetricComponent() : nose.getNextSymmetricComponent();
    const auto switchOff = [this] {
        selection()->apply<NoseCone>(
            [](NoseCone& selected) { selected.setBaseRadiusAutomatic(false); });
    };
    if (reference == nullptr)
    {
        BodyComponentFields::setAutomaticState(
            *m_automatic, false,
            //: NoseConeCfg.checkbox.ttip.Automatic_noReferenceComponent
            tr("There is no next component to take the diameter of."));
        switchOff();
        return;
    }
    if ((!flipped && !reference->usesPreviousCompAutomatic()) ||
        (flipped && !reference->usesNextCompAutomatic()))
    {
        BodyComponentFields::setAutomaticState(*m_automatic, true,
                                               //: NoseConeCfg.checkbox.ttip.Automatic
                                               tr("Uses the diameter of the next component."));
    }
    else
    {
        BodyComponentFields::setAutomaticState(
            *m_automatic, false,
            //: NoseConeCfg.checkbox.ttip.Automatic_alreadyAuto
            tr("The next component already has its auto setting turned on."));
        switchOff();
    }
}

}  // namespace QtRocket::Gui
