#include "QtRocket/gui/util/QtStrings.h"

#include <filesystem>
#include <string>

#include <QChar>
#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/FileIo.h"

namespace QtRocket::Gui
{

namespace
{

using QtStringsTest = Test::GuiTest;

// "Finé ° μ \U0001F680" (e acute, degree, mu, a rocket) in UTF-8.
constexpr const char* kUtf8 = "Fin\xC3\xA9 \xC2\xB0 \xCE\xBC \xF0\x9F\x9A\x80";

TEST_F(QtStringsTest, ConvertsUtf8BothWays)
{
    const QString text = QtStrings::fromUtf8(kUtf8);
    EXPECT_EQ(text, QStringLiteral(u"Fin\u00E9 \u00B0 \u03BC \U0001F680"));
    EXPECT_EQ(QtStrings::toUtf8(text), std::string(kUtf8));
    EXPECT_EQ(QtStrings::toUtf8(QString()), std::string());
}

TEST_F(QtStringsTest, ReadsBrokenUtf8AsReplacementCharacters)
{
    EXPECT_EQ(QtStrings::fromUtf8("a\xFF"
                                  "b"),
              QStringLiteral(u"a\uFFFDb"));
}

TEST_F(QtStringsTest, ConvertsPathsThroughTheCoresUtf8Paths)
{
    const std::filesystem::path path = pathFromUtf8(std::string("designs/") + kUtf8 + ".ork");
    const QString               text = QtStrings::fromPath(path);
    EXPECT_EQ(QtStrings::toUtf8(text), pathToUtf8(path));
    EXPECT_EQ(QtStrings::toPath(text), path);
    // A dialog's '/' is a separator on every platform.
    EXPECT_EQ(QtStrings::toPath(QStringLiteral("designs/rocket.ork")).filename(),
              std::filesystem::path("rocket.ork"));
}

TEST_F(QtStringsTest, WritesValuesInTheirUnits)
{
    const UnitGroup& length = unitGroup(UnitGroupId::LENGTH);
    const Unit&      unit   = length.getDefaultUnit();
    EXPECT_EQ(QtStrings::unitText(unit, 0.05), QtStrings::fromUtf8(unit.toString(0.05)));
    EXPECT_EQ(QtStrings::unitTextWithUnit(unit, 0.05),
              QtStrings::fromUtf8(unit.toStringUnit(0.05)));
    EXPECT_EQ(QtStrings::defaultUnitText(UnitGroupId::LENGTH, 0.05),
              QtStrings::fromUtf8(length.toStringUnit(0.05)));
    EXPECT_EQ(QtStrings::defaultUnitText(UnitGroupId::LENGTH, 0.05), QStringLiteral("5 cm"));
    // The angle's default unit is the degree sign, U+00B0.
    EXPECT_EQ(QtStrings::unitSymbol(unitGroup(UnitGroupId::ANGLE).getDefaultUnit()),
              QString(QChar(0x00B0)));
}

TEST_F(QtStringsTest, WritesDoublesAsJava)
{
    EXPECT_EQ(QtStrings::javaDoubleText(0.5), QStringLiteral("0.5"));
    EXPECT_EQ(QtStrings::javaDoubleText(1.0), QStringLiteral("1.0"));
    EXPECT_EQ(QtStrings::javaDoubleText(1.0e-4), QStringLiteral("1.0E-4"));
}

}  // namespace

}  // namespace QtRocket::Gui
