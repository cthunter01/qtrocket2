#include "QtRocket/gui/configdialog/RocketComponentConfig.h"

#include <array>
#include <format>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <QCoreApplication>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLatin1Char>
#include <QLayout>
#include <QLineEdit>
#include <QObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "ConfigDialogTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/aero/ComponentDrag.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/PresetBinding.h"
#include "QtRocket/gui/binding/PresetDiameterFilters.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigPanelFactory.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"
#include "QtRocket/gui/widgets/IconToggleButton.h"
#include "QtRocket/gui/widgets/PresetComboBox.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/TextCommitter.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetFavorites.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "ScriptedUserPrompts.h"
#include "aero/BarrowmanTestRockets.h"
#include "document/TestDocuments.h"
#include "rocket/preset/TestPresetDatabase.h"

namespace QtRocket::Gui
{

namespace
{

using RocketComponentConfigTest = Test::GuiTest;
using Test::childNamed;
using Test::ConfigDialogServices;
using Test::EveryKindDesign;
using Test::tabTitles;

/// The panel of @p main (and @p others) of @p document, a top-level widget of the test.
[[nodiscard]] std::unique_ptr<RocketComponentConfig> panelOf(
    OpenRocketDocument& document, const ConfigDialogServices& services, RocketComponent& main,
    std::span<RocketComponent* const> others = {})
{
    return std::make_unique<RocketComponentConfig>(
        document, services.services(), std::make_shared<ComponentSelection>(main, others), nullptr);
}

/// The text of @p cd as the CD field shows it.
[[nodiscard]] QString coefficientText(double cd)
{
    return QtStrings::unitText(unitGroup(UnitGroupId::COEFFICIENT).getDefaultUnit(), cd);
}

TEST_F(RocketComponentConfigTest, TheCDFieldShowsTheCalculatedCDWhileItIsNotOverridden)
{
    // RocketComponent.getOverrideCD() of Java gives the calculated CD at the preferences' default
    // Mach number while the CD is not overridden (here ComponentDrag::getOverrideCD()).
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    auto&                      tube  = design.component(ComponentKind::BODY_TUBE);
    const auto                 panel = panelOf(*design.document, services, tube);
    const Result<double>       cd =
        ComponentDrag::getOverrideCD(tube, services.services().preferences().getDefaultMach());
    ASSERT_TRUE(cd.has_value()) << cd.error().message;
    auto* field = childNamed<UnitSpinBox>(*panel, QStringLiteral("OverrideCD"));
    ASSERT_NE(field, nullptr);
    EXPECT_EQ(field->text(), coefficientText(*cd));
    EXPECT_FALSE(field->isEnabled());
    EXPECT_EQ(tube.getOverrideCD(), 0.0);
    EXPECT_EQ(childNamed<UnitSelector>(*panel, QStringLiteral("OverrideCD")), nullptr);
    auto* slider = childNamed<ValueSlider>(*panel, QStringLiteral("OverrideCD"));
    ASSERT_NE(slider, nullptr);
    EXPECT_TRUE(slider->mapping().isLinear());
    EXPECT_EQ(slider->mapping().getMin(), -1.0);
    EXPECT_EQ(slider->mapping().getMax(), 1.0);
}

TEST_F(RocketComponentConfigTest, SwitchingTheCDOverrideOnStoresEachComponentsCalculatedCD)
{
    // Java's override starts from the calculated CD it stored when it was read; here each selected
    // component gets its own calculated CD before its override is switched on.
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    RocketComponent&           body    = design.component(ComponentKind::BODY_TUBE);
    RocketComponent&           other   = *design.boosterTube;
    const double               mach    = services.services().preferences().getDefaultMach();
    const Result<double>       bodyCD  = ComponentDrag::getOverrideCD(body, mach);
    const Result<double>       otherCD = ComponentDrag::getOverrideCD(other, mach);
    ASSERT_TRUE(bodyCD.has_value());
    ASSERT_TRUE(otherCD.has_value());
    ASSERT_NE(*bodyCD, *otherCD);
    const std::array<RocketComponent*, 1> others{&other};
    const auto                            panel = panelOf(*design.document, services, body, others);
    auto* check = childNamed<BoolCheckBox>(*panel, QStringLiteral("CDOverridden"));
    ASSERT_NE(check, nullptr);
    check->click();
    EXPECT_TRUE(body.isCDOverridden());
    EXPECT_TRUE(other.isCDOverridden());
    EXPECT_EQ(body.getOverrideCD(), *bodyCD);
    EXPECT_EQ(other.getOverrideCD(), *otherCD);
    auto* field = childNamed<UnitSpinBox>(*panel, QStringLiteral("OverrideCD"));
    ASSERT_NE(field, nullptr);
    EXPECT_TRUE(field->isEnabled());
    EXPECT_EQ(field->text(), coefficientText(*bodyCD));
}

TEST_F(RocketComponentConfigTest, TheCDFieldShowsTheErrorOfAStageSelectionWithoutAnAnalysis)
{
    // The inner boosters active alone, below two inactive stages: the force analysis cannot be
    // made (OpenRocket throws a NullPointerException), and the field shows why.
    const ConfigDialogServices                   services;
    QtRocket::Test::TestBoostersOnBoostersRocket nested;
    const auto                                   document =
        OpenRocketDocumentFactory::createDocumentFromRocket(std::move(nested.rocket));
    document->getRocket().getSelectedConfiguration().setOnlyStage(
        QtRocket::Test::TestBoostersOnBoostersRocket::kInnerStageNumber);
    const auto panel = panelOf(*document, services, *nested.innerFins);
    auto*      field = childNamed<UnitSpinBox>(*panel, QStringLiteral("OverrideCD"));
    ASSERT_NE(field, nullptr);
    EXPECT_EQ(
        field->text(),
        QStringLiteral("The force analysis cannot be made for this selection of stages: stage "
                       "'Inner Boosters' is active below two inactive stages ('Booster Stage' "
                       "and 'Core Stage')."));

    // Switching the override on stores nothing calculated; the stored CD is shown.
    auto* check = childNamed<BoolCheckBox>(*panel, QStringLiteral("CDOverridden"));
    ASSERT_NE(check, nullptr);
    nested.innerFins->setOverrideCD(0.25);
    check->click();
    EXPECT_TRUE(nested.innerFins->isCDOverridden());
    EXPECT_EQ(nested.innerFins->getOverrideCD(), 0.25);
    EXPECT_EQ(field->text(), coefficientText(0.25));
}

TEST_F(RocketComponentConfigTest, TheOverrideFieldsFollowTheirCheckBox)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    RocketComponent&           tube  = design.component(ComponentKind::BODY_TUBE);
    const auto                 panel = panelOf(*design.document, services, tube);
    auto* check = childNamed<BoolCheckBox>(*panel, QStringLiteral("MassOverridden"));
    auto* checkSub =
        childNamed<BoolCheckBox>(*panel, QStringLiteral("SubcomponentsOverriddenMass"));
    auto* field  = childNamed<UnitSpinBox>(*panel, QStringLiteral("OverrideMass"));
    auto* unit   = childNamed<UnitSelector>(*panel, QStringLiteral("OverrideMass"));
    auto* slider = childNamed<ValueSlider>(*panel, QStringLiteral("OverrideMass"));
    ASSERT_NE(check, nullptr);
    ASSERT_NE(checkSub, nullptr);
    ASSERT_NE(field, nullptr);
    ASSERT_NE(unit, nullptr);
    ASSERT_NE(slider, nullptr);
    EXPECT_TRUE(check->isEnabled());
    EXPECT_FALSE(checkSub->isEnabled());
    EXPECT_FALSE(field->isEnabled());
    EXPECT_FALSE(unit->isEnabled());
    EXPECT_FALSE(slider->isEnabled());
    // The mass slider: 0, 30 g, 1 kg (quadratic).
    EXPECT_FALSE(slider->mapping().isLinear());
    EXPECT_EQ(slider->mapping().getMid(), 0.03);
    EXPECT_EQ(slider->mapping().getMax(), 1.0);

    check->click();
    EXPECT_TRUE(tube.isMassOverridden());
    EXPECT_TRUE(checkSub->isEnabled());
    EXPECT_TRUE(field->isEnabled());
    EXPECT_TRUE(unit->isEnabled());
    EXPECT_TRUE(slider->isEnabled());
}

/// The names of @p names that are no check box of @p panel or an enabled one.
[[nodiscard]] std::string enabledCheckBoxes(const QWidget&                     panel,
                                            std::initializer_list<const char*> names)
{
    std::string enabled;
    for (const char* name : names)
    {
        const auto* check = childNamed<BoolCheckBox>(panel, QString::fromLatin1(name));
        if (check == nullptr || check->isEnabled())
        {
            enabled += std::string(name) + '\n';
        }
    }
    return enabled;
}

/// The texts of the labels of @p widget.
[[nodiscard]] QStringList labelTextsOf(const QWidget& widget)
{
    QStringList texts;
    for (const QLabel* label : widget.findChildren<QLabel*>())
    {
        texts.append(label->text());
    }
    return texts;
}

TEST_F(RocketComponentConfigTest, AnAncestorsOverrideDisablesTheBlock)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    RocketComponent&           body = design.component(ComponentKind::BODY_TUBE);
    body.setName("A<B");
    body.setMassOverridden(true);
    body.setSubcomponentsOverriddenMass(true);
    body.setCDOverridden(true);
    body.setSubcomponentsOverriddenCD(true);
    RocketComponent& inner = design.component(ComponentKind::INNER_TUBE);
    ASSERT_EQ(inner.getMassOverriddenBy(), &body);
    const auto panel = panelOf(*design.document, services, inner);

    auto* massBy = childNamed<StyledLabel>(*panel, QStringLiteral("MassOverriddenBy"));
    ASSERT_NE(massBy, nullptr);
    EXPECT_EQ(massBy->text(), QStringLiteral("Mass overridden by A<B"));
    EXPECT_EQ(massBy->textFormat(), Qt::PlainText);
    EXPECT_EQ(massBy->toolTip(),
              QStringLiteral(
                  "The mass of this component is determined by the mass override value of A<B"));
    auto* cdBy = childNamed<StyledLabel>(*panel, QStringLiteral("CDOverriddenBy"));
    ASSERT_NE(cdBy, nullptr);
    EXPECT_EQ(cdBy->text(), QStringLiteral("<html>C<sub>D</sub> overridden by A&lt;B"));
    EXPECT_EQ(cdBy->toolTip(), cdBy->text());
    EXPECT_EQ(childNamed<StyledLabel>(*panel, QStringLiteral("CGOverriddenBy")), nullptr);

    EXPECT_EQ(enabledCheckBoxes(*panel, {"MassOverridden", "SubcomponentsOverriddenMass",
                                         "CDOverridden", "SubcomponentsOverriddenCD"}),
              "");
    auto* mass = childNamed<UnitSpinBox>(*panel, QStringLiteral("OverrideMass"));
    ASSERT_NE(mass, nullptr);
    EXPECT_FALSE(mass->isEnabled());
    // Decided when the panel is made (Java): the ancestor's change leaves the block disabled.
    inner.setMassOverridden(true);
    EXPECT_FALSE(mass->isEnabled());
    auto* cg = childNamed<BoolCheckBox>(*panel, QStringLiteral("CGOverridden"));
    ASSERT_NE(cg, nullptr);
    EXPECT_TRUE(cg->isEnabled());
}

/// A rocket of one stage with a body tube "Body" of 0.3 m and nothing else, in a document.
struct OneTubeDesign
{
    std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
    BodyTube*                           body{nullptr};

    OneTubeDesign()
    {
        body = &document->getRocket().getChildren().front()->addChild(std::make_unique<BodyTube>());
        body->setName("Body");
        body->setLength(0.3);
    }
};

TEST_F(RocketComponentConfigTest, TheCGSliderEndsAtTheExtentOfTheSubtree)
{
    // RocketComponentConfig.overrideTab(): the span of the component and its subtree, from the
    // first front to the last end, absolute positions.
    OneTubeDesign design;
    auto&         inner = design.body->addChild(std::make_unique<InnerTube>());
    inner.setLength(0.1);
    inner.setAxialMethod(AxialMethod::BOTTOM);
    inner.setAxialOffset(0.05);
    ASSERT_DOUBLE_EQ(inner.getAxialOffset(AxialMethod::ABSOLUTE), 0.25);
    const std::optional<double> extent = RocketComponentConfig::overrideCGExtent(*design.body);
    ASSERT_TRUE(extent.has_value());
    EXPECT_DOUBLE_EQ(extent.value_or(-1.0), 0.35);

    // A childless component: its length, followed.
    EXPECT_FALSE(RocketComponentConfig::overrideCGExtent(inner).has_value());
    const ConfigDialogServices services;
    const auto                 panel = panelOf(*design.document, services, inner);
    auto* slider = childNamed<ValueSlider>(*panel, QStringLiteral("OverrideCGX"));
    ASSERT_NE(slider, nullptr);
    EXPECT_EQ(slider->mapping().getMax(), 0.1);
    inner.setLength(0.2);
    EXPECT_EQ(slider->mapping().getMax(), 0.2);
}

TEST_F(RocketComponentConfigTest, TheCGSliderOfAChildlessFinSetEndsAtItsBoundingBox)
{
    OneTubeDesign const         design;
    auto const&                 fins   = design.body->addChild(std::make_unique<TrapezoidFinSet>());
    const std::optional<double> extent = RocketComponentConfig::overrideCGExtent(fins);
    ASSERT_TRUE(extent.has_value());
    EXPECT_EQ(extent.value_or(-1.0), fins.getInstanceBoundingBox().span().x);
}

TEST_F(RocketComponentConfigTest, TheCGExtentKeepsJavasStartOfTheMaximum)
{
    // Java starts maxL at Double.MIN_VALUE, the smallest POSITIVE double: a subtree that ends
    // before 0 spans from its front to 0.
    OneTubeDesign const design;
    auto&               inner = design.body->addChild(std::make_unique<InnerTube>());
    inner.setLength(0.07);
    inner.setAxialMethod(AxialMethod::ABSOLUTE);
    inner.setAxialOffset(-1.0);
    static_cast<void>(inner.addChild(std::make_unique<EngineBlock>()));
    ASSERT_EQ(inner.getAxialOffset(AxialMethod::ABSOLUTE), -1.0);
    const std::optional<double> extent = RocketComponentConfig::overrideCGExtent(inner);
    ASSERT_TRUE(extent.has_value());
    EXPECT_EQ(extent.value_or(-1.0), std::numeric_limits<double>::denorm_min() + 1.0);
}

TEST_F(RocketComponentConfigTest, TheCommentTabCommitsWithTheName)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    RocketComponent&           tube = design.component(ComponentKind::BODY_TUBE);
    tube.setComment("first");
    const auto panel = panelOf(*design.document, services, tube);
    EXPECT_EQ(panel->commentArea().toPlainText(), QStringLiteral("first"));
    EXPECT_TRUE(labelTextsOf(*panel->tabs().widget(1))
                    .contains(QStringLiteral("Comments on the Body Tube:")));

    // Java's TextFieldListener: the name field's commit commits the comment too.
    panel->commentArea().setPlainText(QStringLiteral("second"));
    panel->nameField().setText(QStringLiteral("Main"));
    Q_EMIT panel->nameField().textEdited(QStringLiteral("Main"));
    EXPECT_TRUE(panel->textCommitter().commit());
    EXPECT_EQ(tube.getName(), "Main");
    EXPECT_EQ(tube.getComment(), "second");
    EXPECT_EQ(panel->commentArea().objectName(), QStringLiteral("Comment"));
    EXPECT_EQ(panel->nameField().objectName(), QStringLiteral("Name"));
}

TEST_F(RocketComponentConfigTest, TheNameNamesEverySelectedComponent)
{
    const ConfigDialogServices            services;
    const EveryKindDesign                 design;
    RocketComponent&                      body = design.component(ComponentKind::BODY_TUBE);
    const std::array<RocketComponent*, 1> others{design.boosterTube};
    const auto                            panel = panelOf(*design.document, services, body, others);
    QTest::keyClicks(&panel->nameField(), QStringLiteral(" 2"));
    QTest::keyClick(&panel->nameField(), Qt::Key_Return);
    EXPECT_EQ(body.getName(), "Body 2");
    EXPECT_EQ(design.boosterTube->getName(), "Body 2");
}

TEST_F(RocketComponentConfigTest, TheInfoLabelShowsTheMassOfOneMassiveComponent)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    RocketComponent&           body  = design.component(ComponentKind::BODY_TUBE);
    const auto                 panel = panelOf(*design.document, services, body);
    const QString mass = QtStrings::defaultUnitText(UnitGroupId::MASS, body.getComponentMass());
    EXPECT_EQ(panel->infoLabel().text(), QStringLiteral("Component mass: ") + mass);

    body.setMassOverridden(true);
    body.setOverrideMass(0.05);
    panel->updateFields();
    EXPECT_EQ(panel->infoLabel().text(),
              QStringLiteral("Component mass: ") + mass + QStringLiteral(" (overridden to ") +
                  QtStrings::defaultUnitText(UnitGroupId::MASS, 0.05) + QLatin1Char(')'));

    body.setSubcomponentsOverriddenMass(true);
    RocketComponent& inner      = design.component(ComponentKind::INNER_TUBE);
    const auto       innerPanel = panelOf(*design.document, services, inner);
    EXPECT_EQ(innerPanel->infoLabel().text(),
              QStringLiteral("Component mass: ") +
                  QtStrings::defaultUnitText(UnitGroupId::MASS, inner.getComponentMass()) +
                  QStringLiteral(" (overridden by Body)"));

    // Not for several components, nor for one that is not massive.
    const std::array<RocketComponent*, 1> others{design.boosterTube};
    EXPECT_EQ(panelOf(*design.document, services, body, others)->infoLabel().text(), QString());
    EXPECT_EQ(panelOf(*design.document, services, design.document->getRocket())->infoLabel().text(),
              QString());
}

TEST_F(RocketComponentConfigTest, TheInfoLabelNamesTheComponentsPreset)
{
    const ConfigDialogServices services;
    const auto                 document = QtRocket::Test::makeTestRocketV105WithComponentPreset();
    RocketComponent* tube = document->getRocket().getChildren().front()->getChildren().front();
    ASSERT_NE(tube->getPresetComponent(), nullptr);
    const auto panel = panelOf(*document, services, *tube);
    EXPECT_TRUE(panel->infoLabel().text().startsWith(
        QStringLiteral("manufacturer partno      Component mass: ")))
        << QtStrings::toUtf8(panel->infoLabel().text());
}

TEST_F(RocketComponentConfigTest, TheInfoButtonShowsTheDescription)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    QWidget                    window;
    RocketComponent&           body = design.component(ComponentKind::BODY_TUBE);
    auto* panel = makeChild<RocketComponentConfig>(&window, *design.document, services.services(),
                                                   std::make_shared<ComponentSelection>(body));
    IconToggleButton*      button = panel->componentInfoButton();
    const DescriptionArea* info   = panel->componentInfo();
    ASSERT_NE(button, nullptr);
    ASSERT_NE(info, nullptr);
    window.show();
    EXPECT_TRUE(button->isChecked());
    EXPECT_FALSE(info->isVisible());
    EXPECT_EQ(info->description(),
              QStringLiteral("<html>") +
                  RocketComponentConfig::componentInfoText(ComponentKind::BODY_TUBE) +
                  QStringLiteral("</html>"));
    button->click();
    EXPECT_TRUE(info->isVisible());
    // Java's updateFields() hides it again.
    panel->updateFields();
    EXPECT_FALSE(info->isVisible());
    EXPECT_TRUE(button->isChecked());
    EXPECT_EQ(panel->multiComponentEditLabel().text(), QString());
}

TEST_F(RocketComponentConfigTest, AMultiComponentEditHasNoDescriptionButALabelNamingThem)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    RocketComponent&           body = design.component(ComponentKind::BODY_TUBE);
    design.boosterTube->setName("<i>Booster</i>");
    const std::array<RocketComponent*, 1> others{design.boosterTube};
    const auto                            panel = panelOf(*design.document, services, body, others);
    EXPECT_EQ(panel->componentInfoButton(), nullptr);
    EXPECT_EQ(panel->componentInfo(), nullptr);
    EXPECT_EQ(panel->multiComponentEditLabel().text(), QStringLiteral("Multi-component edit"));
    EXPECT_EQ(panel->multiComponentEditLabel().toolTip(),
              QStringLiteral("<html>You are editing the following components: <br>Body, "
                             "&lt;i&gt;Booster&lt;/i&gt;"));
    // Java: StyledLabel(" ", -1, Style.BOLD), the size of the mass label beside it.
    EXPECT_TRUE(panel->multiComponentEditLabel().font().bold());
    EXPECT_DOUBLE_EQ(panel->multiComponentEditLabel().font().pointSizeF(),
                     panel->infoLabel().font().pointSizeF());
    EXPECT_FALSE(panel->infoLabel().font().bold());
}

/// The objectNames the Java property names give the Override tab's widgets, and their kinds.
[[nodiscard]] std::string missingOverrideWidgets(const QWidget& panel)
{
    std::string missing;
    for (const char* name :
         {"MassOverridden", "SubcomponentsOverriddenMass", "CGOverridden",
          "SubcomponentsOverriddenCG", "CDOverridden", "SubcomponentsOverriddenCD"})
    {
        if (childNamed<BoolCheckBox>(panel, QString::fromLatin1(name)) == nullptr)
        {
            missing += std::string(name) + " check box\n";
        }
    }
    for (const char* name : {"OverrideMass", "OverrideCGX", "OverrideCD"})
    {
        if (childNamed<UnitSpinBox>(panel, QString::fromLatin1(name)) == nullptr ||
            childNamed<ValueSlider>(panel, QString::fromLatin1(name)) == nullptr)
        {
            missing += std::string(name) + " field\n";
        }
    }
    for (const char* name : {"OverrideMass", "OverrideCGX"})
    {
        if (childNamed<UnitSelector>(panel, QString::fromLatin1(name)) == nullptr)
        {
            missing += std::string(name) + " unit selector\n";
        }
    }
    return missing;
}

TEST_F(RocketComponentConfigTest, TheOverrideTabIsNamedAfterJavasProperties)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    const auto                 panel =
        panelOf(*design.document, services, design.component(ComponentKind::BODY_TUBE));
    EXPECT_EQ(missingOverrideWidgets(*panel), "");
    auto* note = childNamed<StyledLabel>(*panel, QStringLiteral("overrideNote"));
    ASSERT_NE(note, nullptr);
    EXPECT_EQ(note->text(),
              QStringLiteral("<html>The overridden mass and center of gravity does not include "
                             "motors. <br>The center of gravity is measured from the front end of "
                             "the body tube."));
    EXPECT_EQ(tabTitles(*panel),
              (QStringList{QStringLiteral("Override"), QStringLiteral("Comment")}));
    EXPECT_EQ(panel->tabs().tabToolTip(0), QStringLiteral("Override auto-calculated properties."));
    EXPECT_EQ(panel->tabs().tabToolTip(1), QStringLiteral("Specify a comment for the component"));
}

TEST_F(RocketComponentConfigTest, TabsAreInsertedAndSelectedByTitle)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    const auto                 panel =
        panelOf(*design.document, services, design.component(ComponentKind::BODY_TUBE));
    auto* page = makeChild<QWidget>(panel.get());
    panel->insertTab(0, *page, QStringLiteral("General"), QStringLiteral("General properties"));
    EXPECT_EQ(tabTitles(*panel), (QStringList{QStringLiteral("General"), QStringLiteral("Override"),
                                              QStringLiteral("Comment")}));
    EXPECT_EQ(panel->tabs().tabToolTip(0), QStringLiteral("General properties"));
    panel->setSelectedTab(QStringLiteral("Comment"));
    EXPECT_EQ(panel->selectedTabIndex(), 2);
    EXPECT_EQ(panel->selectedTabName(), QStringLiteral("Comment"));
    panel->setSelectedTab(QStringLiteral("Appearance"));
    EXPECT_EQ(panel->selectedTabIndex(), 0);
    panel->setSelectedTabIndex(1);
    EXPECT_EQ(panel->selectedTabName(), QStringLiteral("Override"));
}

TEST_F(RocketComponentConfigTest, AButtonGoesBeforeCancelAndOKLastInTheFocusChain)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    const auto                 panel =
        panelOf(*design.document, services, design.component(ComponentKind::BODY_TUBE));
    auto* split = makeChild<QPushButton>(panel.get(), QStringLiteral("Split"));
    panel->addButton(*split);
    EXPECT_FALSE(split->autoDefault());
    EXPECT_EQ(split->parentWidget(), panel->cancelButton().parentWidget());
    EXPECT_LT(split->parentWidget()->layout()->indexOf(split),
              split->parentWidget()->layout()->indexOf(&panel->cancelButton()));
    EXPECT_EQ(panel->okButton().text(), QStringLiteral("OK"));
    EXPECT_EQ(panel->cancelButton().text(), QStringLiteral("Cancel"));
    EXPECT_FALSE(panel->okButton().autoDefault());
    EXPECT_FALSE(panel->cancelButton().autoDefault());

    auto*       page = makeChild<QWidget>(panel.get());
    auto const* late = makeChild<QLineEdit>(page);
    panel->insertTab(0, *page, QStringLiteral("Late"), QString());
    panel->orderFocus();
    EXPECT_EQ(panel->cancelButton().nextInFocusChain(), &panel->okButton());
    EXPECT_EQ(late->nextInFocusChain(), &panel->cancelButton());
}

TEST_F(RocketComponentConfigTest, EveryKindHasADescription)
{
    for (const ComponentKind kind : kAllComponentKinds)
    {
        EXPECT_FALSE(RocketComponentConfig::componentInfoText(kind).isEmpty())
            << componentKindName(kind);
    }
    // OpenRocket's texts name the application.
    EXPECT_TRUE(RocketComponentConfig::componentInfoText(ComponentKind::TUBE_FIN_SET)
                    .contains(QStringLiteral("Currently, QtRocket only supports")));
    EXPECT_TRUE(RocketComponentConfig::componentInfoText(ComponentKind::MASS_COMPONENT)
                    .contains(QStringLiteral("In QtRocket, this component type")));
    EXPECT_TRUE(
        RocketComponentConfig::componentInfoText(ComponentKind::MASS_COMPONENT)
            .contains(QStringLiteral("<b>battery</b>") + QChar(0x2026) + QStringLiteral(" Each")));
}

TEST_F(RocketComponentConfigTest, AnInvalidSelectionIsABug)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    EXPECT_THROW(RocketComponentConfig(*design.document, services.services(), nullptr, nullptr),
                 BugError);
    auto const selection =
        std::make_shared<ComponentSelection>(design.component(ComponentKind::BODY_TUBE));
    selection->release();
    EXPECT_THROW(RocketComponentConfig(*design.document, services.services(), selection, nullptr),
                 BugError);
}

TEST_F(RocketComponentConfigTest, ReleaseEndsTheBindings)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    RocketComponent&           body  = design.component(ComponentKind::BODY_TUBE);
    const auto                 panel = panelOf(*design.document, services, body);
    EXPECT_FALSE(panel->scope().isReleased());
    panel->release();
    EXPECT_TRUE(panel->scope().isReleased());
    EXPECT_FALSE(panel->selection()->isValid());
    panel->nameField().setText(QStringLiteral("Dropped"));
    Q_EMIT panel->nameField().textEdited(QStringLiteral("Dropped"));
    EXPECT_FALSE(panel->commitAll());
    EXPECT_EQ(body.getName(), "Body");
    // updateFields() of a released panel shows what it showed.
    panel->updateFields();
}

// =========================================================================== the presets

// Expectations marked "probe" are OpenRocket's own, printed by a probe of Milestone 3 run 2 part S5
// (ConfigPresetRowProbe, OpenRocket 5f164fd0e, JDK 17, under xvfb: the panels need a parent
// dialog, which is never shown; not in tools/, so its transcript is summed up here): the panel
// ComponentConfigDialog makes for each component of a design with a component of every kind, and
// for two body tubes and for a body tube with a nose cone (config listeners), and of each panel
// whether it has the preset combo box (its tooltip, 25 rows, not editable) and the "Parts Library"
// button (its tooltip), the first entries of its focus order (the name field, the combo box, the
// button, the tabs) and the field it focuses after a preset (focusElement: "spinner Length", the
// property of the spinner's DoubleModel, or null). They agree with
// RocketComponentConfig.java:160-181 and the panels' "focusElement = spin" (BodyTubeConfig:50,
// LaunchLugConfig:46, InnerTubeConfig:93, TubeFinSetConfig:57, RingComponentConfig:54 for a
// ThicknessRingComponent).

/// The preset combo box's tooltip (PresetModel.combo.ttip).
[[nodiscard]] QString presetComboTip()
{
    return QStringLiteral(
        "<html>Select a preset model from a list of favorites (selected in the component preset "
        "dialog), <br>or select 'Custom' when no preset is required.</html>");
}

/// The "Parts Library" button's tooltip (PresetModel.lbl.partsLib.ttip).
[[nodiscard]] QString partsLibraryTip()
{
    return QStringLiteral(
        "Select a preset model for this rocket component from a library of parts.");
}

/// A panel the factory made (the kind's own class), with the widget that owns it.
struct CreatedPanel
{
    std::unique_ptr<QWidget> host = std::make_unique<QWidget>();
    RocketComponentConfig*   panel{nullptr};

    RocketComponentConfig* operator->() const noexcept { return panel; }
};

/// The panel the dialog makes for @p main (and @p others).
[[nodiscard]] CreatedPanel createdPanel(OpenRocketDocument&         document,
                                        const ConfigDialogServices& services, RocketComponent& main,
                                        std::span<RocketComponent* const> others = {})
{
    CreatedPanel created;
    created.panel = ConfigPanelFactory::create(document, services.services(),
                                               std::make_shared<ComponentSelection>(main, others),
                                               created.host.get());
    return created;
}

/// What the probe printed of a panel's preset row: "no combo", or the combo box and the button.
[[nodiscard]] std::string presetRowText(const RocketComponentConfig& panel)
{
    const PresetComboBox* combo  = panel.presetComboBox();
    const QPushButton*    button = panel.partsLibraryButton();
    if (combo == nullptr || button == nullptr)
    {
        return combo == nullptr && button == nullptr ? "no combo" : "half a row";
    }
    return std::format("combo {} tip={} rows={} editable={}; button {} '{}' tip={}",
                       QtStrings::toUtf8(combo->objectName()), combo->toolTip() == presetComboTip(),
                       combo->maxVisibleItems(), combo->isEditable(),
                       QtStrings::toUtf8(button->objectName()), QtStrings::toUtf8(button->text()),
                       button->toolTip() == partsLibraryTip());
}

/// The text of @p preset in the combo box (Java: ComponentPreset.toString(), its part number).
[[nodiscard]] QString presetText(const ComponentPreset& preset)
{
    return QtStrings::fromUtf8(preset.toString());
}

/// The field that takes the focus after a preset, as the probe printed it ("spinner Length").
[[nodiscard]] std::string presetFocusText(const RocketComponentConfig& panel)
{
    const QWidget* field = panel.presetFocusField();
    if (field == nullptr)
    {
        return "null";
    }
    return (qobject_cast<const UnitSpinBox*>(field) != nullptr ? "spinner " : "widget ") +
           QtStrings::toUtf8(field->objectName());
}

/// The kinds with OpenRocket's preset row and the field it focuses after a preset (probe; the
/// other kinds have no row: the Rocket, the stages, the pod set, the three fin sets with fins, the
/// mass component and the shock cord).
struct PresetRowPin
{
    ComponentKind kind;
    const char*   focus;
};

constexpr std::array kPresetRowPins{
    PresetRowPin{.kind = ComponentKind::NOSE_CONE, .focus = "null"},
    PresetRowPin{.kind = ComponentKind::BODY_TUBE, .focus = "spinner Length"},
    PresetRowPin{.kind = ComponentKind::TRANSITION, .focus = "null"},
    PresetRowPin{.kind = ComponentKind::TUBE_FIN_SET, .focus = "spinner Length"},
    PresetRowPin{.kind = ComponentKind::LAUNCH_LUG, .focus = "spinner Length"},
    PresetRowPin{.kind = ComponentKind::RAIL_BUTTON, .focus = "null"},
    PresetRowPin{.kind = ComponentKind::INNER_TUBE, .focus = "spinner Length"},
    PresetRowPin{.kind = ComponentKind::ENGINE_BLOCK, .focus = "spinner Length"},
    PresetRowPin{.kind = ComponentKind::TUBE_COUPLER, .focus = "spinner Length"},
    PresetRowPin{.kind = ComponentKind::CENTERING_RING, .focus = "null"},
    PresetRowPin{.kind = ComponentKind::BULKHEAD, .focus = "null"},
    PresetRowPin{.kind = ComponentKind::PARACHUTE, .focus = "null"},
    PresetRowPin{.kind = ComponentKind::STREAMER, .focus = "null"},
};

/// The line of @p kind's panel ("<class>: <row>; focus <field>").
[[nodiscard]] std::string presetRowLine(const EveryKindDesign&      design,
                                        const ConfigDialogServices& services, ComponentKind kind)
{
    const CreatedPanel panel = createdPanel(*design.document, services, design.component(kind));
    return std::format("{}: {}; focus {}", className(kind), presetRowText(*panel.panel),
                       presetFocusText(*panel.panel));
}

/// The line the probe gives @p kind.
[[nodiscard]] std::string expectedPresetRowLine(ComponentKind kind)
{
    const std::string row =
        "combo presetComboBox tip=true rows=25 editable=false; button selectPresetButton 'Parts "
        "Library' tip=true";
    for (const PresetRowPin& pin : kPresetRowPins)
    {
        if (pin.kind == kind)
        {
            return std::format("{}: {}; focus {}", className(kind), row, pin.focus);
        }
    }
    return std::format("{}: no combo; focus null", className(kind));
}

/// The lines of every kind that differ from the probe's, one per line.
[[nodiscard]] std::string presetRowDifferences()
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    std::string                differences;
    for (const ComponentKind kind : kAllComponentKinds)
    {
        const std::string actual   = presetRowLine(design, services, kind);
        const std::string expected = expectedPresetRowLine(kind);
        if (actual != expected)
        {
            differences += std::format("{}\n  expected {}\n", actual, expected);
        }
    }
    return differences;
}

/// The object names of the first @p count widgets after the name field in the focus chain of
/// @p panel that take the focus by Tab ("tabs" for one inside the tabs).
[[nodiscard]] QStringList focusAfterName(const RocketComponentConfig& panel, int count)
{
    QStringList names;
    for (const QWidget* widget = panel.nameField().nextInFocusChain();
         widget != &panel.nameField() && names.size() < count; widget = widget->nextInFocusChain())
    {
        if (!panel.isAncestorOf(widget) || (widget->focusPolicy() & Qt::TabFocus) == 0)
        {
            continue;
        }
        names.append(panel.tabs().isAncestorOf(widget) || widget == &panel.tabs()
                         ? QStringLiteral("tabs")
                         : widget->objectName());
    }
    return names;
}

TEST_F(RocketComponentConfigTest, ThePresetRowIsThatOfTheKindsThatTakePresets)
{
    // Probe: the 13 kinds whose class has a preset type, with Java's texts, rows and focus field.
    EXPECT_EQ(presetRowDifferences(), "");
}

TEST_F(RocketComponentConfigTest, SeveralComponentsOfOneClassShareThePresetRow)
{
    // Probe: two body tubes have it, a body tube with a nose cone has not (RocketComponentConfig).
    const ConfigDialogServices            services;
    const EveryKindDesign                 design;
    RocketComponent&                      body = design.component(ComponentKind::BODY_TUBE);
    const std::array<RocketComponent*, 1> tubes{design.boosterTube};
    const CreatedPanel two = createdPanel(*design.document, services, body, tubes);
    EXPECT_EQ(presetRowText(*two.panel),
              "combo presetComboBox tip=true rows=25 editable=false; button selectPresetButton "
              "'Parts Library' tip=true");
    EXPECT_EQ(presetFocusText(*two.panel), "spinner Length");
    const std::array<RocketComponent*, 1> nose{&design.component(ComponentKind::NOSE_CONE)};
    const CreatedPanel mixed = createdPanel(*design.document, services, body, nose);
    EXPECT_EQ(presetRowText(*mixed.panel), "no combo");
    EXPECT_EQ(presetFocusText(*mixed.panel), "null");
}

TEST_F(RocketComponentConfigTest, ThePresetRowFollowsTheNameField)
{
    // Probe: the focus order is the name field, the combo box, the button, then the tabs. The
    // combo box grows a little more than the name field (Java: "growx 110" and "growx").
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    const CreatedPanel         panel =
        createdPanel(*design.document, services, design.component(ComponentKind::BODY_TUBE));
    EXPECT_EQ(focusAfterName(*panel.panel, 3),
              (QStringList{QStringLiteral("presetComboBox"), QStringLiteral("selectPresetButton"),
                           QStringLiteral("tabs")}));
    auto* header = qobject_cast<QHBoxLayout*>(panel->nameField().parentWidget()->layout());
    ASSERT_NE(header, nullptr);
    ASSERT_NE(panel->presetComboBox(), nullptr);
    EXPECT_EQ(header->stretch(header->indexOf(&panel->nameField())), 100);
    EXPECT_EQ(header->stretch(header->indexOf(panel->presetComboBox())), 110);
    EXPECT_EQ(header->stretch(header->indexOf(panel->partsLibraryButton())), 0);
    EXPECT_FALSE(panel->partsLibraryButton()->autoDefault());
}

TEST_F(RocketComponentConfigTest, ThePartsLibraryAsksLaterForTheMainComponentWithItsDiameters)
{
    // RocketComponentConfig.selectPreset(): invokeLater, then the parts library for the main
    // component (its kind, its neighbours' diameters: PresetDiameterFilters).
    ConfigDialogServices                  services;
    const EveryKindDesign                 design;
    RocketComponent&                      body = design.component(ComponentKind::BODY_TUBE);
    const std::array<RocketComponent*, 1> others{design.boosterTube};
    const CreatedPanel panel = createdPanel(*design.document, services, body, others);
    services.prompts().answerPreset(
        {.outcome = UserPrompts::PresetOutcome::NONE, .preset = nullptr});
    panel->partsLibraryButton()->click();
    EXPECT_TRUE(services.prompts().presetRequests().empty());
    QCoreApplication::processEvents();
    ASSERT_EQ(services.prompts().presetRequests().size(), 1U);
    const UserPrompts::PresetRequest& request = services.prompts().presetRequests().front();
    const PresetDiameterFilters       filters = PresetDiameterFilters::forComponent(body);
    EXPECT_EQ(request.componentKind, ComponentKind::BODY_TUBE);
    ASSERT_TRUE(filters.foreDiameter.has_value());
    ASSERT_TRUE(filters.aftDiameter.has_value());
    EXPECT_EQ(request.foreDiameter, filters.foreDiameter);
    EXPECT_EQ(request.aftDiameter, filters.aftDiameter);
    EXPECT_FALSE(request.flippedNoseCone);
    // Nothing chosen: nothing changed.
    EXPECT_FALSE(design.document->isUndoAvailable());
    EXPECT_EQ(body.getPresetComponent(), nullptr);
}

TEST_F(RocketComponentConfigTest, AFlippedNoseConeAsksWithTheFlippedTexts)
{
    ConfigDialogServices  services;
    const EveryKindDesign design;
    auto& nose = dynamic_cast<NoseCone&>(design.component(ComponentKind::NOSE_CONE));
    nose.setFlipped(true);
    design.document->clearUndo();
    const CreatedPanel panel = createdPanel(*design.document, services, nose);
    services.prompts().answerPreset(
        {.outcome = UserPrompts::PresetOutcome::CLOSED, .preset = nullptr});
    panel->selectPreset();
    QCoreApplication::processEvents();
    ASSERT_EQ(services.prompts().presetRequests().size(), 1U);
    const UserPrompts::PresetRequest& request = services.prompts().presetRequests().front();
    const PresetDiameterFilters       filters = PresetDiameterFilters::forComponent(nose);
    EXPECT_EQ(request.componentKind, ComponentKind::NOSE_CONE);
    EXPECT_EQ(request.foreDiameter, filters.foreDiameter);
    EXPECT_EQ(request.aftDiameter, filters.aftDiameter);
    EXPECT_TRUE(request.flippedNoseCone);
}

TEST_F(RocketComponentConfigTest, APresetChosenInThePartsLibraryIsAppliedAndTheLengthTakesTheFocus)
{
    // The parts library's Close: PresetModel.setSelectedItem() of the preset, the undo position
    // "Use Preset Body Tube" and loadPreset() on every selected component; then setFocusElement()
    // later (invokeLater): the length field.
    ConfigDialogServices services;
    services.presets() = QtRocket::Test::smallPresetDatabase();
    const EveryKindDesign design;
    auto&     body  = dynamic_cast<BodyTube&>(design.component(ComponentKind::BODY_TUBE));
    BodyTube& other = *design.boosterTube;
    const std::array<RocketComponent*, 1> others{&other};
    const CreatedPanel panel = createdPanel(*design.document, services, body, others);
    panel.host->show();
    const QWidget* const field = panel->presetFocusField();
    ASSERT_NE(field, nullptr);
    const std::vector<PresetBinding::PresetPtr> tubes =
        services.presets().listForType(ComponentPresetType::BODY_TUBE);
    ASSERT_GT(tubes.size(), 7U);
    services.prompts().answerPreset(
        {.outcome = UserPrompts::PresetOutcome::CHOSEN, .preset = tubes.at(7)});
    panel->partsLibraryButton()->click();
    QCoreApplication::processEvents();
    EXPECT_EQ(body.getPresetComponent(), tubes.at(7).get());
    EXPECT_EQ(other.getPresetComponent(), tubes.at(7).get());
    EXPECT_EQ(design.document->getUndoDescription(),
              std::optional<std::string>("Use Preset Body Tube"));
    ASSERT_NE(panel->presetComboBox(), nullptr);
    EXPECT_EQ(panel->presetComboBox()->shownText(), presetText(*tubes.at(7)));
    // The focus moves one turn later.
    EXPECT_NE(panel->focusWidget(), field);
    QCoreApplication::processEvents();
    EXPECT_EQ(panel->focusWidget(), field);
}

TEST_F(RocketComponentConfigTest, TheFavouritesTickedInThePartsLibraryReachTheComboBox)
{
    // Java's combo box listens to the database while the parts library is open; here it reads the
    // favourites again when the parts library is left, also without a choice.
    ConfigDialogServices services;
    services.presets() = QtRocket::Test::smallPresetDatabase();
    const EveryKindDesign design;
    const CreatedPanel    panel =
        createdPanel(*design.document, services, design.component(ComponentKind::BODY_TUBE));
    const PresetComboBox* combo = panel->presetComboBox();
    ASSERT_NE(combo, nullptr);
    EXPECT_EQ(combo->count(), 1);
    const std::vector<PresetBinding::PresetPtr> tubes =
        services.presets().listForType(ComponentPresetType::BODY_TUBE);
    ASSERT_FALSE(tubes.empty());
    // A tick in the parts library: the store changes, no event of the rocket.
    ComponentPresetFavorites::setFavorite(services.preferences(), *tubes.front(),
                                          ComponentPresetType::BODY_TUBE, true);
    services.prompts().answerPreset(
        {.outcome = UserPrompts::PresetOutcome::NONE, .preset = nullptr});
    panel->selectPreset();
    EXPECT_EQ(combo->count(), 1);
    QCoreApplication::processEvents();
    ASSERT_EQ(combo->count(), 2);
    EXPECT_EQ(combo->itemText(1), presetText(*tubes.front()));
    EXPECT_FALSE(design.document->isUndoAvailable());
}

TEST_F(RocketComponentConfigTest, APresetOfTheComboBoxMovesTheFocusToo)
{
    // PresetModel.setSelectedItem() calls setFocusElement() for a preset of the combo box too, and
    // not for "Custom". Java's requestFocusInWindow() gives no focus to a field that is not
    // showing (another tab is selected).
    ConfigDialogServices services;
    services.presets() = QtRocket::Test::smallPresetDatabase();
    const EveryKindDesign design;
    const CreatedPanel    panel =
        createdPanel(*design.document, services, design.component(ComponentKind::LAUNCH_LUG));
    panel.host->show();
    const QWidget* const field = panel->presetFocusField();
    PresetComboBox*      combo = panel->presetComboBox();
    ASSERT_NE(field, nullptr);
    ASSERT_NE(combo, nullptr);
    const std::vector<PresetBinding::PresetPtr> lugs =
        services.presets().listForType(ComponentPresetType::LAUNCH_LUG);
    ASSERT_FALSE(lugs.empty());
    panel->nameField().setFocus();
    EXPECT_TRUE(combo->choosePreset(lugs.front()));
    QCoreApplication::processEvents();
    EXPECT_EQ(panel->focusWidget(), field);

    panel->nameField().setFocus();
    panel->setSelectedTab(QStringLiteral("Override"));
    EXPECT_TRUE(combo->choosePreset(lugs.front()));
    QCoreApplication::processEvents();
    EXPECT_NE(panel->focusWidget(), field);
}

TEST_F(RocketComponentConfigTest, ThePartsLibraryOfAReleasedPanelDoesNotOpen)
{
    // Java's selectPreset() does nothing without its combo box; here neither once the dialog has
    // closed in the meantime (its selection has ended).
    ConfigDialogServices  services;
    const EveryKindDesign design;
    const CreatedPanel    panel =
        createdPanel(*design.document, services, design.component(ComponentKind::BODY_TUBE));
    panel->selectPreset();
    panel->release();
    QCoreApplication::processEvents();
    EXPECT_TRUE(services.prompts().presetRequests().empty());

    // A panel without the row asks nothing either.
    const CreatedPanel fins = createdPanel(*design.document, services,
                                           design.component(ComponentKind::TRAPEZOID_FIN_SET));
    EXPECT_EQ(fins->presetComboBox(), nullptr);
    fins->selectPreset();
    QCoreApplication::processEvents();
    EXPECT_TRUE(services.prompts().presetRequests().empty());
}

}  // namespace

}  // namespace QtRocket::Gui
