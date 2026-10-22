#include "QtRocket/gui/dialogs/DefaultUnitSelector.h"

#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "dialogs/PreferencesPageTranscript.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using DefaultUnitSelectorTest = Gui::Test::GuiTest;

/// The texts of the items of @p selector.
[[nodiscard]] QStringList itemTexts(const DefaultUnitSelector& selector)
{
    QStringList texts;
    for (int index = 0; index < selector.count(); ++index)
    {
        texts.append(selector.itemText(index));
    }
    return texts;
}

/// The index of the unit named @p name in @p group.
[[nodiscard]] int indexOf(UnitGroupId group, const char* name)
{
    const Unit* const unit = unitGroup(group).getUnit(name);
    return unit == nullptr ? -1 : unitGroup(group).getUnitIndex(*unit);
}

TEST_F(DefaultUnitSelectorTest, ShowsTheGroupsUnitsWithItsDefaultCurrent)
{
    const DefaultUnitSelector selector(UnitGroupId::LENGTH, nullptr);
    EXPECT_EQ(selector.objectName(), QStringLiteral("LENGTH"));
    EXPECT_EQ(selector.group(), UnitGroupId::LENGTH);
    // OpenRocket's Rocket dimensions (PreferencesProbe).
    EXPECT_EQ(itemTexts(selector),
              QStringList({QStringLiteral("mm"), QStringLiteral("cm"), QStringLiteral("m"),
                           QStringLiteral("in"), QStringLiteral("in/64"), QStringLiteral("ft")}));
    EXPECT_EQ(selector.currentText(), QStringLiteral("cm"));
}

TEST_F(DefaultUnitSelectorTest, AChosenUnitIsTheDefaultAtOnce)
{
    DefaultUnitSelector selector(UnitGroupId::TEMPERATURE, nullptr);
    Gui::Test::chooseItem(selector, indexOf(UnitGroupId::TEMPERATURE, "\u00B0F"));
    EXPECT_EQ(QtStrings::unitSymbol(unitGroup(UnitGroupId::TEMPERATURE).getDefaultUnit()),
              QStringLiteral("\u00B0F"));
    EXPECT_EQ(selector.currentText(), QStringLiteral("\u00B0F"));
    UnitGroup::resetDefaultUnits();
}

TEST_F(DefaultUnitSelectorTest, RefreshShowsADefaultChangedElsewhere)
{
    DefaultUnitSelector selector(UnitGroupId::MASS, nullptr);
    UnitGroup::setDefaultImperialUnits();
    EXPECT_EQ(selector.currentText(), QStringLiteral("g"));
    selector.refresh();
    EXPECT_EQ(selector.currentText(), QStringLiteral("oz"));
    UnitGroup::resetDefaultUnits();
}

TEST_F(DefaultUnitSelectorTest, TheWheelChoosesNothingWithoutTheFocus)
{
    DefaultUnitSelector selector(UnitGroupId::LENGTH, nullptr);
    EXPECT_FALSE(Gui::Test::turnWheel(selector, -120));
    EXPECT_EQ(selector.currentText(), QStringLiteral("cm"));
    EXPECT_EQ(unitGroup(UnitGroupId::LENGTH).getDefaultUnit().getUnit(), "cm");
}

}  // namespace

}  // namespace QtRocket::Gui
