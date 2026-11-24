// The unit of a number field and the choice of another (Java: UnitSelector on a DoubleModel), and
// the choice of a unit of a bare unit group (Java: UnitSelector(UnitGroup)).
//
// Expectations marked "probe" are OpenRocket's own results, printed by the Java probe
// tools/openrocket-gui-pins/plot-widgets/probe/PlotWidgetsProbe.java (OpenRocket 5f164fd0e, JDK 17;
// its transcript is plot-widgets/hand-pinned.txt): a UnitSelector(UnitGroup.UNITS_LENGTH), its
// texts, selected units and item events.

#include "QtRocket/gui/widgets/UnitSelector.h"

#include <memory>
#include <string>
#include <vector>

#include <QAction>
#include <QList>
#include <QMenu>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QToolButton>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "WidgetTestSupport.h"
#include "binding/BindingTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using UnitSelectorTest = Test::GuiTest;
using Test::BeanProperty;
using Test::bodyTubeLength;
using Test::OffscreenPopupWarnings;
using Test::showActiveWith;
using Test::TwoTubeRocket;

/// A binding of a body tube's length on @p component, in the length units.
[[nodiscard]] std::shared_ptr<DoubleBinding> lengthBinding(RocketComponent& component)
{
    return std::make_shared<DoubleBinding>(std::make_shared<ComponentSelection>(component),
                                           bodyTubeLength(), unitGroup(UnitGroupId::LENGTH),
                                           DoubleBinding::Options{});
}

/// The texts of the items of @p menu.
[[nodiscard]] std::vector<QString> itemTexts(const QMenu& menu)
{
    std::vector<QString> texts;
    for (const QAction* const action : menu.actions())
    {
        texts.push_back(action->text());
    }
    return texts;
}

/// The symbols of the units of @p group, in the group's order.
[[nodiscard]] std::vector<QString> unitSymbols(const UnitGroup& group)
{
    std::vector<QString> symbols;
    for (const Unit* const unit : group.getUnits())
    {
        symbols.push_back(QtStrings::unitSymbol(*unit));
    }
    return symbols;
}

/// The item of @p menu with the text @p text, or null.
[[nodiscard]] QAction* menuItem(const QMenu& menu, const QString& text)
{
    for (QAction* const action : menu.actions())
    {
        if (action->text() == text)
        {
            return action;
        }
    }
    return nullptr;
}

/// Whether a press of @p button on @p selector opens its menu (which is closed again).
[[nodiscard]] bool opensMenu(UnitSelector& selector, Qt::MouseButton button)
{
    QTest::mouseClick(&selector, button);
    QMenu* const menu   = selector.unitMenu();
    const bool   opened = menu != nullptr && menu->isVisible();
    if (menu != nullptr)
    {
        menu->hide();
    }
    return opened;
}

/// A shown, active window holding the spin box and the unit selector of the first of two
/// bindings and the unit selector of the second.
class Window
{
public:
    Window(const std::shared_ptr<DoubleBinding>& first,
           const std::shared_ptr<DoubleBinding>& second)
      : m_firstSpin(makeChild<UnitSpinBox>(&m_window, first)),
        m_first(makeChild<UnitSelector>(&m_window, first)),
        m_second(makeChild<UnitSelector>(&m_window, second)),
        m_active(showActiveWith(m_window, {m_firstSpin, m_first, m_second}))
    {
    }

    [[nodiscard]] bool          isActive() const noexcept { return m_active; }
    [[nodiscard]] UnitSpinBox&  firstSpin() const noexcept { return *m_firstSpin; }
    [[nodiscard]] UnitSelector& first() const noexcept { return *m_first; }
    [[nodiscard]] UnitSelector& second() const noexcept { return *m_second; }

private:
    QWidget       m_window;
    UnitSpinBox*  m_firstSpin{nullptr};
    UnitSelector* m_first{nullptr};
    UnitSelector* m_second{nullptr};
    bool          m_active{false};
};

TEST_F(UnitSelectorTest, NeedsABinding)
{
    EXPECT_THROW(static_cast<void>(UnitSelector(nullptr, nullptr)), BugError);
    const TwoTubeRocket rocket;
    UnitSelector        selector(lengthBinding(*rocket.tubeA), nullptr);
    EXPECT_THROW(selector.setBinding(nullptr), BugError);
}

TEST_F(UnitSelectorTest, ShowsTheCurrentUnitOnAFlatButtonWithoutFocus)
{
    const TwoTubeRocket rocket;
    const UnitSelector  selector(lengthBinding(*rocket.tubeA), nullptr);
    EXPECT_EQ(selector.text(), QStringLiteral("cm"));
    EXPECT_TRUE(selector.autoRaise());
    EXPECT_EQ(selector.toolButtonStyle(), Qt::ToolButtonTextOnly);
    EXPECT_EQ(selector.focusPolicy(), Qt::NoFocus);
    EXPECT_EQ(selector.contextMenuPolicy(), Qt::PreventContextMenu);
    EXPECT_FALSE(selector.testAttribute(Qt::WA_TransparentForMouseEvents));
    EXPECT_TRUE(selector.isEnabled());
}

TEST_F(UnitSelectorTest, AnyMouseButtonOpensTheMenuOfTheGroup)
{
    // Java: mouseClicked() of any button opens the popup of the group's units, in order.
    const TwoTubeRocket          rocket;
    const auto                   binding = lengthBinding(*rocket.tubeA);
    const Window                 window(binding, lengthBinding(*rocket.tubeB));
    const OffscreenPopupWarnings popup;
    ASSERT_TRUE(window.isActive());
    EXPECT_TRUE(opensMenu(window.first(), Qt::LeftButton));
    EXPECT_TRUE(opensMenu(window.first(), Qt::RightButton));
    EXPECT_TRUE(opensMenu(window.first(), Qt::MiddleButton));
    ASSERT_NE(window.first().unitMenu(), nullptr);
    EXPECT_EQ(itemTexts(*window.first().unitMenu()), unitSymbols(unitGroup(UnitGroupId::LENGTH)));
    EXPECT_FALSE(window.first().hasFocus());
    EXPECT_EQ(&binding->getCurrentUnit(), &unitGroup(UnitGroupId::LENGTH).getDefaultUnit());
}

TEST_F(UnitSelectorTest, TheUnitChosenIsThatOfItsBindingAlone)
{
    // The spin box of the binding follows; the other field, the group's default and the next
    // field made keep the default unit (Java: setCurrentUnit() of this model only).
    const TwoTubeRocket          rocket;
    const auto                   first  = lengthBinding(*rocket.tubeA);
    const auto                   second = lengthBinding(*rocket.tubeB);
    const Window                 window(first, second);
    const OffscreenPopupWarnings popup;
    ASSERT_TRUE(window.isActive());
    QTest::mouseClick(&window.first(), Qt::LeftButton);
    QMenu* const menu = window.first().unitMenu();
    ASSERT_NE(menu, nullptr);
    QAction* const millimetres = menuItem(*menu, QStringLiteral("mm"));
    ASSERT_NE(millimetres, nullptr);
    QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                      menu->actionGeometry(millimetres).center());
    EXPECT_FALSE(menu->isVisible());
    EXPECT_EQ(first->getCurrentUnit().getUnit(), "mm");
    EXPECT_EQ(window.first().text(), QStringLiteral("mm"));
    EXPECT_EQ(window.firstSpin().text(), QStringLiteral("300"));
    EXPECT_EQ(second->getCurrentUnit().getUnit(), "cm");
    EXPECT_EQ(window.second().text(), QStringLiteral("cm"));
    EXPECT_EQ(unitGroup(UnitGroupId::LENGTH).getDefaultUnit().getUnit(), "cm");
    EXPECT_EQ(lengthBinding(*rocket.tubeA)->getCurrentUnit().getUnit(), "cm");
}

TEST_F(UnitSelectorTest, FollowsAUnitChangeOfItsBinding)
{
    const TwoTubeRocket rocket;
    const auto          binding = lengthBinding(*rocket.tubeA);
    const UnitSelector  selector(binding, nullptr);
    binding->setCurrentUnit(*unitGroup(UnitGroupId::LENGTH).getUnit("in"));
    EXPECT_EQ(selector.text(), QStringLiteral("in"));
}

TEST_F(UnitSelectorTest, AGroupOfOneUnitOffersNoChoice)
{
    // Java: no popup and no hover border when the group has one unit.
    const TwoTubeRocket rocket;
    BeanProperty        bean{.value = 0.5};
    UnitSelector selector(std::make_shared<DoubleBinding>(
                              std::make_shared<ComponentSelection>(*rocket.tubeA), bean.source(),
                              unitGroup(UnitGroupId::NONE), DoubleBinding::Options{}),
                          nullptr);
    ASSERT_EQ(unitGroup(UnitGroupId::NONE).getUnitCount(), 1);
    EXPECT_TRUE(selector.testAttribute(Qt::WA_TransparentForMouseEvents));
    selector.showUnitMenu();
    ASSERT_NE(selector.unitMenu(), nullptr);
    EXPECT_FALSE(selector.unitMenu()->isVisible());
    EXPECT_TRUE(selector.unitMenu()->actions().isEmpty());
}

TEST_F(UnitSelectorTest, TheBindingAndTheFieldEnableIt)
{
    const TwoTubeRocket rocket;
    BeanProperty        bean{.value = 0.5};
    UnitSelector        selector(
        std::make_shared<DoubleBinding>(std::make_shared<ComponentSelection>(*rocket.tubeA),
                                        bean.checkedSource(), unitGroup(UnitGroupId::LENGTH),
                                        DoubleBinding::Options{}),
        nullptr);
    selector.setFieldEnabled(false);  // the follower of an "override" check box
    EXPECT_FALSE(selector.isEnabled());
    EXPECT_FALSE(selector.isFieldEnabled());
    selector.showUnitMenu();
    EXPECT_FALSE(selector.unitMenu()->isVisible());
    selector.setFieldEnabled(true);
    EXPECT_TRUE(selector.isEnabled());
    bean.enabled = false;
    rocket.tubeA->setLength(0.4);  // any change of the rocket
    EXPECT_FALSE(selector.isEnabled());
}

TEST_F(UnitSelectorTest, AnotherBindingTakesThePlaceOfTheFirst)
{
    const TwoTubeRocket rocket;
    const auto          first = lengthBinding(*rocket.tubeA);
    UnitSelector        selector(first, nullptr);
    BeanProperty        bean{.value = 0.5};
    const auto          mass = std::make_shared<DoubleBinding>(
        std::make_shared<ComponentSelection>(*rocket.tubeA), bean.source(),
        unitGroup(UnitGroupId::MASS), DoubleBinding::Options{});
    selector.setBinding(mass);
    EXPECT_EQ(selector.binding(), mass);
    EXPECT_EQ(selector.text(),
              QtStrings::unitSymbol(unitGroup(UnitGroupId::MASS).getDefaultUnit()));
    first->setCurrentUnit(*unitGroup(UnitGroupId::LENGTH).getUnit("mm"));
    EXPECT_EQ(selector.text(),
              QtStrings::unitSymbol(unitGroup(UnitGroupId::MASS).getDefaultUnit()));
}

TEST_F(UnitSelectorTest, ABareGroupStartsAtItsDefaultUnit)
{
    // probe: units: mm cm m in in/64 ft; default=cm; text='cm' selected=cm model=null
    const UnitGroup&   length = unitGroup(UnitGroupId::LENGTH);
    const UnitSelector selector(length, nullptr);
    EXPECT_EQ(unitSymbols(length),
              (std::vector<QString>{QStringLiteral("mm"), QStringLiteral("cm"), QStringLiteral("m"),
                                    QStringLiteral("in"), QStringLiteral("in/64"),
                                    QStringLiteral("ft")}));
    EXPECT_EQ(selector.text(), QStringLiteral("cm"));
    EXPECT_EQ(&selector.getSelectedUnit(), &length.getDefaultUnit());
    EXPECT_EQ(&selector.getUnitGroup(), &length);
    EXPECT_EQ(selector.binding(), nullptr);
    EXPECT_TRUE(selector.autoRaise());
    EXPECT_EQ(selector.focusPolicy(), Qt::NoFocus);
    EXPECT_EQ(selector.contextMenuPolicy(), Qt::PreventContextMenu);
    EXPECT_FALSE(selector.testAttribute(Qt::WA_TransparentForMouseEvents));
    EXPECT_TRUE(selector.isEnabled());
}

TEST_F(UnitSelectorTest, EverySelectionOfAUnitIsToldOf)
{
    // probe: setSelectedUnit(in): text='in' selected=in events=[SELECTED in] default=cm; again:
    // events=[SELECTED in].
    const UnitGroup& length = unitGroup(UnitGroupId::LENGTH);
    UnitSelector     selector(length, nullptr);
    const QSignalSpy selections(&selector, &UnitSelector::unitSelected);
    const Unit*      inch = length.getUnit("in");
    ASSERT_NE(inch, nullptr);
    selector.setSelectedUnit(*inch);
    EXPECT_EQ(selector.text(), QStringLiteral("in"));
    EXPECT_EQ(&selector.getSelectedUnit(), inch);
    EXPECT_EQ(selections.count(), 1);
    EXPECT_EQ(length.getDefaultUnit().getUnit(), "cm");
    selector.setSelectedUnit(*inch);
    EXPECT_EQ(selections.count(), 2);
}

TEST_F(UnitSelectorTest, AnotherGroupStartsAtItsDefaultUnitAndTellsOfNothing)
{
    // probe: setUnitGroup(UNITS_LENGTH), the same: text='in' selected=in events=[];
    // setUnitGroup(UNITS_DISTANCE): units: m km ft yd mi nmi; text='m' selected=m events=[].
    const UnitGroup& length   = unitGroup(UnitGroupId::LENGTH);
    const UnitGroup& distance = unitGroup(UnitGroupId::DISTANCE);
    UnitSelector     selector(length, nullptr);
    selector.setSelectedUnit(*length.getUnit("in"));
    const QSignalSpy selections(&selector, &UnitSelector::unitSelected);
    selector.setUnitGroup(length);
    EXPECT_EQ(selector.text(), QStringLiteral("in"));
    selector.setUnitGroup(distance);
    EXPECT_EQ(&selector.getUnitGroup(), &distance);
    EXPECT_EQ(selector.text(), QStringLiteral("m"));
    EXPECT_EQ(&selector.getSelectedUnit(), &distance.getDefaultUnit());
    EXPECT_EQ(selections.count(), 0);
    EXPECT_EQ(
        unitSymbols(distance),
        (std::vector<QString>{QStringLiteral("m"), QStringLiteral("km"), QStringLiteral("ft"),
                              QStringLiteral("yd"), QStringLiteral("mi"), QStringLiteral("nmi")}));
}

TEST_F(UnitSelectorTest, AUnitOfAnotherGroupIsAProgrammingError)
{
    // probe: setSelectedUnit(in) of another group: IllegalArgumentException, text='m' events=[];
    // setUnitGroup() of a model's selector: IllegalStateException.
    UnitSelector     selector(unitGroup(UnitGroupId::DISTANCE), nullptr);
    const QSignalSpy selections(&selector, &UnitSelector::unitSelected);
    EXPECT_THROW(selector.setSelectedUnit(*unitGroup(UnitGroupId::LENGTH).getUnit("in")), BugError);
    EXPECT_EQ(selector.text(), QStringLiteral("m"));
    EXPECT_EQ(selections.count(), 0);

    const TwoTubeRocket rocket;
    UnitSelector        bound(lengthBinding(*rocket.tubeA), nullptr);
    EXPECT_THROW(bound.setUnitGroup(unitGroup(UnitGroupId::DISTANCE)), BugError);
}

TEST_F(UnitSelectorTest, TheMenuChoosesTheUnitOfABareGroup)
{
    QWidget window;
    auto*   selector = makeChild<UnitSelector>(&window, unitGroup(UnitGroupId::LENGTH));
    ASSERT_TRUE(showActiveWith(window, {selector}));
    const QSignalSpy             selections(selector, &UnitSelector::unitSelected);
    const OffscreenPopupWarnings popup;
    EXPECT_TRUE(opensMenu(*selector, Qt::LeftButton));
    ASSERT_NE(selector->unitMenu(), nullptr);
    EXPECT_EQ(itemTexts(*selector->unitMenu()), unitSymbols(unitGroup(UnitGroupId::LENGTH)));
    QAction* const feet = menuItem(*selector->unitMenu(), QStringLiteral("ft"));
    ASSERT_NE(feet, nullptr);
    feet->trigger();
    EXPECT_EQ(selector->getSelectedUnit().getUnit(), "ft");
    EXPECT_EQ(selector->text(), QStringLiteral("ft"));
    EXPECT_EQ(selections.count(), 1);
}

TEST_F(UnitSelectorTest, ABareGroupOfOneUnitOffersNoChoice)
{
    // probe: UnitSelector(UNITS_NONE): units=1 text='\u200b'
    UnitSelector selector(unitGroup(UnitGroupId::NONE), nullptr);
    EXPECT_EQ(selector.text(), QStringLiteral("\u200b"));
    EXPECT_TRUE(selector.testAttribute(Qt::WA_TransparentForMouseEvents));
    selector.showUnitMenu();
    EXPECT_FALSE(selector.unitMenu()->isVisible());
}

TEST_F(UnitSelectorTest, OverABindingTheSelectedUnitIsTheBindings)
{
    const TwoTubeRocket rocket;
    const auto          binding = lengthBinding(*rocket.tubeA);
    UnitSelector        selector(binding, nullptr);
    const QSignalSpy    selections(&selector, &UnitSelector::unitSelected);
    EXPECT_EQ(&selector.getUnitGroup(), &binding->getUnitGroup());
    EXPECT_EQ(&selector.getSelectedUnit(), &binding->getCurrentUnit());
    selector.setSelectedUnit(*unitGroup(UnitGroupId::LENGTH).getUnit("mm"));
    EXPECT_EQ(binding->getCurrentUnit().getUnit(), "mm");
    EXPECT_EQ(selector.text(), QStringLiteral("mm"));
    EXPECT_EQ(selections.count(), 1);
}

TEST_F(UnitSelectorTest, ABindingTakesThePlaceOfTheBareGroup)
{
    // Java: setModel() on a selector of a unit group.
    const TwoTubeRocket rocket;
    UnitSelector        selector(unitGroup(UnitGroupId::MASS), nullptr);
    const auto          binding = lengthBinding(*rocket.tubeA);
    selector.setBinding(binding);
    EXPECT_EQ(selector.binding(), binding);
    EXPECT_EQ(&selector.getUnitGroup(), &unitGroup(UnitGroupId::LENGTH));
    EXPECT_EQ(selector.text(), QStringLiteral("cm"));
}

TEST_F(UnitSelectorTest, TheFieldEnablesABareSelector)
{
    UnitSelector selector(unitGroup(UnitGroupId::LENGTH), nullptr);
    selector.setFieldEnabled(false);
    EXPECT_FALSE(selector.isEnabled());
    selector.showUnitMenu();
    EXPECT_FALSE(selector.unitMenu()->isVisible());
    selector.setFieldEnabled(true);
    EXPECT_TRUE(selector.isEnabled());
}

}  // namespace

}  // namespace QtRocket::Gui
