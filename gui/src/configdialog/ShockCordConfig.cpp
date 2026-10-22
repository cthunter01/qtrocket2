#include "QtRocket/gui/configdialog/ShockCordConfig.h"

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
#include "QtRocket/gui/configdialog/MassObjectFields.h"
#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/configdialog/PlacementPanel.h"
#include "QtRocket/gui/configdialog/RadialPositionPanel.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The selection of @p panel, which must be valid and edit a shock cord.
[[nodiscard]] const std::shared_ptr<ComponentSelection>& cordSelection(
    const RocketComponentConfig& panel)
{
    const std::shared_ptr<ComponentSelection>& selection = panel.selection();
    if (selection == nullptr || !selection->isValid())
    {
        bug("The configuration of a shock cord needs a valid selection");
    }
    if (dynamic_cast<const ShockCord*>(&selection->getMain()) == nullptr)
    {
        bug("The configuration of a shock cord needs a shock cord");
    }
    return selection;
}

}  // namespace

DoubleSource ShockCordConfig::cordLengthSource()
{
    return DoubleSource::of<ShockCord>({
        .get          = [](const ShockCord& cord) { return cord.getCordLength(); },
        .set          = [](ShockCord& cord, const double& length) { cord.setCordLength(length); },
        .isAutomatic  = [](const ShockCord& cord) { return cord.isCordLengthAutomatic(); },
        .setAutomatic = [](ShockCord& cord,
                           bool       automatic) { cord.setCordLengthAutomatic(automatic); },
    });
}

ShockCordConfig::ShockCordConfig(OpenRocketDocument& document, const GuiServices& services,
                                 std::shared_ptr<ComponentSelection> selection, QWidget* parent)
  : RocketComponentConfig(document, services, std::move(selection), parent)
{
    const std::shared_ptr<ComponentSelection>& valid    = cordSelection(*this);
    BindingScope&                              bindings = scope();

    auto* page       = makeChild<QWidget>(this);
    auto* primary    = makeChild<QHBoxLayout>(page);
    auto* left       = makeChild<QWidget>(page);
    auto* leftLayout = makeChild<QVBoxLayout>(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    primary->addWidget(left, 0, Qt::AlignTop);

    // The cord length and its "Automatic".
    auto* fields = makeChild<QWidget>(left);
    auto* grid   = makeChild<QGridLayout>(fields);
    grid->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(fields);
    const auto cordLength =
        bindings.make<DoubleBinding>(valid, cordLengthSource(), unitGroup(UnitGroupId::LENGTH),
                                     DoubleBinding::Options{.min = 0.0});
    m_cordLength = ConfigFields::addNumberRow(*grid, 0,
                                              //: ShockCordCfg.lbl.Shockcordlength
                                              tr("Shock cord length:"), cordLength,
                                              QStringLiteral("CordLength"),
                                              {.slider = SliderMapping::fromRange(0, 1, 10)});
    //: ShockCordCfg.checkbox.AutomaticLength
    m_automatic = makeChild<BoolCheckBox>(fields, cordLength, tr("Automatic"));
    m_automatic->setObjectName(QStringLiteral("CordLengthAutomatic"));
    //: ShockCordCfg.checkbox.AutomaticLength.ttip
    m_automatic->setToolTip(tr("Sets the shock cord length to 3x the rocket length."));
    grid->addWidget(m_automatic, 1, 1, 1, 3);
    grid->setColumnStretch(3, 1);

    // The line material (no finish: a shock cord is no external component).
    m_material = makeChild<MaterialPanel>(left, bindings, document, services.materials(),
                                          Material::Type::LINE,
                                          //: ShockCordCfg.lbl.Shockcordmaterial
                                          tr("Shock cord material:"), QString());
    leftLayout->addWidget(m_material);
    leftLayout->addStretch(1);

    // The right side: the placement with the packed size.
    auto* right       = makeChild<QWidget>(page);
    auto* rightLayout = makeChild<QVBoxLayout>(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    m_placement = makeChild<PlacementPanel>(right, bindings);
    m_size = MassObjectFields::addSizeRows(bindings, m_placement->grid(), m_placement->nextRow(),
                                           //: ShockCordCfg.lbl.Packedlength
                                           tr("Packed length:"),
                                           //: ShockCordCfg.lbl.Packeddiam
                                           tr("Packed diameter:"));
    rightLayout->addWidget(m_placement);
    rightLayout->addStretch(1);
    primary->addWidget(right, 0, Qt::AlignTop);
    primary->addStretch(1);

    insertTab(0, *page,
              //: ShockCordCfg.tab.General
              tr("General"),
              //: ShockCordCfg.tab.ttip.General
              tr("General properties"));
    RadialPositionPanel::addTab(*this);
    setSelectedTabIndex(0);
}

}  // namespace QtRocket::Gui
