#include "QtRocket/gui/util/ComponentIcons.h"

#include <QIcon>
#include <QImage>
#include <QList>
#include <QRgb>
#include <QSize>
#include <QString>
#include <QStringList>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/Rocket.h"

namespace QtRocket::Gui
{

namespace
{

using ComponentIconsTest = Test::GuiTest;
using MassType           = MassComponent::MassComponentType;
using Theme              = ThemeColors::Theme;

/// The image of @p icon at its natural size (the PNG's).
QImage naturalImage(const QIcon& icon)
{
    const QList<QSize> sizes = icon.availableSizes();
    return icon.pixmap(sizes.isEmpty() ? QSize(50, 40) : sizes.front()).toImage();
}

// Port of ComponentIconsTest.componentInstanceUsesItsClassIcon (swing
// gui/main/ComponentIconsTest.java). Java's assertSame is the same cacheKey() here: the same icon.
TEST_F(ComponentIconsTest, ComponentInstanceUsesItsClassIcon)
{
    const BodyTube bodyTube;

    EXPECT_FALSE(ComponentIcons::smallIcon(bodyTube).isNull());
    EXPECT_EQ(ComponentIcons::smallIcon(ComponentKind::BODY_TUBE).cacheKey(),
              ComponentIcons::smallIcon(bodyTube).cacheKey());
}

// Port of ComponentIconsTest.massComponentInstanceUsesItsConfiguredSubtypeIcon.
TEST_F(ComponentIconsTest, MassComponentInstanceUsesItsConfiguredSubtypeIcon)
{
    MassComponent massComponent;
    massComponent.setMassComponentType(MassType::ALTIMETER);

    EXPECT_FALSE(ComponentIcons::smallIcon(massComponent).isNull());
    EXPECT_EQ(ComponentIcons::smallMassTypeIcon(MassType::ALTIMETER).cacheKey(),
              ComponentIcons::smallIcon(massComponent).cacheKey());
}

/// The icons of @p theme that are missing, by name: every kind but the rocket has a small, a
/// large and a disabled large icon, every mass component type a small one.
QStringList missingIcons(Theme theme)
{
    QStringList missing;
    for (const ComponentKind kind : kAllComponentKinds)
    {
        if (kind == ComponentKind::ROCKET)
        {
            continue;
        }
        const QString name = ComponentIcons::iconName(kind, theme);
        if (ComponentIcons::smallIcon(kind, theme).isNull())
        {
            missing.append(name + QStringLiteral("-small"));
        }
        if (ComponentIcons::largeIcon(kind, theme).isNull())
        {
            missing.append(name + QStringLiteral("-large"));
        }
        if (ComponentIcons::largeDisabledIcon(kind, theme).isNull())
        {
            missing.append(name + QStringLiteral("-large (disabled)"));
        }
    }
    for (const MassType type : MassComponent::kAllMassComponentTypes)
    {
        if (ComponentIcons::smallMassTypeIcon(type, theme).isNull())
        {
            missing.append(ComponentIcons::massTypeIconName(type, theme) +
                           QStringLiteral("-small"));
        }
    }
    return missing;
}

TEST_F(ComponentIconsTest, EveryKindButTheRocketHasItsThreeIconsInBothThemes)
{
    EXPECT_EQ(missingIcons(Theme::LIGHT), QStringList());
    EXPECT_EQ(missingIcons(Theme::DARK), QStringList());
}

TEST_F(ComponentIconsTest, TheRocketHasNoIcon)
{
    const Rocket rocket;
    EXPECT_TRUE(ComponentIcons::smallIcon(ComponentKind::ROCKET).isNull());
    EXPECT_TRUE(ComponentIcons::smallIcon(rocket).isNull());
    EXPECT_TRUE(ComponentIcons::largeIcon(ComponentKind::ROCKET).isNull());
    EXPECT_TRUE(ComponentIcons::largeDisabledIcon(ComponentKind::ROCKET).isNull());
}

TEST_F(ComponentIconsTest, ADarkPaletteUsesTheDarkVariantsOfOpenRocketsDarkTheme)
{
    EXPECT_EQ(ComponentIcons::iconName(ComponentKind::PARACHUTE, Theme::LIGHT),
              QStringLiteral("parachute"));
    EXPECT_EQ(ComponentIcons::iconName(ComponentKind::PARACHUTE, Theme::DARK),
              QStringLiteral("parachute_dark"));
    // A kind without a dark variant keeps its icon.
    EXPECT_EQ(ComponentIcons::iconName(ComponentKind::BODY_TUBE, Theme::DARK),
              QStringLiteral("bodytube"));
    // OpenRocket's dark theme has no dark payload icon.
    EXPECT_EQ(ComponentIcons::massTypeIconName(MassType::PAYLOAD, Theme::DARK),
              QStringLiteral("payload"));
    EXPECT_EQ(ComponentIcons::massTypeIconName(MassType::DEPLOYMENTCHARGE, Theme::DARK),
              QStringLiteral("deployment-charge_dark"));
    EXPECT_NE(ComponentIcons::smallIcon(ComponentKind::PARACHUTE, Theme::LIGHT).cacheKey(),
              ComponentIcons::smallIcon(ComponentKind::PARACHUTE, Theme::DARK).cacheKey());
}

TEST_F(ComponentIconsTest, MapsEachKindToOpenRocketsFile)
{
    EXPECT_EQ(ComponentIcons::iconName(ComponentKind::AXIAL_STAGE, Theme::LIGHT),
              QStringLiteral("stage"));
    EXPECT_EQ(ComponentIcons::iconName(ComponentKind::PARALLEL_STAGE, Theme::LIGHT),
              QStringLiteral("boosters"));
    EXPECT_EQ(ComponentIcons::iconName(ComponentKind::POD_SET, Theme::LIGHT),
              QStringLiteral("pods"));
    EXPECT_EQ(ComponentIcons::iconName(ComponentKind::TRAPEZOID_FIN_SET, Theme::LIGHT),
              QStringLiteral("trapezoidfin"));
    EXPECT_EQ(ComponentIcons::iconName(ComponentKind::MASS_COMPONENT, Theme::LIGHT),
              QStringLiteral("mass"));
    EXPECT_EQ(ComponentIcons::massTypeIconName(MassType::FLIGHTCOMPUTER, Theme::LIGHT),
              QStringLiteral("flight-comp"));
    EXPECT_EQ(ComponentIcons::resourcePath(QStringLiteral("bodytube"), false),
              QStringLiteral(":/icons/components/bodytube-small.png"));
    EXPECT_EQ(ComponentIcons::resourcePath(QStringLiteral("bodytube"), true),
              QStringLiteral(":/icons/components/bodytube-large.png"));
}

/// The pixels of @p disabled that differ from what OpenRocket makes of @p original: transparent
/// black where x + y is even, the original's pixel elsewhere.
int wronglyDisabledPixels(const QImage& original, const QImage& disabled)
{
    int wrong = 0;
    for (int y = 0; y < original.height(); ++y)
    {
        for (int x = 0; x < original.width(); ++x)
        {
            const QRgb expected = (x + y) % 2 == 0 ? 0U : original.pixel(x, y);
            if (disabled.pixel(x, y) != expected)
            {
                ++wrong;
            }
        }
    }
    return wrong;
}

TEST_F(ComponentIconsTest, TheDisabledIconClearsEveryPixelWhoseCoordinatesSumToAnEvenNumber)
{
    const QImage original = QImage(ComponentIcons::resourcePath(QStringLiteral("nosecone"), true))
                                .convertToFormat(QImage::Format_ARGB32);
    const QImage disabled =
        naturalImage(ComponentIcons::largeDisabledIcon(ComponentKind::NOSE_CONE, Theme::LIGHT))
            .convertToFormat(QImage::Format_ARGB32);
    ASSERT_EQ(disabled.size(), original.size());
    EXPECT_EQ(wronglyDisabledPixels(original, disabled), 0);
    // Not every pixel is cleared.
    QImage transparent(disabled.size(), QImage::Format_ARGB32);
    transparent.fill(Qt::transparent);
    EXPECT_NE(disabled, transparent);
}

TEST_F(ComponentIconsTest, TheLargeIconIsThePng)
{
    const QImage original = QImage(ComponentIcons::resourcePath(QStringLiteral("bodytube"), true))
                                .convertToFormat(QImage::Format_ARGB32);
    const QImage large =
        naturalImage(ComponentIcons::largeIcon(ComponentKind::BODY_TUBE, Theme::LIGHT))
            .convertToFormat(QImage::Format_ARGB32);
    EXPECT_EQ(large, original);
    EXPECT_EQ(original.size(), QSize(50, 40));
}

}  // namespace

}  // namespace QtRocket::Gui
