#include "QtRocket/gui/configdialog/MotorConfig.h"

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

#include <QChar>
#include <QLabel>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "ConfigDialogTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorConfigurationSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "binding/BindingTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using MotorConfigTest = Test::GuiTest;
using Test::EveryKindDesign;
using Test::PanelHost;
using Test::RocketEventRecorder;

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

TEST_F(MotorConfigTest, TheMountCheckBoxEnablesTheRest)
{
    const EveryKindDesign design;
    auto&       tube = dynamic_cast<BodyTube&>(design.component(ComponentKind::BODY_TUBE));
    PanelHost   host(tube);
    auto const* panel = &host.make<MotorConfig>();
    EXPECT_EQ(panel->mountCheckBox().text(), QStringLiteral("This component is a motor mount"));
    EXPECT_EQ(panel->mountCheckBox().objectName(), QStringLiteral("MotorMount"));
    EXPECT_FALSE(panel->mountCheckBox().isChecked());
    EXPECT_FALSE(panel->mountPanel().isEnabled());
    EXPECT_FALSE(panel->ignitionEventComboBox().isEnabled());
    panel->mountCheckBox().click();
    EXPECT_TRUE(tube.isMotorMount());
    EXPECT_TRUE(panel->mountPanel().isEnabled());
    EXPECT_TRUE(panel->ignitionEventComboBox().isEnabled());
    EXPECT_TRUE(panel->overhangWidgets().spinBox->isEnabled());
}

TEST_F(MotorConfigTest, ShowsJavasFieldsAndNotes)
{
    const EveryKindDesign              design;
    PanelHost                          host(design.component(ComponentKind::INNER_TUBE));
    auto const*                        panel    = &host.make<MotorConfig>();
    const ConfigFields::NumberWidgets& overhang = panel->overhangWidgets();
    ASSERT_NE(overhang.spinBox, nullptr);
    EXPECT_EQ(overhang.spinBox->objectName(), QStringLiteral("MotorOverhang"));
    ASSERT_NE(overhang.slider, nullptr);
    EXPECT_EQ(overhang.slider->mapping().getMin(), -0.02);
    EXPECT_EQ(overhang.slider->mapping().getMax(), 0.06);
    EXPECT_EQ(panel->ignitionEventComboBox().objectName(), QStringLiteral("IgnitionEvent"));
    EXPECT_EQ(panel->ignitionEventComboBox().count(), static_cast<int>(kAllIgnitionEvents.size()));
    ASSERT_NE(panel->ignitionDelayWidgets().spinBox, nullptr);
    EXPECT_EQ(panel->ignitionDelayWidgets().spinBox->objectName(), QStringLiteral("IgnitionDelay"));
    const QString dagger(QChar(0x2020));
    EXPECT_EQ(
        labelTexts(*panel),
        (QStringList{QStringLiteral("Motor overhang:"), QStringLiteral("Ignition at: ") + dagger,
                     QStringLiteral("plus"), QStringLiteral("seconds"),
                     dagger + QStringLiteral(" This parameter can be overridden in each "
                                             "flight configuration."),
                     QStringLiteral("The current design has 2 stages.")}));
}

TEST_F(MotorConfigTest, ADesignOfOneStageSaysHowToAddOne)
{
    const auto document = OpenRocketDocumentFactory::createNewRocket();
    auto&      tube =
        document->getRocket().getChildren().front()->addChild(std::make_unique<BodyTube>());
    PanelHost   host(tube);
    auto const* panel = &host.make<MotorConfig>();
    EXPECT_EQ(panel->stagesLabel().text(),
              QStringLiteral("The current design has only one stage. Stages can be added by "
                             "clicking \"New stage\"."));
}

/// Whether @p types holds an event with every bit of kDefaultMotorEventType.
[[nodiscard]] bool hasMotorEvent(const std::vector<int>& types)
{
    constexpr int kType = MotorConfigurationSet::kDefaultMotorEventType;
    return std::ranges::any_of(types, [](int type) { return (type & kType) == kType; });
}

TEST_F(MotorConfigTest, TheDefaultIgnitionOfEverySelectedMountFiresTheMotorEvent)
{
    // The user's decision U4: OpenRocket's MotorConfiguration setters notify nobody, so the dialog
    // stays unmodified and Cancel cannot undo them; here the mount fires the event the core
    // prescribes.
    const EveryKindDesign design;
    auto& tube  = dynamic_cast<BodyTube&>(design.component(ComponentKind::BODY_TUBE));
    auto& other = *design.boosterTube;
    tube.setMotorMount(true);
    other.setMotorMount(true);
    const std::array<RocketComponent*, 1> others{&other};
    PanelHost                             host(tube, others);
    auto const*                           panel = &host.make<MotorConfig>();
    RocketEventRecorder                   events(design.document->getRocket());
    const int                             burnout = static_cast<int>(IgnitionEvent::BURNOUT);
    Q_EMIT panel->ignitionEventComboBox().activated(burnout);
    EXPECT_EQ(tube.getDefaultMotorConfig().getIgnitionEvent(), IgnitionEvent::BURNOUT);
    EXPECT_EQ(other.getDefaultMotorConfig().getIgnitionEvent(), IgnitionEvent::BURNOUT);
    EXPECT_TRUE(hasMotorEvent(events.types()));

    // The value shown already: nothing set, nothing fired (EnumBinding's SKIP).
    events.clear();
    Q_EMIT panel->ignitionEventComboBox().activated(burnout);
    EXPECT_EQ(events.count(), 0U);

    ASSERT_TRUE(panel->ignitionDelayWidgets().spinBox->binding()->commitText("2"));
    EXPECT_EQ(tube.getDefaultMotorConfig().getIgnitionDelay(), 2.0);
    EXPECT_EQ(other.getDefaultMotorConfig().getIgnitionDelay(), 2.0);
    EXPECT_TRUE(hasMotorEvent(events.types()));
    // At least 0.
    ASSERT_TRUE(panel->ignitionDelayWidgets().spinBox->binding()->commitText("-1"));
    EXPECT_EQ(tube.getDefaultMotorConfig().getIgnitionDelay(), 0.0);
}

TEST_F(MotorConfigTest, AComponentThatIsNoMountIsABug)
{
    const EveryKindDesign design;
    PanelHost             host(design.component(ComponentKind::NOSE_CONE));
    EXPECT_THROW(static_cast<void>(&host.make<MotorConfig>()), BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
