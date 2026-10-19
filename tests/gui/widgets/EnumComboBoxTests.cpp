// The combo box of an enum property (Java: a JComboBox on an EnumModel or a TransitionShapeModel).
//
// Expectations marked "probe" are OpenRocket's own texts, the toString() of the enums the
// dialogs' combo boxes show, printed by the Java probe of this part (scratchpad
// probes/m2r2-B4/java/src/probe/B4Probe.java -> b4-probe.out, run on OpenRocket 5f164fd0e with
// JDK 17, English messages).

#include "QtRocket/gui/widgets/EnumComboBox.h"

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <tuple>

#include <QComboBox>
#include <QLabel>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/TransitionShapeBinding.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "RecordingGuardHook.h"
#include "WidgetTestSupport.h"
#include "binding/BindingTestSupport.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui
{

namespace
{

using EnumComboBoxTest = Test::GuiTest;
using Test::RecordingGuardHook;
using Test::showActiveWith;
using Test::turnWheel;
using Test::TwoTubeRocket;
using DeployEvent = DeploymentConfiguration::DeployEvent;

/// The source of a component's axial method (Java: PlacementPanel's "AxialMethod").
[[nodiscard]] EnumSource<AxialMethod> axialMethod()
{
    return EnumSource<AxialMethod>::of<RocketComponent>({
        .get = [](const RocketComponent& component) { return component.getAxialMethod(); },
        .set = [](RocketComponent&   component,
                  const AxialMethod& method) { component.setAxialMethod(method); },
    });
}

/// The source of an external component's finish (Java: MaterialPanel's "Finish").
[[nodiscard]] EnumSource<Finish> finish()
{
    return EnumSource<Finish>::of<ExternalComponent>({
        .get = [](const ExternalComponent& component) { return component.getFinish(); },
        .set = [](ExternalComponent& component,
                  const Finish&      value) { component.setFinish(value); },
    });
}

/// A deployment event that is not a component's, with a count of the setter's calls.
struct DeployBean
{
    DeployEvent value{DeployEvent::EJECTION};
    int         sets{0};

    [[nodiscard]] EnumSource<DeployEvent> source()
    {
        return EnumSource<DeployEvent>::of<RocketComponent>({
            .get = [this](const RocketComponent&) { return value; },
            .set =
                [this](RocketComponent& component, const DeployEvent& event) {
                    ++sets;
                    value = event;
                    component.fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
                },
        });
    }
};

/// The items of @p combo.
[[nodiscard]] QStringList items(const QComboBox& combo)
{
    QStringList texts{};
    for (int index = 0; index < combo.count(); ++index)
    {
        texts.append(combo.itemText(index));
    }
    return texts;
}

/// A binding of @p source on @p component alone, offering @p values.
template <class E, std::size_t N>
[[nodiscard]] std::shared_ptr<EnumBinding<E>> bindOn(RocketComponent&        component,
                                                     const EnumSource<E>&    source,
                                                     const std::array<E, N>& values)
{
    return std::make_shared<EnumBinding<E>>(std::make_shared<ComponentSelection>(component), source,
                                            values);
}

TEST_F(EnumComboBoxTest, NeedsABindingAndATextFunction)
{
    const TwoTubeRocket design;
    EXPECT_THROW(std::ignore = EnumComboBox(std::shared_ptr<EnumBinding<AxialMethod>>(), nullptr),
                 BugError);
    EXPECT_THROW(
        std::ignore = EnumComboBox(bindOn(*design.tubeA, axialMethod(), kAxialOffsetMethods),
                                   std::function<QString(AxialMethod)>(), nullptr),
        BugError);
}

TEST_F(EnumComboBoxTest, TheAxialMethodsAsOpenRocket)
{
    // probe: AxialMethod.axialOffsetMethods: Tip of the rocket | Top of the parent component |
    // Middle of the parent component | Bottom of the parent component.
    const TwoTubeRocket design;
    const EnumComboBox  combo(bindOn(*design.tubeA, axialMethod(), kAxialOffsetMethods), nullptr);
    EXPECT_EQ(items(combo), (QStringList{QStringLiteral("Tip of the rocket"),
                                         QStringLiteral("Top of the parent component"),
                                         QStringLiteral("Middle of the parent component"),
                                         QStringLiteral("Bottom of the parent component")}));
    EXPECT_EQ(combo.focusPolicy(), Qt::StrongFocus);  // never the wheel's focus
    EXPECT_TRUE(combo.isEnabled());
}

TEST_F(EnumComboBoxTest, TheMassComponentTypesAsOpenRocket)
{
    // probe: MassComponentType: Mass Component | Altimeter | Flight Computer | Deployment Charge |
    // Tracker | Payload | Recovery Hardware | Battery.
    const TwoTubeRocket design;
    auto&               mass = design.tubeA->addChild(std::make_unique<MassComponent>());
    const auto          type = EnumSource<MassComponent::MassComponentType>::of<MassComponent>({
        .get = [](const MassComponent& part) { return part.getMassComponentType(); },
        .set =
            [](MassComponent& part, const MassComponent::MassComponentType& value) {
                part.setMassComponentType(value);
            },
    });
    const EnumComboBox  combo(bindOn(mass, type, MassComponent::kAllMassComponentTypes), nullptr);
    EXPECT_EQ(items(combo),
              (QStringList{QStringLiteral("Mass Component"), QStringLiteral("Altimeter"),
                           QStringLiteral("Flight Computer"), QStringLiteral("Deployment Charge"),
                           QStringLiteral("Tracker"), QStringLiteral("Payload"),
                           QStringLiteral("Recovery Hardware"), QStringLiteral("Battery")}));
    EXPECT_EQ(combo.shownText(), QStringLiteral("Mass Component"));
}

TEST_F(EnumComboBoxTest, TheTransitionShapesAsOpenRocket)
{
    // probe: Transition.Shape: Conical | Ogive | Ellipsoid | Power series | Parabolic series |
    // Haack series.
    const TwoTubeRocket design;
    auto&               transition = design.stage->addChild(std::make_unique<Transition>());
    const auto          binding =
        makeTransitionShapeBinding(std::make_shared<ComponentSelection>(transition));
    EnumComboBox combo(binding, nullptr);
    EXPECT_EQ(items(combo),
              (QStringList{QStringLiteral("Conical"), QStringLiteral("Ogive"),
                           QStringLiteral("Ellipsoid"), QStringLiteral("Power series"),
                           QStringLiteral("Parabolic series"), QStringLiteral("Haack series")}));
    EXPECT_EQ(combo.enumBinding<TransitionShape>(), binding);
    EXPECT_EQ(combo.enumBinding<AxialMethod>(), nullptr);
    Q_EMIT combo.activated(4);
    EXPECT_EQ(transition.getShapeType(), TransitionShape::PARABOLIC);
    EXPECT_EQ(combo.currentIndex(), 4);
}

TEST_F(EnumComboBoxTest, AFinishShowsItsRoughnessInTheCurrentUnit)
{
    // probe: Finish: Rough (500 um) | Rough unfinished (250 um) | Unfinished (150 um) | Regular
    // paint (60 um) | Smooth paint (20 um) | Optimum paint (5 um) | Aircraft sheet-metal (2 um) |
    // Finished/polished surface (0.5 um) | Mirror surface (0 um), the micro sign U+00B5 for u.
    const TwoTubeRocket design;
    const EnumComboBox  combo(
        bindOn(*design.tubeA, finish(), kAllFinishes),
        [](Finish value) { return QtStrings::fromUtf8(toString(value)); }, nullptr);
    const QString micro = QStringLiteral(" \u00b5m)");
    EXPECT_EQ(
        items(combo),
        (QStringList{
            QStringLiteral("Rough (500") + micro, QStringLiteral("Rough unfinished (250") + micro,
            QStringLiteral("Unfinished (150") + micro, QStringLiteral("Regular paint (60") + micro,
            QStringLiteral("Smooth paint (20") + micro, QStringLiteral("Optimum paint (5") + micro,
            QStringLiteral("Aircraft sheet-metal (2") + micro,
            QStringLiteral("Finished/polished surface (0.5") + micro,
            QStringLiteral("Mirror surface (0") + micro}));
    EXPECT_EQ(combo.shownText(), QStringLiteral("Regular paint (60") + micro);
}

TEST_F(EnumComboBoxTest, RefreshReadsTheTextsAnew)
{
    // A unit change fires nothing: refresh() reads the texts anew (the preferences' Apply).
    const QtRocket::Test::DefaultUnitsGuard units;
    const TwoTubeRocket                     design;
    EnumComboBox                            combo(
        bindOn(*design.tubeA, finish(), kAllFinishes),
        [](Finish value) { return QtStrings::fromUtf8(toString(value)); }, nullptr);
    const QString before = combo.itemText(0);
    ASSERT_TRUE(unitGroup(UnitGroupId::ROUGHNESS).setDefaultUnit("mil"));
    EXPECT_EQ(combo.itemText(0), before);
    combo.refresh();
    EXPECT_TRUE(combo.itemText(0).endsWith(QStringLiteral(" mil)")))
        << combo.itemText(0).toStdString();
    EXPECT_EQ(combo.count(), 9);
    EXPECT_EQ(combo.currentIndex(), 3);
}

TEST_F(EnumComboBoxTest, AChoiceSetsEverySelectedComponentAndSkipsTheSameValue)
{
    // Java's EnumModel: no duplicate writes (picking the main's value leaves the others alone).
    const TwoTubeRocket design;
    design.tubeB->setAxialMethod(AxialMethod::TOP);
    const auto selection = std::make_shared<ComponentSelection>(
        *design.tubeA, std::array<RocketComponent*, 1>{design.tubeB});
    EnumComboBox combo(
        std::make_shared<EnumBinding<AxialMethod>>(selection, axialMethod(), kAxialOffsetMethods),
        nullptr);
    EXPECT_EQ(design.tubeA->getAxialMethod(), AxialMethod::AFTER);
    EXPECT_EQ(combo.currentIndex(), -1);  // AFTER is not offered: shown apart
    EXPECT_EQ(combo.shownText(), QStringLiteral("After the sibling component"));
    EXPECT_EQ(combo.currentText(), QString());

    Q_EMIT combo.activated(3);  // Bottom
    EXPECT_EQ(design.tubeA->getAxialMethod(), AxialMethod::BOTTOM);
    EXPECT_EQ(design.tubeB->getAxialMethod(), AxialMethod::BOTTOM);
    EXPECT_EQ(combo.currentIndex(), 3);
    EXPECT_EQ(combo.shownText(), QStringLiteral("Bottom of the parent component"));

    design.tubeB->setAxialMethod(AxialMethod::TOP);
    Q_EMIT combo.activated(3);  // the main's value: nothing
    EXPECT_EQ(design.tubeB->getAxialMethod(), AxialMethod::TOP);
}

TEST_F(EnumComboBoxTest, AHiddenValueIsShownApart)
{
    // probe: DeployEvent: Launch (plus NN seconds) | First ejection charge of this stage | Apogee |
    // Specific altitude during descent | Lower stage separation | Never. The bottom stage's
    // recovery devices hide the lower stage's separation (ParachuteConfig, StreamerConfig).
    const TwoTubeRocket design;
    DeployBean          bean{.value = DeployEvent::LOWER_STAGE_SEPARATION};
    const auto          binding =
        bindOn(*design.tubeA, bean.source(), DeploymentConfiguration::kAllDeployEvents);
    EnumComboBox combo(binding, nullptr);
    EXPECT_EQ(combo.count(), 6);
    EXPECT_EQ(combo.currentIndex(), 4);
    binding->removeElement(DeployEvent::LOWER_STAGE_SEPARATION);
    EXPECT_EQ(items(combo),
              (QStringList{
                  QStringLiteral("Launch (plus NN seconds)"),
                  QStringLiteral("First ejection charge of this stage"), QStringLiteral("Apogee"),
                  QStringLiteral("Specific altitude during descent"), QStringLiteral("Never")}));
    EXPECT_EQ(combo.currentIndex(), -1);
    EXPECT_EQ(combo.shownText(), QStringLiteral("Lower stage separation"));

    Q_EMIT combo.activated(2);
    EXPECT_EQ(bean.value, DeployEvent::APOGEE);
    EXPECT_EQ(combo.currentIndex(), 2);
    EXPECT_EQ(combo.shownText(), QStringLiteral("Apogee"));
}

TEST_F(EnumComboBoxTest, FollowsChangesElsewhere)
{
    // An inner component, whose setAxialMethod() fires a change (InternalComponent; a stage's
    // direct children are placed after their sibling whatever their method).
    const TwoTubeRocket design;
    auto&               mass = design.tubeA->addChild(std::make_unique<MassComponent>());
    const EnumComboBox  combo(bindOn(mass, axialMethod(), kAllAxialMethods), nullptr);
    EXPECT_EQ(combo.currentIndex(), 2);  // Top of the parent component
    mass.setAxialMethod(AxialMethod::MIDDLE);
    EXPECT_EQ(combo.currentIndex(), 3);
    EXPECT_EQ(combo.shownText(), QStringLiteral("Middle of the parent component"));
}

TEST_F(EnumComboBoxTest, TheBindingAndTheFieldEnableItTogether)
{
    const TwoTubeRocket design;
    EnumComboBox        combo(bindOn(*design.tubeA, axialMethod(), kAllAxialMethods), nullptr);
    combo.setFieldEnabled(false);
    EXPECT_FALSE(combo.isEnabled());
    EXPECT_FALSE(combo.isFieldEnabled());
    design.tubeA->setAxialMethod(AxialMethod::TOP);  // the binding allows it, the dialog not
    EXPECT_FALSE(combo.isEnabled());
    combo.setFieldEnabled(true);
    EXPECT_TRUE(combo.isEnabled());
}

TEST_F(EnumComboBoxTest, AnInvalidSelectionKeepsTheLastValueAndTakesNothing)
{
    const TwoTubeRocket design;
    DeployBean          bean;
    EnumComboBox        combo(
        bindOn(*design.tubeA, bean.source(), DeploymentConfiguration::kAllDeployEvents), nullptr);
    std::ignore = design.stage->addChild(std::make_unique<BodyTube>());
    EXPECT_FALSE(combo.isEnabled());
    EXPECT_EQ(combo.shownText(), QStringLiteral("First ejection charge of this stage"));
    Q_EMIT combo.activated(0);
    EXPECT_EQ(bean.sets, 0);
    EXPECT_EQ(combo.currentIndex(), 1);
}

TEST_F(EnumComboBoxTest, TheWheelChoosesOnlyWithTheFocus)
{
    const TwoTubeRocket design;
    QWidget             window;
    auto*               combo =
        makeChild<EnumComboBox>(&window, bindOn(*design.tubeA, axialMethod(), kAllAxialMethods));
    auto* other = makeChild<QLabel>(&window);
    other->setFocusPolicy(Qt::StrongFocus);
    ASSERT_TRUE(showActiveWith(window, {combo, other}));
    other->setFocus();
    EXPECT_FALSE(turnWheel(*combo, -120));
    EXPECT_EQ(design.tubeA->getAxialMethod(), AxialMethod::AFTER);
    EXPECT_FALSE(combo->hasFocus());  // the wheel takes no focus

    // With the focus, Qt's rule: one notch down is the next value, where the style scrolls a
    // combo box by the wheel at all (not the macOS style).
    combo->setFocus();
    const bool scrolls =
        combo->style()->styleHint(QStyle::SH_ComboBox_AllowWheelScrolling, nullptr, combo) != 0;
    static_cast<void>(turnWheel(*combo, -120));
    EXPECT_EQ(design.tubeA->getAxialMethod(), scrolls ? AxialMethod::TOP : AxialMethod::AFTER);
}

TEST_F(EnumComboBoxTest, ACoreErrorOfAChoiceIsReported)
{
    const TwoTubeRocket      design;
    const RecordingGuardHook hook;
    const auto               throwing = EnumSource<AxialMethod>::of<RocketComponent>({
        .get = [](const RocketComponent& c) { return c.getAxialMethod(); },
        .set = [](RocketComponent&, const AxialMethod&) { bug("a broken setter"); },
    });
    EnumComboBox             combo(bindOn(*design.tubeA, throwing, kAllAxialMethods), nullptr);
    combo.setObjectName(QStringLiteral("AxialMethod"));
    Q_EMIT combo.activated(0);
    ASSERT_EQ(hook.failures().size(), 1U);
    EXPECT_EQ(hook.failures().front().context, QStringLiteral("AxialMethod"));
    EXPECT_EQ(combo.currentIndex(), 1);  // the binding's value again
}

}  // namespace

}  // namespace QtRocket::Gui
