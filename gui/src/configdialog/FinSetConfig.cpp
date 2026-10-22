#include "QtRocket/gui/configdialog/FinSetConfig.h"

#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QObject>
#include <QPointer>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/binding/IntBinding.h"
#include "QtRocket/gui/binding/MaterialBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/FinTabCalculator.h"
#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/configdialog/PlacementPanel.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/IntSpinBox.h"
#include "QtRocket/gui/widgets/MaterialComboBox.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

/// The bounds of the fin count (Java: IntegerModel(component, "FinCount", 1, 8)).
constexpr int kMinFinCount = 1;
constexpr int kMaxFinCount = 8;
/// The thickness slider: 0 to 1 cm (Java: getSliderModel(0, 0.01)).
constexpr double kThicknessSliderMax = 0.01;
/// The fillet radius: steps of 0.1 in the unit, slider 0 to 10 cm (Java: getSpinnerModel(0.1),
/// getSliderModel(0, 0.1)).
constexpr double kFilletRadiusStep      = 0.1;
constexpr double kFilletRadiusSliderMax = 0.1;
/// The multipliers of the tab offset slider's bounds: minus and plus half the fin's length
/// (Java: DoubleModel(component, "Length", -0.5 and 0.5, UNITS_LENGTH, 0)).
constexpr double kHalfLength = 0.5;

/// The rows of the tab "Fin tabs" (Java: "wrap 30lp" after the heading).
constexpr int kHeadingRow    = 0;
constexpr int kTabLengthRow  = 1;
constexpr int kTabHeightRow  = 2;
constexpr int kTabOffsetRow  = 3;
constexpr int kTabMethodRow  = 4;
constexpr int kAutoCalcRow   = 5;
constexpr int kFinTabRows    = 6;
constexpr int kHeadingGap    = 30;
constexpr int kFinTabMargins = 25;
/// The columns of ConfigFields and the span of the combo box after its label.
constexpr int kSpinColumn   = 1;
constexpr int kSliderColumn = 3;
constexpr int kAfterLabel   = 3;
constexpr int kColumnCount  = 4;

/// @p selection, whose main component must be a fin set.
/// @throws BugError when it is not one (or the selection is null or invalid)
[[nodiscard]] std::shared_ptr<ComponentSelection> finSetSelection(
    std::shared_ptr<ComponentSelection> selection)
{
    if (selection != nullptr && selection->isValid() &&
        dynamic_cast<const FinSet*>(&selection->getMain()) == nullptr)
    {
        bug("The fin set configuration needs a fin set");
    }
    return selection;  // a null or invalid one is RocketComponentConfig's BugError
}

/// Runs @p change inside one freeze of @p rocket (taken unless it is frozen already), thawed also
/// when @p change throws.
template <class F>
void inOneFreeze(Rocket& rocket, const F& change)
{
    const bool freeze = !rocket.isFrozen();
    if (freeze)
    {
        rocket.freeze();
    }
    try
    {
        change();
    }
    catch (...)
    {
        if (freeze)
        {
            rocket.thaw();
        }
        throw;
    }
    if (freeze)
    {
        rocket.thaw();
    }
}

/// The fin sets of @p rocket with the ids @p ids, those still in it, in that order.
[[nodiscard]] std::vector<FinSet*> finSetsWithIds(Rocket& rocket, std::span<const Uuid> ids)
{
    std::vector<FinSet*> found;
    for (const Uuid& id : ids)
    {
        if (auto* fins = dynamic_cast<FinSet*>(rocket.findComponent(id)))
        {
            found.push_back(fins);
        }
    }
    return found;
}

/// The ids of the main component of @p selection and of the others that @p take accepts.
template <class Take>
[[nodiscard]] std::vector<Uuid> selectedIds(const ComponentSelection& selection, const Take& take)
{
    std::vector<Uuid> ids{selection.getMain().getId()};
    for (const RocketComponent* other : selection.getOthers())
    {
        if (take(*other))
        {
            ids.push_back(other->getId());
        }
    }
    return ids;
}

/// The fields "Calculate automatically" sets (Java: its models).
struct AutoTabFields
{
    std::shared_ptr<DoubleBinding>            length;
    std::shared_ptr<DoubleBinding>            height;
    std::shared_ptr<DoubleBinding>            offset;
    std::shared_ptr<EnumBinding<AxialMethod>> method;
};

/// Sets the tab through @p fields, in Java's order (calculateAutoTab()): the method TOP, the offset
/// and the length of @p placement and the method again as it was, or without a placement the
/// method TOP, the offset 0 and the length @p finLength; then, from the tab as it now lies on the
/// main fin set of @p selection, the height down to @p maxTubeRadius, in the default length unit.
void applyAutoTab(const AutoTabFields& fields, const ComponentSelection& selection,
                  const std::optional<FinTabCalculator::TabPlacement>& placement, double finLength,
                  double maxTubeRadius)
{
    // Figure out position and length of the fin tab
    if (placement.has_value())
    {
        const AxialMethod temp = fields.method->value();
        fields.method->select(AxialMethod::TOP);
        fields.offset->setValue(placement->offset);
        fields.length->setValue(placement->length);
        // Be nice to the user and set the tab relative position enum back the way they had it.
        fields.method->select(temp);
    }
    else
    {
        fields.method->select(AxialMethod::TOP);
        fields.offset->setValue(0);
        fields.length->setValue(finLength);
    }
    if (!selection.isValid())
    {
        return;
    }
    // Compute tab height
    const std::optional<double> height = FinTabCalculator::tabHeight(
        dynamic_cast<const FinSet&>(selection.getMain()), maxTubeRadius);
    // Set tab height
    if (height.has_value())
    {
        fields.height->setValue(*height);
        fields.height->setCurrentUnit(unitGroup(UnitGroupId::LENGTH).getDefaultUnit());
    }
}

}  // namespace

// ---------------------------------------------------------------------------------- sources

IntSource FinSetConfig::finCountSource()
{
    return IntSource::of<FinSet>({
        .get = [](const FinSet& fins) { return fins.getFinCount(); },
        .set = [](FinSet& fins, const int& count) { fins.setFinCount(count); },
    });
}

DoubleSource FinSetConfig::cantAngleSource()
{
    return DoubleSource::of<FinSet>({
        .get = [](const FinSet& fins) { return fins.getCantAngle(); },
        .set = [](FinSet& fins, const double& angle) { fins.setCantAngle(angle); },
    });
}

EnumSource<FinCrossSection> FinSetConfig::crossSectionSource()
{
    return EnumSource<FinCrossSection>::of<FinSet>({
        .get = [](const FinSet& fins) { return fins.getCrossSection(); },
        .set = [](FinSet& fins, const FinCrossSection& section) { fins.setCrossSection(section); },
    });
}

DoubleSource FinSetConfig::thicknessSource()
{
    return DoubleSource::of<FinSet>({
        .get = [](const FinSet& fins) { return fins.getThickness(); },
        .set = [](FinSet& fins, const double& thickness) { fins.setThickness(thickness); },
    });
}

DoubleSource FinSetConfig::baseRotationSource()
{
    return DoubleSource::of<FinSet>({
        .get = [](const FinSet& fins) { return fins.getBaseRotation(); },
        .set = [](FinSet& fins, const double& angle) { fins.setBaseRotation(angle); },
    });
}

DoubleSource FinSetConfig::finLengthSource()
{
    return DoubleSource::of<FinSet>({
        .get = [](const FinSet& fins) { return fins.getLength(); },
    });
}

DoubleSource FinSetConfig::tabLengthSource()
{
    return DoubleSource::of<FinSet>({
        .get = [](const FinSet& fins) { return fins.getTabLength(); },
        .set = [](FinSet& fins, const double& length) { fins.setTabLength(length); },
    });
}

DoubleSource FinSetConfig::tabHeightSource()
{
    return DoubleSource::of<FinSet>({
        .get = [](const FinSet& fins) { return fins.getTabHeight(); },
        .set = [](FinSet& fins, const double& height) { fins.setTabHeight(height); },
    });
}

DoubleSource FinSetConfig::tabOffsetSource()
{
    return DoubleSource::of<FinSet>({
        .get = [](const FinSet& fins) { return fins.getTabOffset(); },
        .set = [](FinSet& fins, const double& offset) { fins.setTabOffset(offset); },
    });
}

DoubleSource FinSetConfig::maxTabHeightSource()
{
    return DoubleSource::of<FinSet>({
        .get = [](const FinSet& fins) { return fins.getMaxTabHeight(); },
    });
}

EnumSource<AxialMethod> FinSetConfig::tabOffsetMethodSource()
{
    return EnumSource<AxialMethod>::of<FinSet>({
        .get = [](const FinSet& fins) { return fins.getTabOffsetMethod(); },
        .set = [](FinSet& fins, const AxialMethod& method) { fins.setTabOffsetMethod(method); },
    });
}

DoubleSource FinSetConfig::filletRadiusSource()
{
    return DoubleSource::of<FinSet>({
        .get = [](const FinSet& fins) { return fins.getFilletRadius(); },
        .set = [](FinSet& fins, const double& radius) { fins.setFilletRadius(radius); },
    });
}

MaterialSource FinSetConfig::filletMaterialSource()
{
    return MaterialSource::of<FinSet>({
        .get = [](const FinSet& fins) { return fins.getFilletMaterial(); },
        .set = [](FinSet& fins, const Material& material) { fins.setFilletMaterial(material); },
    });
}

// ---------------------------------------------------------------------------------- the panel

FinSetConfig::FinSetConfig(OpenRocketDocument& document, const GuiServices& services,
                           std::shared_ptr<ComponentSelection> selection, QWidget* parent)
  : RocketComponentConfig(document, services, finSetSelection(std::move(selection)), parent)
{
    //: FinSetConfig.tab.Fintabs
    insertTab(0, makeFinTabTab(), tr("Fin tabs"),
              //: FinSetConfig.tab.Through-the-wall
              tr("Through-the-wall fin tabs"));
}

QWidget& FinSetConfig::makeFinTabTab()
{
    auto* page = makeChild<QWidget>(this);
    auto* grid = makeChild<QGridLayout>(page);
    grid->setContentsMargins(kFinTabMargins, kFinTabMargins, kFinTabMargins, kFinTabMargins);
    BindingScope&                              bindings    = scope();
    const std::shared_ptr<ComponentSelection>& selected    = selection();
    const UnitGroup&                           lengthUnits = unitGroup(UnitGroupId::LENGTH);

    // The heading over the columns, with a gap below it (Java: "spanx, wrap 30lp").
    auto* heading =
        makeChild<StyledLabel>(page,
                               //: FinSetConfig.lbl.Through-the-wall
                               tr("Through-the-wall fin tabs:"), StyledLabel::Style::BOLD);
    heading->setContentsMargins(0, 0, 0, kHeadingGap);
    grid->addWidget(heading, kHeadingRow, 0, 1, kColumnCount);

    // The fin's length and the largest tab height, which the sliders follow (Java: the models
    // "Length" times 1, 0.5 and -0.5, and "MaxTabHeight").
    const auto length = bindings.make<DoubleBinding>(selected, finLengthSource(), lengthUnits,
                                                     DoubleBinding::Options{.min = 0.0});
    const auto halfLength =
        bindings.make<DoubleBinding>(selected, finLengthSource(), lengthUnits,
                                     DoubleBinding::Options{.multiplier = kHalfLength, .min = 0.0});
    const auto minusHalfLength = bindings.make<DoubleBinding>(
        selected, finLengthSource(), lengthUnits,
        DoubleBinding::Options{.multiplier = -kHalfLength, .min = 0.0});
    const auto maxTabHeight = bindings.make<DoubleBinding>(
        selected, maxTabHeightSource(), lengthUnits, DoubleBinding::Options{.min = 0.0});

    // The tab length, its slider from 0 to the fin's length.
    const auto tabLength = bindings.make<DoubleBinding>(selected, tabLengthSource(), lengthUnits,
                                                        DoubleBinding::Options{.min = 0.0});
    m_tabLength =
        ConfigFields::addNumberRow(*grid, kTabLengthRow,
                                   //: FinSetConfig.lbl.Tablength
                                   tr("Tab length:"), tabLength, QStringLiteral("TabLength"),
                                   {.slider = SliderMapping::linear(0.0, length->value()),
                                    .bounds = {.min = nullptr, .max = length}});
    //: FinSetConfig.ttip.Tablength
    m_tabLength.label->setToolTip(tr("The length of the fin tab."));

    // The tab height, at most the largest one when the panel is made; its slider up to the largest
    // one as it changes.
    const auto& main      = dynamic_cast<const FinSet&>(selected->getMain());
    const auto  tabHeight = bindings.make<DoubleBinding>(
        selected, tabHeightSource(), lengthUnits,
        DoubleBinding::Options{.min = 0.0, .max = main.getMaxTabHeight()});
    m_tabHeight =
        ConfigFields::addNumberRow(*grid, kTabHeightRow,
                                   //: FinSetConfig.lbl.Tabheight
                                   tr("Tab height:"), tabHeight, QStringLiteral("TabHeight"),
                                   {.slider = SliderMapping::linear(0.0, maxTabHeight->value()),
                                    .bounds = {.min = nullptr, .max = maxTabHeight}});
    //: FinSetConfig.ttip.Tabheight
    m_tabHeight.label->setToolTip(tr("The spanwise height of the fin tab."));

    // The tab position, its slider from minus to plus half the fin's length.
    const auto tabOffset = bindings.make<DoubleBinding>(selected, tabOffsetSource(), lengthUnits,
                                                        DoubleBinding::Options{});
    m_tabOffset          = ConfigFields::addNumberRow(
        *grid, kTabOffsetRow,
        //: FinSetConfig.lbl.Tabposition
        tr("Tab position:"), tabOffset, QStringLiteral("TabOffset"),
        {.slider = SliderMapping::linear(minusHalfLength->value(), halfLength->value()),
         .bounds = {.min = minusHalfLength, .max = halfLength}});
    //: FinSetConfig.ttip.Tabposition
    m_tabOffset.label->setToolTip(tr("The position of the fin tab."));

    // The tab offset method: every AxialMethod (Java: EnumModel without a list of values).
    //: FinSetConfig.lbl.relativeto
    grid->addWidget(makeChild<QLabel>(page, tr("relative to")), kTabMethodRow, 0,
                    Qt::AlignRight | Qt::AlignVCenter);
    m_tabOffsetMethod = makeChild<EnumComboBox>(
        page,
        bindings.make<EnumBinding<AxialMethod>>(selected, tabOffsetMethodSource(),
                                                std::span<const AxialMethod>(kAllAxialMethods)));
    m_tabOffsetMethod->setObjectName(QStringLiteral("TabOffsetMethod"));
    grid->addWidget(m_tabOffsetMethod, kTabMethodRow, kSpinColumn, 1, kAfterLabel);

    // Calculate fin tab height, length, and position
    //: FinSetConfig.but.AutoCalc
    m_autoCalc = makeChild<QPushButton>(page, tr("Calculate automatically"));
    m_autoCalc->setObjectName(QStringLiteral("autoCalcButton"));
    m_autoCalc->setAutoDefault(false);
    //: FinSetConfig.but.AutoCalc.ttip
    m_autoCalc->setToolTip(
        tr("<html>Calculates the height and length of the fin tabs, based on "
           "<br>inner tube and centering ring components of the fin's parent "
           "component.</html>"));
    grid->addWidget(m_autoCalc, kAutoCalcRow, kSpinColumn, 1, kAfterLabel, Qt::AlignLeft);
    connect(m_autoCalc, &QPushButton::clicked, this,
            guarded([this] { calculateAutoTab(); }, QStringLiteral("autoCalcButton")));

    grid->setColumnStretch(kSliderColumn, 1);
    grid->setRowStretch(kFinTabRows, 1);
    return *page;
}

void FinSetConfig::addFinCountRow(QWidget& page, QGridLayout& grid, int row, const QString& label,
                                  const QString& toolTip)
{
    auto* text = makeChild<QLabel>(&page, label);
    text->setToolTip(toolTip);
    grid.addWidget(text, row, 0);
    m_finCount = makeChild<IntSpinBox>(
        &page, scope().make<IntBinding>(selection(), finCountSource(), kMinFinCount, kMaxFinCount));
    m_finCount->setObjectName(QStringLiteral("FinCount"));
    m_finCount->setToolTip(toolTip);
    grid.addWidget(m_finCount, row, kSpinColumn);
}

void FinSetConfig::addCantRow(QGridLayout& grid, int row, const QString& label,
                              const QString& toolTip)
{
    const auto cant = scope().make<DoubleBinding>(
        selection(), cantAngleSource(), unitGroup(UnitGroupId::ANGLE),
        DoubleBinding::Options{.min = -FinSet::kMaxCantRadians, .max = FinSet::kMaxCantRadians});
    m_cant = ConfigFields::addNumberRow(
        grid, row, label, cant, QStringLiteral("CantAngle"),
        {.slider = SliderMapping::linear(-FinSet::kMaxCantRadians, FinSet::kMaxCantRadians)});
    m_cant.label->setToolTip(toolTip);
}

void FinSetConfig::addCrossSectionRow(QWidget& page, QGridLayout& grid, int row,
                                      const QString& label)
{
    grid.addWidget(makeChild<QLabel>(&page, label), row, 0);
    m_crossSection =
        makeChild<EnumComboBox>(&page, scope().make<EnumBinding<FinCrossSection>>(
                                           selection(), crossSectionSource(),
                                           std::span<const FinCrossSection>(kAllFinCrossSections)));
    m_crossSection->setObjectName(QStringLiteral("CrossSection"));
    grid.addWidget(m_crossSection, row, kSpinColumn, 1, kAfterLabel);
}

void FinSetConfig::addThicknessRow(QGridLayout& grid, int row, const QString& label)
{
    const auto thickness =
        scope().make<DoubleBinding>(selection(), thicknessSource(), unitGroup(UnitGroupId::LENGTH),
                                    DoubleBinding::Options{.min = 0.0});
    m_thickness =
        ConfigFields::addNumberRow(grid, row, label, thickness, QStringLiteral("Thickness"),
                                   {.slider = SliderMapping::linear(0.0, kThicknessSliderMax)});
}

QWidget& FinSetConfig::makeRightColumn(QWidget& page, bool boundedRotation)
{
    auto* column = makeChild<QWidget>(&page);
    auto* layout = makeChild<QVBoxLayout>(column);
    layout->setContentsMargins(0, 0, 0, 0);

    // -------- Placement -------
    m_placement = makeChild<PlacementPanel>(column, scope());
    layout->addWidget(m_placement);
    // Fin rotation (Java: placementPanel.add()): TrapezoidFinSetConfig bounds the field to -pi
    // ... pi, the other fin set panels leave it unbounded; the slider is -pi ... pi in all.
    DoubleBinding::Options rotationBounds{};
    if (boundedRotation)
    {
        rotationBounds = DoubleBinding::Options{.min = -std::numbers::pi, .max = std::numbers::pi};
    }
    const auto rotation = scope().make<DoubleBinding>(
        selection(), baseRotationSource(), unitGroup(UnitGroupId::ANGLE), rotationBounds);
    m_baseRotation = ConfigFields::addNumberRow(
        m_placement->grid(), m_placement->nextRow(),
        //: FinSetCfg.lbl.FinRotation
        tr("Fin rotation:"), rotation, QStringLiteral("BaseRotation"),
        {.slider = SliderMapping::linear(-std::numbers::pi, std::numbers::pi)});
    //: FinSetCfg.lbl.FinRotation.ttip
    m_baseRotation.label->setToolTip(tr("The angle of the first fin in the fin set."));

    // Material
    m_material = makeChild<MaterialPanel>(column, scope(), document(), services().materials(),
                                          Material::Type::BULK);
    layout->addWidget(m_material);

    // Root fillets
    layout->addWidget(&makeFilletPanel(*column));
    layout->addStretch(1);
    return *column;
}

QGroupBox& FinSetConfig::makeFilletPanel(QWidget& parent)
{
    //: FinSetConfig.title.RootFillets
    m_filletPanel = makeChild<QGroupBox>(&parent, tr("Root Fillets"));
    m_filletPanel->setObjectName(QStringLiteral("filletPanel"));
    auto* grid = makeChild<QGridLayout>(m_filletPanel);
    //: FinsetConfig.ttip.Finfillets1
    const QString tip = tr("<HTML>Adds the predicted mass of fin fillets to the root of the fins. "
                           "<br>") +
                        //: FinsetConfig.ttip.Finfillets2
                        tr("Assumes the fillet is concave and tangent to the body tube and fin. "
                           "<br>") +
                        //: FinsetConfig.ttip.Finfillets3
                        tr("Zero radius will give no fillet.  ");

    // Fillet Radius:
    const auto radius = scope().make<DoubleBinding>(selection(), filletRadiusSource(),
                                                    unitGroup(UnitGroupId::LENGTH),
                                                    DoubleBinding::Options{.min = 0.0});
    m_filletRadius =
        ConfigFields::addNumberRow(*grid, 0,
                                   //: FinSetConfig.lbl.Filletradius
                                   tr("Fillet radius:"), radius, QStringLiteral("FilletRadius"),
                                   {.slider = SliderMapping::linear(0.0, kFilletRadiusSliderMax)});
    m_filletRadius.spinBox->setFixedStep(kFilletRadiusStep);
    m_filletRadius.spinBox->setToolTip(tip);
    m_filletRadius.unitSelector->setToolTip(tip);
    m_filletRadius.slider->setToolTip(tip);

    // Fillet Material: (Java sets the fillets' tooltip on the label, then replaces it)
    //: FinSetConfig.lbl.Finfilletmaterial
    auto* label = makeChild<QLabel>(m_filletPanel, tr("Fillet material:"));
    //: MaterialPanel.lbl.ttip.ComponentMaterialAffects
    label->setToolTip(tr("The component material affects the weight of the component."));
    grid->addWidget(label, 1, 0, 1, kColumnCount);

    // Material selection combo box
    m_filletMaterial = makeChild<MaterialComboBox>(
        m_filletPanel,
        scope().make<MaterialBinding>(selection(), filletMaterialSource(), Material::Type::BULK,
                                      services().materials(), document()));
    m_filletMaterial->setObjectName(QStringLiteral("FilletMaterial"));
    //: MaterialPanel.combo.ttip.ComponentMaterialAffects
    m_filletMaterial->setToolTip(tr("The component material affects the weight of the component."));
    grid->addWidget(m_filletMaterial, 2, 0, 1, kColumnCount);
    m_filletPanel->setToolTip(tip);
    grid->setColumnStretch(kSliderColumn, 1);
    return *m_filletPanel;
}

void FinSetConfig::addFinSetButtons()
{
    // Convert buttons
    if (dynamic_cast<const FreeformFinSet*>(&selection()->getMain()) == nullptr)
    {
        //: FinSetConfig.but.Converttofreeform
        m_convert = makeChild<QPushButton>(this, tr("Convert to freeform"));
        m_convert->setObjectName(QStringLiteral("convertButton"));
        //: FinSetConfig.but.Converttofreeform.ttip
        m_convert->setToolTip(tr("Convert this fin set into a freeform fin set"));
        connect(m_convert, &QPushButton::clicked, this,
                guarded([this] { convertToFreeform(); }, QStringLiteral("convertButton")));
    }

    // Split fins
    //: FinSetConfig.but.Splitfins
    m_split = makeChild<QPushButton>(this, tr("Split fins"));
    m_split->setObjectName(QStringLiteral("splitButton"));
    //: FinSetConfig.but.Splitfins.ttip
    m_split->setToolTip(tr("Split the fin set into separate fins."));
    connect(m_split, &QPushButton::clicked, this,
            guarded([this] { splitFins(); }, QStringLiteral("splitButton")));
    m_split->setEnabled(dynamic_cast<const FinSet&>(selection()->getMain()).getFinCount() > 1);

    addButton(*m_split);
    if (m_convert != nullptr)
    {
        addButton(*m_convert);
    }
}

void FinSetConfig::updateFields()
{
    RocketComponentConfig::updateFields();
    const std::shared_ptr<ComponentSelection>& selected = selection();
    if (m_split != nullptr && selected != nullptr && selected->isValid())
    {
        m_split->setEnabled(dynamic_cast<const FinSet&>(selected->getMain()).getFinCount() > 1);
    }
}

// ---------------------------------------------------------------------------------- actions

void FinSetConfig::calculateAutoTab()
{
    // A copy: a field's change may end the panel's bindings.
    const std::shared_ptr<ComponentSelection> selected = selection();
    if (selected == nullptr || !selected->isValid())
    {
        return;  // the dialog is going
    }
    const auto& main = dynamic_cast<const FinSet&>(selected->getMain());
    if (dynamic_cast<const SymmetricComponent*>(main.getParent()) == nullptr)
    {
        return;
    }
    // What the calculation reads of the main fin set, read before the fields change anything
    // (Java reads the fin's position and length once the tab's method is TOP, which changes
    // neither): the inner tubes and centering rings of the parent, and the tab between the rings.
    const FinTabCalculator::TabSupports supports = FinTabCalculator::findTabSupports(main);
    std::optional<FinTabCalculator::TabPlacement> placement;
    if (supports.ringsPlaceTheTab())
    {
        placement = FinTabCalculator::computeFinTabLength(
            supports.rings, main.getAxialOffset(AxialMethod::TOP), main.getLength());
    }
    const AutoTabFields fields{
        .length = m_tabLength.spinBox->binding(),
        .height = m_tabHeight.spinBox->binding(),
        .offset = m_tabOffset.spinBox->binding(),
        .method = m_tabOffsetMethod->enumBinding<AxialMethod>(),
    };
    const double finLength = main.getLength();

    //: Not translated in OpenRocket (FinSetConfig.java): the undo step of "Calculate automatically"
    document().startUndo(QtStrings::toUtf8(tr("Compute fin tabs")));
    try
    {
        applyAutoTab(fields, *selected, placement, finLength, supports.maxTubeRadius);
    }
    catch (...)
    {
        document().stopUndo();
        throw;
    }
    document().stopUndo();
}

QWidget* FinSetConfig::closeForStructuralChange()
{
    const auto* const dialog = qobject_cast<const ComponentConfigDialog*>(window());
    if (dialog != nullptr && dialog == ComponentConfigDialog::dialog())
    {
        QWidget* const owner = dialog->parentWidget();
        ComponentConfigDialog::disposeDialog();
        return owner;
    }
    // In no open dialog (a panel on its own): as the dialog's closing, commit, then end the
    // bindings.
    commitAll();
    release();
    return nullptr;
}

void FinSetConfig::convertToFreeform()
{
    const std::shared_ptr<ComponentSelection>& selected = selection();
    if (selected == nullptr || !selected->isValid())
    {
        return;  // the dialog is going
    }
    // The fin sets by id: the dialog's closing commits its fields.
    const std::vector<Uuid> ids    = selectedIds(*selected, [](const RocketComponent& other) {
        return dynamic_cast<const FinSet*>(&other) != nullptr;
    });
    OpenRocketDocument&     design = document();
    const GuiServices       gui    = services();
    const QPointer<QWidget> owner(closeForStructuralChange());

    // Convert fin set
    design.addUndoPosition(QtStrings::toUtf8(
        //: FinSetConfig.Convertfinset
        tr("Convert fin set")));
    Rocket&                                       rocket  = design.getRocket();
    const std::vector<FinSet*>                    finSets = finSetsWithIds(rocket, ids);
    std::vector<FreeformFinSet::ConversionResult> converted;
    inOneFreeze(rocket, [&] {
        for (FinSet* fins : finSets)
        {
            converted.push_back(FreeformFinSet::convertFinSet(*fins, false));
        }
    });
    // The replaced fin sets go now that the event has been delivered; the new ones keep the ids.
    converted.clear();

    if (owner.isNull() || ids.empty())
    {
        return;
    }
    const std::vector<FinSet*> freeforms = finSetsWithIds(rocket, ids);
    if (freeforms.empty() || freeforms.front()->getId() != ids.front())
    {
        return;
    }
    const std::vector<RocketComponent*> others(freeforms.begin() + 1, freeforms.end());
    ComponentConfigDialog::showDialog(*owner, gui, design, *freeforms.front(), others);
}

void FinSetConfig::splitFins()
{
    const std::shared_ptr<ComponentSelection>& selected = selection();
    if (selected == nullptr || !selected->isValid())
    {
        return;  // the dialog is going
    }
    // The fin sets of the main one's class (Java: listener.getClass().isAssignableFrom(this's)).
    const ComponentKind     kind = selected->getMain().kind();
    const std::vector<Uuid> ids  = selectedIds(
        *selected, [kind](const RocketComponent& other) { return other.kind() == kind; });
    OpenRocketDocument& design = document();
    static_cast<void>(closeForStructuralChange());

    //: Not translated in OpenRocket (FinSetConfig.java): the undo step of "Split fins"
    design.addUndoPosition(QtStrings::toUtf8(tr("Split fin set")));
    Rocket&                                   rocket  = design.getRocket();
    const std::vector<FinSet*>                finSets = finSetsWithIds(rocket, ids);
    std::vector<RocketComponent::SplitResult> split;
    inOneFreeze(rocket, [&] {
        for (FinSet* fins : finSets)
        {
            split.push_back(fins->splitFins(false));
        }
    });
    // The replaced fin sets go now that the event has been delivered.
    split.clear();
}

}  // namespace QtRocket::Gui
