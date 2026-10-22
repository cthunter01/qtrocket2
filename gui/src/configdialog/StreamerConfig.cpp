#include "QtRocket/gui/configdialog/StreamerConfig.h"

#include <memory>
#include <utility>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/MaterialBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/MassObjectFields.h"
#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/configdialog/PlacementPanel.h"
#include "QtRocket/gui/configdialog/RadialPositionPanel.h"
#include "QtRocket/gui/configdialog/RecoveryDeviceConfig.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/HtmlLabel.h"
#include "QtRocket/gui/widgets/MaterialComboBox.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/Streamer.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The selection of @p panel, which must be valid and edit a streamer.
[[nodiscard]] const std::shared_ptr<ComponentSelection>& streamerSelection(
    const RocketComponentConfig& panel)
{
    const std::shared_ptr<ComponentSelection>& selection = panel.selection();
    if (selection == nullptr || !selection->isValid())
    {
        bug("The configuration of a streamer needs a valid selection");
    }
    if (dynamic_cast<const Streamer*>(&selection->getMain()) == nullptr)
    {
        bug("The configuration of a streamer needs a streamer");
    }
    return selection;
}

}  // namespace

DoubleSource StreamerConfig::stripLengthSource()
{
    return DoubleSource::of<Streamer>({
        .get = [](const Streamer& streamer) { return streamer.getStripLength(); },
        .set = [](Streamer& streamer, const double& length) { streamer.setStripLength(length); },
    });
}

DoubleSource StreamerConfig::stripWidthSource()
{
    return DoubleSource::of<Streamer>({
        .get = [](const Streamer& streamer) { return streamer.getStripWidth(); },
        .set = [](Streamer& streamer, const double& width) { streamer.setStripWidth(width); },
    });
}

DoubleSource StreamerConfig::areaSource()
{
    return DoubleSource::of<Streamer>({
        .get = [](const Streamer& streamer) { return streamer.getArea(); },
        .set = [](Streamer& streamer, const double& area) { streamer.setArea(area); },
    });
}

DoubleSource StreamerConfig::aspectRatioSource()
{
    return DoubleSource::of<Streamer>({
        .get = [](const Streamer& streamer) { return streamer.getAspectRatio(); },
        .set = [](Streamer& streamer, const double& ratio) { streamer.setAspectRatio(ratio); },
    });
}

StreamerConfig::StreamerConfig(OpenRocketDocument& document, const GuiServices& services,
                               std::shared_ptr<ComponentSelection> selection, QWidget* parent)
  : RecoveryDeviceConfig(document, services, std::move(selection), parent)
{
    BindingScope& bindings = scope();
    static_cast<void>(streamerSelection(*this));

    auto* page    = makeChild<QWidget>(this);
    auto* primary = makeChild<QHBoxLayout>(page);
    auto* left    = makeChild<QWidget>(page);
    addStripFields(*left);
    primary->addWidget(left, 0, Qt::AlignTop);

    // The right side: the placement with the packed size, then the deployment.
    auto* right       = makeChild<QWidget>(page);
    auto* rightLayout = makeChild<QVBoxLayout>(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    m_placement = makeChild<PlacementPanel>(right, bindings);
    m_size = MassObjectFields::addSizeRows(bindings, m_placement->grid(), m_placement->nextRow(),
                                           //: StreamerCfg.lbl.Packedlength
                                           tr("Packed length:"),
                                           //: StreamerCfg.lbl.Packeddiam
                                           tr("Packed diameter:"));
    rightLayout->addWidget(m_placement);
    rightLayout->addWidget(&makeDeploymentPanel(*right));
    rightLayout->addStretch(1);
    primary->addWidget(right, 0, Qt::AlignTop);
    primary->addStretch(1);

    updateFields();

    insertTab(0, *page,
              //: StreamerCfg.tab.General
              tr("General"),
              //: StreamerCfg.tab.ttip.General
              tr("General properties"));
    RadialPositionPanel::addTab(*this);
    setSelectedTabIndex(0);
}

void StreamerConfig::addStripFields(QWidget& left)
{
    const std::shared_ptr<ComponentSelection>& valid    = streamerSelection(*this);
    BindingScope&                              bindings = scope();
    const UnitGroup&                           length   = unitGroup(UnitGroupId::LENGTH);
    auto*                                      grid     = makeChild<QGridLayout>(&left);
    grid->setContentsMargins(0, 0, 0, 0);

    // The strip.
    m_stripLength = ConfigFields::addNumberRow(
        *grid, 0,
        //: StreamerCfg.lbl.Striplength
        tr("Strip length:"),
        bindings.make<DoubleBinding>(valid, stripLengthSource(), length,
                                     DoubleBinding::Options{.min = 0.0}),
        QStringLiteral("StripLength"), {.slider = SliderMapping::fromRange(0, 0.6, 1.5)});
    m_stripWidth = ConfigFields::addNumberRow(
        *grid, 1,
        //: StreamerCfg.lbl.Stripwidth
        tr("Strip width:"),
        bindings.make<DoubleBinding>(valid, stripWidthSource(), length,
                                     DoubleBinding::Options{.min = 0.0}),
        QStringLiteral("StripWidth"), {.slider = SliderMapping::linear(0, 0.2)});
    m_area = ConfigFields::addNumberRow(
        *grid, 2,
        //: StreamerCfg.lbl.Striparea
        tr("Strip area:"),
        bindings.make<DoubleBinding>(valid, areaSource(), unitGroup(UnitGroupId::AREA),
                                     DoubleBinding::Options{.min = 0.0}),
        QStringLiteral("Area"), {.slider = SliderMapping::fromRange(0, 0.04, 0.25)});
    // No unit selector (Java comments it out); the slider in its column all the same.
    m_aspectRatio = ConfigFields::addNumberRow(
        *grid, 3,
        //: StreamerCfg.lbl.Aspectratio
        tr("Aspect ratio:"),
        bindings.make<DoubleBinding>(valid, aspectRatioSource(), unitGroup(UnitGroupId::NONE),
                                     DoubleBinding::Options{.min = 0.0}),
        QStringLiteral("AspectRatio"),
        {.slider = SliderMapping::linear(2, 15), .unitSelector = false});

    // The surface material (Java: MaterialModel and its combo box, no MaterialPanel).
    //: StreamerCfg.lbl.Material
    grid->addWidget(makeChild<QLabel>(&left, tr("Material:")), 4, 0);
    m_material = makeChild<MaterialComboBox>(
        &left, bindings.make<MaterialBinding>(valid, MaterialPanel::componentMaterialSource(),
                                              Material::Type::SURFACE, services().materials(),
                                              document()));
    m_material->setObjectName(QStringLiteral("Material"));
    //: StreamerCfg.combo.ttip.MaterialModel
    m_material->setToolTip(tr("The component material affects the weight of the component."));
    grid->addWidget(m_material, 4, 1, 1, 3);

    // The drag coefficient: no unit selector, no slider, its "Automatic" at the row's end.
    //: StreamerCfg.lbl.longB1
    const QString tip = tr("<html>The drag coefficient relative to the total area of the "
                           "streamer. <br>") +
                        //: StreamerCfg.lbl.longB2
                        tr("A larger drag coefficient yields a slowed descent rate.");
    //: StreamerCfg.lbl.longA1
    m_cdLabel = makeChild<HtmlLabel>(&left, tr("<html>Drag coefficient C<sub>D</sub>:"));
    m_cdLabel->setToolTip(tip);
    grid->addWidget(m_cdLabel, 5, 0);
    const auto cd = bindings.make<DoubleBinding>(
        valid, cdSource(), unitGroup(UnitGroupId::COEFFICIENT), DoubleBinding::Options{.min = 0.0});
    m_cd       = ConfigFields::addNumberRow(*grid, 5, QString(), cd, QStringLiteral("CD"),
                                            {.unitSelector = false});
    m_cd.label = m_cdLabel;
    m_cd.spinBox->setToolTip(tip);
    //: StreamerCfg.lbl.AutomaticCd
    m_cdAutomatic = makeChild<BoolCheckBox>(&left, cd, tr("Automatic"));
    m_cdAutomatic->setObjectName(QStringLiteral("CDAutomatic"));
    //: StreamerCfg.lbl.AutomaticCd.ttip
    m_cdAutomatic->setToolTip(
        tr("Calculate the streamer drag coefficient (C<sub>D</sub>) using its dimensions."));
    grid->addWidget(m_cdAutomatic, 5, 3);

    m_cdNote =
        makeChild<StyledLabel>(&left,
                               //: StreamerCfg.lbl.longC1
                               tr("The drag coefficient is relative to the area of the streamer."),
                               -1.0, StyledLabel::Style::ITALIC);
    grid->addWidget(m_cdNote, 6, 0, 1, 4);
    grid->setColumnStretch(3, 1);
    grid->setRowStretch(7, 1);
}

}  // namespace QtRocket::Gui
