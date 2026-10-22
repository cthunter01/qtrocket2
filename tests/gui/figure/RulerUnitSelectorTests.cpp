// The unit selector of a ScaleScrollArea's rulers (RulerUnitSelector, OpenRocket's
// ScaleScrollPane corner UnitSelector): it sits in the corner, its choice is the rulers' unit,
// and it shows the rulers' unit when that changes elsewhere.

#include "QtRocket/gui/figure/RulerUnitSelector.h"

#include <string_view>

#include <QCoreApplication>
#include <QEvent>
#include <QFont>
#include <QLabel>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QWidget>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/figure/Ruler.h"
#include "QtRocket/gui/figure/ScaleScrollArea.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Geometry2D.h"
#include "figure/TestScaleFigure.h"

namespace QtRocket::Gui
{

namespace
{

using RulerUnitSelectorTest = Test::GuiTest;

/// A scroll area of a test figure.
struct Area
{
    Area() : figure(makeChild<Gui::Test::TestScaleFigure>(&holder, kSubject, FigureView::SIDE)) { }

    static constexpr Rectangle2D kSubject{.x = 0.0, .y = -0.05, .width = 0.5, .height = 0.1};

    QWidget                     holder;  // the figure's parent until the area takes it over
    Gui::Test::TestScaleFigure* figure;
    ScaleScrollArea             area{figure, nullptr};
};

/// The unit of @p name in the length group.
[[nodiscard]] const Unit& lengthUnit(std::string_view name)
{
    const Unit* unit = unitGroup(UnitGroupId::LENGTH).getUnit(name);
    if (unit == nullptr)
    {
        ADD_FAILURE() << "no unit " << name;
        return unitGroup(UnitGroupId::LENGTH).getDefaultUnit();
    }
    return *unit;
}

TEST_F(RulerUnitSelectorTest, SitsInTheCornerOfItsArea)
{
    Area                    shown;
    const QPointer<QWidget> label = shown.area.rulerCornerWidget();
    ASSERT_NE(qobject_cast<const QLabel*>(label.data()), nullptr);
    const RulerUnitSelector* const selector = RulerUnitSelector::install(shown.area);
    EXPECT_EQ(shown.area.rulerCornerWidget(), selector);
    EXPECT_EQ(selector->parentWidget(), &shown.area);
    EXPECT_EQ(selector->objectName(), QStringLiteral("rulerUnitSelector"));
    // The area's unit label is gone.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(label.isNull());

    // The default length unit, in the font size of the label it replaces (Java: size 8).
    EXPECT_EQ(&selector->binding()->getUnitGroup(), &unitGroup(UnitGroupId::LENGTH));
    EXPECT_EQ(&selector->binding()->getCurrentUnit(), &shown.area.rulerUnit());
    EXPECT_EQ(selector->text(), QStringLiteral("cm"));
    EXPECT_EQ(selector->font().pixelSize(), ScaleScrollArea::kCornerFontPixelSize);
}

TEST_F(RulerUnitSelectorTest, ItsChoiceIsTheRulersUnit)
{
    Area                     shown;
    const RulerUnitSelector* selector    = RulerUnitSelector::install(shown.area);
    const Unit&              millimetres = lengthUnit("mm");
    selector->binding()->setCurrentUnit(millimetres);
    EXPECT_EQ(&shown.area.rulerUnit(), &millimetres);
    EXPECT_EQ(&shown.area.horizontalRuler()->unit(), &millimetres);
    EXPECT_EQ(&shown.area.verticalRuler()->unit(), &millimetres);
    EXPECT_EQ(selector->text(), QStringLiteral("mm"));
}

TEST_F(RulerUnitSelectorTest, ShowsTheRulersUnit)
{
    Area                     shown;
    const RulerUnitSelector* selector = RulerUnitSelector::install(shown.area);
    shown.area.setRulerUnit(lengthUnit("in"));
    EXPECT_EQ(selector->binding()->getCurrentUnit().getUnit(), "in");
    EXPECT_EQ(selector->text(), QStringLiteral("in"));

    // updateRulerUnit() after the preferences: the default unit again.
    shown.area.updateRulerUnit();
    EXPECT_EQ(&selector->binding()->getCurrentUnit(),
              &unitGroup(UnitGroupId::LENGTH).getDefaultUnit());
    EXPECT_EQ(selector->text(), QStringLiteral("cm"));
}

}  // namespace

}  // namespace QtRocket::Gui
