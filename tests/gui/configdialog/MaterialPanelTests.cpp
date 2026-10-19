#include "QtRocket/gui/configdialog/MaterialPanel.h"

#include <array>
#include <memory>
#include <string>
#include <utility>

#include <QChar>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "ConfigDialogTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/MaterialBinding.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/MaterialComboBox.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

using MaterialPanelTest = Test::GuiTest;
using Test::ConfigDialogServices;
using Test::EveryKindDesign;
using Test::PanelHost;

/// The material panel of @p main (and @p others) in @p host, BULK.
[[nodiscard]] MaterialPanel& bulkPanel(PanelHost& host, const ConfigDialogServices& services,
                                       OpenRocketDocument& document)
{
    return host.make<MaterialPanel>(document, services.services().materials(),
                                    Material::Type::BULK);
}

/// The texts of the labels of @p panel, in the order they were made.
[[nodiscard]] QStringList labelTexts(const QWidget& panel)
{
    QStringList texts;
    for (const QLabel* label : panel.findChildren<QLabel*>())
    {
        texts.append(label->text());
    }
    return texts;
}

/// The names of the external components of @p rocket whose finish is not @p finish, and how many
/// external components it has.
[[nodiscard]] std::pair<std::string, int> otherFinishes(const Rocket& rocket, Finish finish)
{
    std::string names;
    int         externals = 0;
    for (const RocketComponent& component : rocket.subtree())
    {
        if (const auto* external = dynamic_cast<const ExternalComponent*>(&component))
        {
            if (external->getFinish() != finish)
            {
                names += component.getName() + '\n';
            }
            ++externals;
        }
    }
    return {names, externals};
}

TEST_F(MaterialPanelTest, AnExternalComponentHasAMaterialAndAFinish)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    RocketComponent&           tube = design.component(ComponentKind::BODY_TUBE);
    PanelHost                  host(tube);
    const MaterialPanel&       panel = bulkPanel(host, services, *design.document);
    EXPECT_EQ(panel.title(), QStringLiteral("Material"));
    EXPECT_EQ(panel.materialComboBox().objectName(), QStringLiteral("Material"));
    EXPECT_EQ(panel.materialComboBox().toolTip(),
              QStringLiteral("The component material affects the weight of the component."));
    EXPECT_EQ(panel.materialComboBox().shownText(),
              QtStrings::fromUtf8(MaterialBinding::displayString(
                  dynamic_cast<const ExternalComponent&>(tube).getMaterial())));
    ASSERT_NE(panel.finishComboBox(), nullptr);
    EXPECT_EQ(panel.finishComboBox()->objectName(), QStringLiteral("Finish"));
    EXPECT_EQ(panel.finishComboBox()->count(), static_cast<int>(kAllFinishes.size()));
    EXPECT_EQ(panel.finishComboBox()->shownText(), MaterialPanel::finishText(Finish::NORMAL));
    ASSERT_NE(panel.setForAllButton(), nullptr);
    EXPECT_EQ(panel.setForAllButton()->text(), QStringLiteral("Set for all"));
    EXPECT_EQ(panel.setForAllButton()->toolTip(),
              QStringLiteral("Set this finish for all components of the rocket."));
    EXPECT_EQ(labelTexts(panel), (QStringList{QStringLiteral("Component material:"),
                                              QStringLiteral("Component finish:")}));
}

TEST_F(MaterialPanelTest, TheFinishShowsItsRoughness)
{
    // Java's Finish.toString(): the name and the roughness in the default unit (micrometres).
    EXPECT_EQ(MaterialPanel::finishText(Finish::NORMAL),
              QStringLiteral("Regular paint (60 ") + QChar(0x00B5) + QStringLiteral("m)"));
    EXPECT_EQ(MaterialPanel::finishText(Finish::MIRROR),
              QStringLiteral("Mirror surface (0 ") + QChar(0x00B5) + QStringLiteral("m)"));
}

TEST_F(MaterialPanelTest, AnInternalComponentHasNoFinish)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    PanelHost                  host(design.component(ComponentKind::INNER_TUBE));
    const MaterialPanel&       panel = bulkPanel(host, services, *design.document);
    EXPECT_EQ(panel.finishComboBox(), nullptr);
    EXPECT_EQ(panel.setForAllButton(), nullptr);
}

TEST_F(MaterialPanelTest, AShockCordHasALineMaterialWithItsOwnLabel)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    auto&     cord = dynamic_cast<ShockCord&>(design.component(ComponentKind::SHOCK_CORD));
    PanelHost host(cord);
    auto*     panel = &host.make<MaterialPanel>(
        *design.document, services.services().materials(), Material::Type::LINE,
        QStringLiteral("Shock cord material:"), QStringLiteral("unused"));
    const auto* label = panel->findChild<QLabel*>();
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->text(), QStringLiteral("Shock cord material:"));
    EXPECT_EQ(panel->finishComboBox(), nullptr);
    EXPECT_EQ(cord.getMaterial().getType(), Material::Type::LINE);
}

TEST_F(MaterialPanelTest, SetForAllGivesEveryExternalComponentTheFinishInOneUndoStep)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    auto& tube = dynamic_cast<ExternalComponent&>(design.component(ComponentKind::BODY_TUBE));
    tube.setFinish(Finish::MIRROR);
    design.document->clearUndo();
    PanelHost            host(tube);
    const MaterialPanel& panel = bulkPanel(host, services, *design.document);
    ASSERT_NE(panel.setForAllButton(), nullptr);
    panel.setForAllButton()->click();
    const auto [others, externals] = otherFinishes(design.document->getRocket(), Finish::MIRROR);
    EXPECT_EQ(others, "");
    EXPECT_GT(externals, 10);
    EXPECT_EQ(design.document->getUndoDescription(), "Set rocket finish");
    // The undo replaces every component: found again by id.
    const Uuid boosterTube = design.boosterTube->getId();
    design.document->undo();
    const auto* restored = dynamic_cast<const ExternalComponent*>(
        design.document->getRocket().findComponent(boosterTube));
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->getFinish(), Finish::NORMAL);
}

/// The index of the first material @p binding offers that is not @p current, or -1.
[[nodiscard]] int otherMaterialIndex(const MaterialBinding& binding, const Material& current)
{
    for (int i = 0; i < binding.size(); ++i)
    {
        if (*binding.materialAt(i) != current)
        {
            return i;
        }
    }
    return -1;
}

TEST_F(MaterialPanelTest, AChosenMaterialReachesEverySelectedComponent)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    auto& tube  = dynamic_cast<BodyTube&>(design.component(ComponentKind::BODY_TUBE));
    auto& other = *design.boosterTube;
    const std::array<RocketComponent*, 1> others{&other};
    PanelHost                             host(tube, others);
    const MaterialPanel&                  panel = bulkPanel(host, services, *design.document);
    const int index = otherMaterialIndex(*panel.materialComboBox().binding(), tube.getMaterial());
    ASSERT_GE(index, 0);
    const Material chosen = *panel.materialComboBox().binding()->materialAt(index);
    Q_EMIT panel.materialComboBox().activated(index);
    EXPECT_EQ(tube.getMaterial(), chosen);
    EXPECT_EQ(other.getMaterial(), chosen);
}

TEST_F(MaterialPanelTest, AnInvalidSelectionIsABug)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    PanelHost                  host(design.component(ComponentKind::BODY_TUBE));
    host.scope().release();
    EXPECT_THROW(static_cast<void>(bulkPanel(host, services, *design.document)), BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
