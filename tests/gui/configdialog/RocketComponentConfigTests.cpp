#include "QtRocket/gui/configdialog/RocketComponentConfig.h"

#include <array>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include <QFont>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
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
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"
#include "QtRocket/gui/widgets/IconToggleButton.h"
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
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "aero/BarrowmanTestRockets.h"
#include "document/TestDocuments.h"

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
    OneTubeDesign               design;
    auto&                       fins   = design.body->addChild(std::make_unique<TrapezoidFinSet>());
    const std::optional<double> extent = RocketComponentConfig::overrideCGExtent(fins);
    ASSERT_TRUE(extent.has_value());
    EXPECT_EQ(extent.value_or(-1.0), fins.getInstanceBoundingBox().span().x);
}

TEST_F(RocketComponentConfigTest, TheCGExtentKeepsJavasStartOfTheMaximum)
{
    // Java starts maxL at Double.MIN_VALUE, the smallest POSITIVE double: a subtree that ends
    // before 0 spans from its front to 0.
    OneTubeDesign design;
    auto&         inner = design.body->addChild(std::make_unique<InnerTube>());
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

    auto* page = makeChild<QWidget>(panel.get());
    auto* late = makeChild<QLineEdit>(page);
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
    auto selection =
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

}  // namespace

}  // namespace QtRocket::Gui
