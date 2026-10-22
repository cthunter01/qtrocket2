#include "QtRocket/gui/configdialog/ParachuteConfig.h"

#include <memory>
#include <utility>

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/IntBinding.h"
#include "QtRocket/gui/binding/MaterialBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/MassObjectFields.h"
#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/configdialog/PlacementPanel.h"
#include "QtRocket/gui/configdialog/RadialPositionPanel.h"
#include "QtRocket/gui/configdialog/RecoveryDeviceConfig.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/HtmlLabel.h"
#include "QtRocket/gui/widgets/IntSpinBox.h"
#include "QtRocket/gui/widgets/MaterialComboBox.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The selection of @p panel, which must be valid and edit a parachute.
[[nodiscard]] const std::shared_ptr<ComponentSelection>& parachuteSelection(
    const RocketComponentConfig& panel)
{
    const std::shared_ptr<ComponentSelection>& selection = panel.selection();
    if (selection == nullptr || !selection->isValid())
    {
        bug("The configuration of a parachute needs a valid selection");
    }
    if (dynamic_cast<const Parachute*>(&selection->getMain()) == nullptr)
    {
        bug("The configuration of a parachute needs a parachute");
    }
    return selection;
}

}  // namespace

DoubleSource ParachuteConfig::diameterSource()
{
    return DoubleSource::of<Parachute>({
        .get = [](const Parachute& parachute) { return parachute.getDiameter(); },
        .set = [](Parachute&    parachute,
                  const double& diameter) { parachute.setDiameter(diameter); },
    });
}

IntSource ParachuteConfig::lineCountSource()
{
    return IntSource::of<Parachute>({
        .get = [](const Parachute& parachute) { return parachute.getLineCount(); },
        .set = [](Parachute& parachute, const int& count) { parachute.setLineCount(count); },
    });
}

DoubleSource ParachuteConfig::lineLengthSource()
{
    return DoubleSource::of<Parachute>({
        .get = [](const Parachute& parachute) { return parachute.getLineLength(); },
        .set = [](Parachute& parachute, const double& length) { parachute.setLineLength(length); },
        .isAutomatic = [](const Parachute& parachute) { return parachute.isLineLengthAutomatic(); },
        .setAutomatic = [](Parachute& parachute,
                           bool       automatic) { parachute.setLineLengthAutomatic(automatic); },
    });
}

MaterialSource ParachuteConfig::lineMaterialSource()
{
    return MaterialSource::of<Parachute>({
        .get = [](const Parachute& parachute) { return parachute.getLineMaterial(); },
        .set = [](Parachute&      parachute,
                  const Material& material) { parachute.setLineMaterial(material); },
    });
}

QString ParachuteConfig::resetCdToolTip()
{
    // Java: String.format() of the key's "%.2f", in the default locale's notation.
    //: ParachuteCfg.but.ResetCd.ttip (%1: the default CD with two decimals)
    return tr("Resets the drag coefficient (C<sub>D</sub>) to a value of %1.")
        .arg(QLocale().toString(Parachute::kDefaultCd, 'f', 2));
}

ParachuteConfig::ParachuteConfig(OpenRocketDocument& document, const GuiServices& services,
                                 std::shared_ptr<ComponentSelection> selection, QWidget* parent)
  : RecoveryDeviceConfig(document, services, std::move(selection), parent)
{
    BindingScope& bindings = scope();
    static_cast<void>(parachuteSelection(*this));

    auto* page       = makeChild<QWidget>(this);
    auto* primary    = makeChild<QHBoxLayout>(page);
    auto* left       = makeChild<QWidget>(page);
    auto* leftLayout = makeChild<QVBoxLayout>(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    primary->addWidget(left, 0, Qt::AlignTop);
    leftLayout->addWidget(&addCanopyGroup(*left));
    leftLayout->addWidget(&addShroudGroup(*left));
    leftLayout->addStretch(1);

    // The right side: the placement with the packed size, then the deployment.
    auto* right       = makeChild<QWidget>(page);
    auto* rightLayout = makeChild<QVBoxLayout>(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    m_placement = makeChild<PlacementPanel>(right, bindings);
    m_size = MassObjectFields::addSizeRows(bindings, m_placement->grid(), m_placement->nextRow(),
                                           //: ParachuteCfg.lbl.Packedlength
                                           tr("Packed length:"),
                                           //: ParachuteCfg.lbl.Packeddiam
                                           tr("Packed diameter:"));
    rightLayout->addWidget(m_placement);
    rightLayout->addWidget(&makeDeploymentPanel(*right));
    rightLayout->addStretch(1);
    primary->addWidget(right, 0, Qt::AlignTop);
    primary->addStretch(1);

    updateFields();

    insertTab(0, *page,
              //: ParachuteCfg.tab.General
              tr("General"),
              //: ParachuteCfg.tab.ttip.General
              tr("General properties"));
    const RadialPositionPanel& radial = RadialPositionPanel::addTab(*this);
    // Java's slip, kept: the key itself is the tooltip (see the class comment).
    radial.resetButton().setToolTip(QStringLiteral("ParachuteCfg.but.ResetRadial.ttip"));
    // Java: MigLayout "ins 25lp".
    radial.grid().setContentsMargins(25, 25, 25, 25);
    setSelectedTabIndex(0);
}

QGroupBox& ParachuteConfig::addCanopyGroup(QWidget& left)
{
    const std::shared_ptr<ComponentSelection>& valid    = parachuteSelection(*this);
    BindingScope&                              bindings = scope();
    //: ParachuteCfg.lbl.Canopy
    m_canopy   = makeChild<QGroupBox>(&left, tr("Canopy"));
    auto* grid = makeChild<QGridLayout>(m_canopy);

    m_diameter = ConfigFields::addNumberRow(
        *grid, 0,
        //: ParachuteCfg.lbl.Diameter
        tr("Diameter:"),
        bindings.make<DoubleBinding>(valid, diameterSource(), unitGroup(UnitGroupId::LENGTH),
                                     DoubleBinding::Options{.min = 0.0}),
        QStringLiteral("Diameter"), {.slider = SliderMapping::fromRange(0, 0.4, 1.5)});

    // The surface material: a label above its combo box (Java: MaterialModel and its combo box,
    // no MaterialPanel).
    //: ParachuteCfg.lbl.Material
    grid->addWidget(makeChild<QLabel>(m_canopy, tr("Material:")), 1, 0);
    m_material = makeChild<MaterialComboBox>(
        m_canopy, bindings.make<MaterialBinding>(valid, MaterialPanel::componentMaterialSource(),
                                                 Material::Type::SURFACE, services().materials(),
                                                 document()));
    m_material->setObjectName(QStringLiteral("Material"));
    //: ParachuteCfg.combo.MaterialModel
    m_material->setToolTip(tr("The component material affects the weight of the component."));
    grid->addWidget(m_material, 2, 0, 1, 4);

    // The drag coefficient: no unit selector, no slider, and a Reset.
    //: ParachuteCfg.lbl.longB1
    const QString tip = tr("<html>The drag coefficient relative to the total area of the "
                           "parachute. <br>") +
                        //: ParachuteCfg.lbl.longB2
                        tr("A larger drag coefficient yields a slowed descent rate.") +
                        QStringLiteral("  ") +
                        //: ParachuteCfg.lbl.longB3
                        tr("A typical value for parachutes is 0.8.");
    //: ParachuteCfg.lbl.longA1
    m_cdLabel = makeChild<HtmlLabel>(m_canopy, tr("<html>Drag coefficient C<sub>D</sub>:"));
    m_cdLabel->setToolTip(tip);
    grid->addWidget(m_cdLabel, 3, 0);
    m_cd = ConfigFields::addNumberRow(
        *grid, 3, QString(),
        bindings.make<DoubleBinding>(valid, cdSource(), unitGroup(UnitGroupId::COEFFICIENT),
                                     DoubleBinding::Options{.min = 0.0}),
        QStringLiteral("CD"), {.unitSelector = false});
    m_cd.label = m_cdLabel;
    m_cd.spinBox->setToolTip(tip);
    //: ParachuteCfg.but.Reset
    m_resetCd = makeChild<QPushButton>(m_canopy, tr("Reset"));
    m_resetCd->setObjectName(QStringLiteral("resetCdButton"));
    m_resetCd->setToolTip(resetCdToolTip());
    connect(m_resetCd, &QPushButton::clicked, this,
            guarded([this] { resetCd(); }, QStringLiteral("resetCdButton")));
    grid->addWidget(m_resetCd, 3, 2, 1, 2, Qt::AlignLeft);
    grid->setColumnStretch(3, 1);
    return *m_canopy;
}

QGroupBox& ParachuteConfig::addShroudGroup(QWidget& left)
{
    const std::shared_ptr<ComponentSelection>& valid    = parachuteSelection(*this);
    BindingScope&                              bindings = scope();
    //: ParachuteCfg.lbl.Shroudlines
    m_shroud   = makeChild<QGroupBox>(&left, tr("Shroud lines"));
    auto* grid = makeChild<QGridLayout>(m_shroud);

    // The number of lines, at least 0.
    //: ParachuteCfg.lbl.Numberoflines
    grid->addWidget(makeChild<QLabel>(m_shroud, tr("Number of lines:")), 0, 0);
    m_lineCount =
        makeChild<IntSpinBox>(m_shroud, bindings.make<IntBinding>(valid, lineCountSource(), 0));
    m_lineCount->setObjectName(QStringLiteral("LineCount"));
    grid->addWidget(m_lineCount, 0, 1);

    // The line length and its "Automatic".
    const auto lineLength =
        bindings.make<DoubleBinding>(valid, lineLengthSource(), unitGroup(UnitGroupId::LENGTH),
                                     DoubleBinding::Options{.min = 0.0});
    m_lineLength =
        ConfigFields::addNumberRow(*grid, 1,
                                   //: ParachuteCfg.lbl.Linelength
                                   tr("Line length:"), lineLength, QStringLiteral("LineLength"),
                                   {.slider = SliderMapping::fromRange(0, 0.4, 1.5)});
    //: ParachuteCfg.checkbox.AutomaticLineLength
    m_lineAutomatic = makeChild<BoolCheckBox>(m_shroud, lineLength, tr("Automatic"));
    m_lineAutomatic->setObjectName(QStringLiteral("LineLengthAutomatic"));
    //: ParachuteCfg.checkbox.AutomaticLineLength.ttip
    m_lineAutomatic->setToolTip(tr("Sets the line length to 1.5x the canopy diameter."));
    grid->addWidget(m_lineAutomatic, 2, 1, 1, 3);

    // The line material, its label above it.
    //: ParachuteCfg.lbl.Material
    grid->addWidget(makeChild<QLabel>(m_shroud, tr("Material:")), 3, 0, 1, 4);
    m_lineMaterial = makeChild<MaterialComboBox>(
        m_shroud, bindings.make<MaterialBinding>(valid, lineMaterialSource(), Material::Type::LINE,
                                                 services().materials(), document()));
    m_lineMaterial->setObjectName(QStringLiteral("LineMaterial"));
    grid->addWidget(m_lineMaterial, 4, 0, 1, 4);
    grid->setColumnStretch(3, 1);
    return *m_shroud;
}

void ParachuteConfig::resetCd()
{
    const std::shared_ptr<ComponentSelection>& valid = selection();
    if (!valid->isValid())
    {
        return;
    }
    valid->apply<Parachute>([](Parachute& parachute) { parachute.setCD(Parachute::kDefaultCd); });
}

}  // namespace QtRocket::Gui
