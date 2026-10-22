#include "QtRocket/gui/configdialog/MaterialPanel.h"

#include <memory>
#include <span>

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLatin1Char>
#include <QPushButton>
#include <QString>
#include <QWidget>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/binding/MaterialBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/MaterialComboBox.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/StructuralComponent.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The material of @p component (see componentMaterialSource()).
/// @throws BugError when it has none
[[nodiscard]] Material materialOf(const RocketComponent& component)
{
    if (const auto* external = dynamic_cast<const ExternalComponent*>(&component))
    {
        return external->getMaterial();
    }
    if (const auto* structural = dynamic_cast<const StructuralComponent*>(&component))
    {
        return structural->getMaterial();
    }
    if (const auto* device = dynamic_cast<const RecoveryDevice*>(&component))
    {
        return device->getMaterial();
    }
    if (const auto* cord = dynamic_cast<const ShockCord*>(&component))
    {
        return cord->getMaterial();
    }
    bug("The component has no material");
}

/// Sets @p material on @p component when it has a material (Java forwards the setter to the
/// selected components whose class has it).
void setMaterialOf(RocketComponent& component, const Material& material)
{
    if (auto* external = dynamic_cast<ExternalComponent*>(&component))
    {
        external->setMaterial(material);
    }
    else if (auto* structural = dynamic_cast<StructuralComponent*>(&component))
    {
        structural->setMaterial(material);
    }
    else if (auto* device = dynamic_cast<RecoveryDevice*>(&component))
    {
        device->setMaterial(material);
    }
    else if (auto* cord = dynamic_cast<ShockCord*>(&component))
    {
        cord->setMaterial(material);
    }
}

}  // namespace

MaterialSource MaterialPanel::componentMaterialSource()
{
    return MaterialSource::of<RocketComponent>({
        .get = [](const RocketComponent& component) { return materialOf(component); },
        .set = [](RocketComponent& component,
                  const Material&  material) { setMaterialOf(component, material); },
    });
}

EnumSource<Finish> MaterialPanel::finishSource()
{
    return EnumSource<Finish>::of<ExternalComponent>({
        .get = [](const ExternalComponent& component) { return component.getFinish(); },
        .set = [](ExternalComponent& component,
                  const Finish&      finish) { component.setFinish(finish); },
    });
}

QString MaterialPanel::finishText(Finish finish)
{
    return enumText(finish) + QStringLiteral(" (") +
           QtStrings::defaultUnitText(UnitGroupId::ROUGHNESS, roughnessSize(finish)) +
           QLatin1Char(')');
}

MaterialPanel::MaterialPanel(BindingScope& scope, OpenRocketDocument& document,
                             MaterialStorage& applicationMaterials, Material::Type type,
                             QWidget* parent)
  : MaterialPanel(scope, document, applicationMaterials, type,
                  //: MaterialPanel.lbl.ComponentMaterial
                  tr("Component material:"),
                  //: MaterialPanel.lbl.ComponentFinish
                  tr("Component finish:"), parent)
{
}

MaterialPanel::MaterialPanel(BindingScope& scope, OpenRocketDocument& document,
                             MaterialStorage& applicationMaterials, Material::Type type,
                             const QString& materialText, const QString& finishLabelText,
                             QWidget* parent)
  //: MaterialPanel.title.Material
  : QGroupBox(tr("Material"), parent), m_scope(&scope)
{
    const std::shared_ptr<ComponentSelection>& selection = scope.selection();
    if (selection == nullptr || !selection->isValid())
    {
        bug("The material panel needs a valid selection");
    }
    auto* grid = makeChild<QGridLayout>(this);

    auto* materialLabel = makeChild<QLabel>(this, materialText);
    //: MaterialPanel.lbl.ttip.ComponentMaterialAffects
    materialLabel->setToolTip(tr("The component material affects the weight of the component."));
    grid->addWidget(materialLabel, 0, 0, 1, 2);

    m_material = makeChild<MaterialComboBox>(
        this, scope.make<MaterialBinding>(selection, componentMaterialSource(), type,
                                          applicationMaterials, document));
    m_material->setObjectName(QStringLiteral("Material"));
    //: MaterialPanel.combo.ttip.ComponentMaterialAffects
    m_material->setToolTip(tr("The component material affects the weight of the component."));
    grid->addWidget(m_material, 1, 0, 1, 2);

    // No surface finish for internal components.
    if (dynamic_cast<const ExternalComponent*>(&selection->getMain()) == nullptr)
    {
        return;
    }

    //: MaterialPanel.lbl.ComponentFinish.ttip.longA1
    const QString tip = tr("<html>The component finish affects the aerodynamic drag of the "
                           "component. <br>") +
                        //: MaterialPanel.lbl.ComponentFinish.ttip.longA2
                        tr("The value indicated is the average roughness height of the surface.");
    auto*         finishLabel = makeChild<QLabel>(this, finishLabelText);
    finishLabel->setToolTip(tip);
    grid->addWidget(finishLabel, 2, 0, 1, 2);

    m_finish = makeChild<EnumComboBox>(
        this,
        scope.make<EnumBinding<Finish>>(selection, finishSource(),
                                        std::span<const Finish>(kAllFinishes)),
        [](Finish finish) { return MaterialPanel::finishText(finish); });
    m_finish->setObjectName(QStringLiteral("Finish"));
    m_finish->setToolTip(tip);
    grid->addWidget(m_finish, 3, 0);

    //: MaterialPanel.but.SetForAll
    m_setForAll = makeChild<QPushButton>(this, tr("Set for all"));
    m_setForAll->setObjectName(QStringLiteral("setForAllButton"));
    m_setForAll->setAutoDefault(false);
    //: MaterialPanel.but.SetForAll.ttip
    m_setForAll->setToolTip(tr("Set this finish for all components of the rocket."));
    grid->addWidget(m_setForAll, 3, 1);
    connect(m_setForAll, &QPushButton::clicked, this,
            guarded([this, &document] { setFinishForAll(document); },
                    QStringLiteral("setForAllButton")));
    grid->setColumnStretch(0, 1);
}

void MaterialPanel::setFinishForAll(OpenRocketDocument& document)
{
    const std::shared_ptr<ComponentSelection>& selection = m_scope->selection();
    if (selection == nullptr || !selection->isValid())
    {
        return;  // the dialog is going
    }
    const Finish finish = dynamic_cast<const ExternalComponent&>(selection->getMain()).getFinish();
    //: Not translated in OpenRocket (MaterialPanel.java): the undo step of "Set for all"
    document.startUndo(QtStrings::toUtf8(tr("Set rocket finish")));
    try
    {
        // As Java, straight over the rocket: a slot that changed the tree would end the walk
        // with a BugError (RocketComponent::subtree()).
        for (RocketComponent& component : selection->getRocket().subtree())
        {
            if (auto* external = dynamic_cast<ExternalComponent*>(&component))
            {
                external->setFinish(finish);
            }
        }
    }
    catch (...)
    {
        document.stopUndo();  // Java: finally
        throw;
    }
    document.stopUndo();
}

}  // namespace QtRocket::Gui
