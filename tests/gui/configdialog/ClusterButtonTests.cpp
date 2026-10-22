#include "QtRocket/gui/configdialog/ClusterButton.h"

#include <cmath>
#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <vector>

#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QList>
#include <QMouseEvent>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSignalSpy>
#include <QSize>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtTypes>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/rocket/ClusterConfiguration.h"

namespace QtRocket::Gui
{

namespace
{

using ClusterButtonTest = Test::GuiTest;

/// A layout of D2aProbe's output: its xml name and the centres of its motors' circles
/// (InnerTubeConfig.java's px = BUTTON_SIZE / 2 + x * MOTOR_DIAMETER and py = BUTTON_SIZE / 2 -
/// y * MOTOR_DIAMETER, printed by Double.toString()).
struct ProbeLayout
{
    const char*          name;
    std::vector<QPointF> centres;
};

/// The 14 layouts of D2aProbe, in ClusterConfiguration.CONFIGURATIONS' order.
[[nodiscard]] std::vector<ProbeLayout> probeLayouts()
{
    return {
        {.name = "single", .centres = {{25.0, 25.0}}},
        {.name = "double", .centres = {{20.0, 25.0}, {30.0, 25.0}}},
        {.name = "3-row", .centres = {{15.0, 25.0}, {25.0, 25.0}, {35.0, 25.0}}},
        {.name = "4-row", .centres = {{10.0, 25.0}, {20.0, 25.0}, {30.0, 25.0}, {40.0, 25.0}}},
        {.name    = "3-ring",
         .centres = {{20.0, 27.886751345948127},
                     {30.0, 27.886751345948127},
                     {25.0, 19.226497308103742}}},
        {.name = "4-ring", .centres = {{20.0, 20.0}, {30.0, 20.0}, {30.0, 30.0}, {20.0, 30.0}}},
        {.name    = "5-ring",
         .centres = {{25.0, 16.493491916479602},
                     {33.09016994374947, 22.371344439404332},
                     {30.0, 31.88190960235587},
                     {20.0, 31.88190960235587},
                     {16.909830056250527, 22.371344439404332}}},
        {.name    = "6-ring",
         .centres = {{25.0, 15.0},
                     {33.66025403784438, 20.0},
                     {33.66025403784438, 30.0},
                     {25.0, 35.0},
                     {16.339745962155614, 30.0},
                     {16.339745962155614, 20.0}}},
        {.name = "3-star",
         .centres =
             {{25.0, 25.0}, {25.0, 15.0}, {33.66025403784438, 30.0}, {16.339745962155614, 30.0}}},
        {.name    = "4-star",
         .centres = {{25.0, 25.0},
                     {17.928932188134524, 17.928932188134524},
                     {32.071067811865476, 17.928932188134524},
                     {32.071067811865476, 32.071067811865476},
                     {17.928932188134524, 32.071067811865476}}},
        {.name    = "5-star",
         .centres = {{25.0, 25.0},
                     {25.0, 15.0},
                     {34.510565162951536, 21.909830056250527},
                     {30.877852522924734, 33.09016994374947},
                     {19.12214747707527, 33.09016994374947},
                     {15.489434837048464, 21.909830056250527}}},
        {.name    = "6-star",
         .centres = {{25.0, 25.0},
                     {25.0, 15.0},
                     {33.66025403784438, 20.0},
                     {33.66025403784438, 30.0},
                     {25.0, 35.0},
                     {16.339745962155614, 30.0},
                     {16.339745962155614, 20.0}}},
        {.name    = "9-grid",
         .centres = {{11.0, 11.0},
                     {25.0, 11.0},
                     {39.0, 11.0},
                     {11.0, 25.0},
                     {25.0, 25.0},
                     {39.0, 25.0},
                     {11.0, 39.0},
                     {25.0, 39.0},
                     {39.0, 39.0}}},
        {.name    = "9-star",
         .centres = {{25.0, 25.0},
                     {39.0, 25.0},
                     {34.89949493661166, 34.89949493661166},
                     {25.0, 39.0},
                     {15.100505063388336, 34.89949493661166},
                     {11.0, 25.0},
                     {15.100505063388336, 15.100505063388336},
                     {25.0, 11.0},
                     {34.89949493661166, 15.100505063388336}}},
    };
}

/// Whether @p a and @p b agree to a few units in the last place (the probe's values are sums
/// and products of the layouts' doubles, which QRectF stores as a corner and a size).
[[nodiscard]] bool isClose(double a, double b)
{
    return std::abs(a - b) <= 1e-12;
}

/// The differences between ClusterButton::motorCircles() and the probe, one line each.
[[nodiscard]] std::string circleDifferences()
{
    const std::span<const ClusterConfiguration> configurations =
        ClusterConfiguration::configurations();
    const std::vector<ProbeLayout> probe = probeLayouts();
    std::string                    differences;
    if (configurations.size() != probe.size())
    {
        return std::format("{} layouts, the probe has {}\n", configurations.size(), probe.size());
    }
    for (std::size_t i = 0; i < probe.size(); ++i)
    {
        const ProbeLayout&  expected = probe[i];
        const QList<QRectF> circles  = ClusterButton::motorCircles(configurations[i]);
        if (configurations[i].getXmlName() != expected.name)
        {
            differences +=
                std::format("{}: named {}\n", expected.name, configurations[i].getXmlName());
        }
        if (circles.size() != static_cast<qsizetype>(expected.centres.size()))
        {
            differences += std::format("{}: {} circles\n", expected.name, circles.size());
            continue;
        }
        for (std::size_t j = 0; j < expected.centres.size(); ++j)
        {
            const QRectF&  circle = circles.at(static_cast<qsizetype>(j));
            const QPointF& centre = expected.centres[j];
            if (!isClose(circle.center().x(), centre.x()) ||
                !isClose(circle.center().y(), centre.y()) || !isClose(circle.width(), 10.0) ||
                !isClose(circle.height(), 10.0))
            {
                differences +=
                    std::format("{} {}: ({}, {}) {} x {}\n", expected.name, j, circle.center().x(),
                                circle.center().y(), circle.width(), circle.height());
            }
        }
    }
    return differences;
}

TEST_F(ClusterButtonTest, TheCirclesOfTheMotorsAreJavas)
{
    EXPECT_EQ(circleDifferences(), "");
}

TEST_F(ClusterButtonTest, AButtonIsFiftyPixelsWithTheLayoutsName)
{
    QWidget       window;
    ClusterButton button(ClusterConfiguration::configurations()[4], &window);
    EXPECT_EQ(button.size(), QSize(ClusterButton::kButtonSize, ClusterButton::kButtonSize));
    EXPECT_EQ(button.minimumSize(), button.maximumSize());
    EXPECT_EQ(button.toolTip(), QStringLiteral("3-ring"));
    EXPECT_EQ(button.objectName(), QStringLiteral("3-ring"));
    EXPECT_EQ(&button.configuration(), &ClusterConfiguration::configurations()[4]);
    EXPECT_EQ(button.focusPolicy(), Qt::NoFocus);
    EXPECT_EQ(button.frameShadow(), QFrame::Sunken);
    EXPECT_FALSE(button.isSelected());
    button.setSelected(true);
    EXPECT_TRUE(button.isSelected());
    button.setSelected(false);
    EXPECT_FALSE(button.isSelected());
}

TEST_F(ClusterButtonTest, ALeftClickInsideEmitsClicked)
{
    QWidget       window;
    ClusterButton button(ClusterConfiguration::single(), &window);
    window.show();
    const QSignalSpy clicks(&button, &ClusterButton::clicked);
    QTest::mouseClick(&button, Qt::LeftButton, Qt::NoModifier, QPoint(25, 25));
    EXPECT_EQ(clicks.count(), 1);

    // Released outside: no click.
    QTest::mousePress(&button, Qt::LeftButton, Qt::NoModifier, QPoint(25, 25));
    QTest::mouseRelease(&button, Qt::LeftButton, Qt::NoModifier, QPoint(80, 25));
    EXPECT_EQ(clicks.count(), 1);

    // The right button: no click (Java: BUTTON1 only).
    const QPointF inside(25, 25);
    QMouseEvent press(QEvent::MouseButtonPress, inside, button.mapToGlobal(inside), Qt::RightButton,
                      Qt::RightButton, Qt::NoModifier);
    QApplication::sendEvent(&button, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, inside, button.mapToGlobal(inside),
                        Qt::RightButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&button, &release);
    EXPECT_EQ(clicks.count(), 1);
}

}  // namespace

}  // namespace QtRocket::Gui
