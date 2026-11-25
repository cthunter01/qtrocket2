#include "QtRocket/gui/util/EnumText.h"

#include <QChar>
#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "KeyTranslator.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/simulation/FlightDataType.h"

namespace QtRocket::Gui
{

namespace
{

using EnumTextTest        = Test::GuiTest;
using InstalledTranslator = Test::InstalledKeyTranslator;

TEST_F(EnumTextTest, IsTheEnglishTextWithoutATranslator)
{
    EXPECT_EQ(enumText(ComponentKind::BODY_TUBE), QStringLiteral("Body Tube"));
    EXPECT_EQ(enumText(MassComponent::MassComponentType::ALTIMETER), QStringLiteral("Altimeter"));
    EXPECT_EQ(enumText(TransitionShape::OGIVE),
              QString::fromUtf8(displayName(TransitionShape::OGIVE)));
}

TEST_F(EnumTextTest, AsksTheTranslatorsWithTheKeyAsTheDisambiguation)
{
    const InstalledTranslator translator;
    EXPECT_EQ(enumText(ComponentKind::BODY_TUBE), QStringLiteral("BodyTube.BodyTube|Body Tube"));
    EXPECT_EQ(enumText(MassComponent::MassComponentType::ALTIMETER),
              QStringLiteral("MassComponent.Altimeter|Altimeter"));
    EXPECT_EQ(translatedEnumText("Some.key", "Some text"), QStringLiteral("Some.key|Some text"));
}

TEST_F(EnumTextTest, AFlightDataTypesNameIsTranslatedWithItsKey)
{
    // Java: FlightDataType.toString(), the name, trans.get() of the key of a built-in type.
    const FlightDataType& altitude = FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE);
    const FlightDataType& cna      = FlightDataType::builtin(FlightDataTypeId::TYPE_CNA);
    EXPECT_EQ(flightDataTypeText(altitude), QStringLiteral("Altitude"));
    EXPECT_EQ(flightDataTypeText(cna), QString::fromUtf8(cna.getName()));
    const InstalledTranslator translator;
    EXPECT_EQ(flightDataTypeText(altitude),
              QStringLiteral("FlightDataType.TYPE_ALTITUDE|Altitude"));
}

TEST_F(EnumTextTest, ReadsTheTextsAsUtf8)
{
    EXPECT_EQ(translatedEnumText("Unit.degree", "\xC2\xB0"), QString(QChar(0x00B0)));
}

}  // namespace

}  // namespace QtRocket::Gui
