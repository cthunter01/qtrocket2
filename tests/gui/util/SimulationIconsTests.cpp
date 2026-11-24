#include "QtRocket/gui/util/SimulationIcons.h"

#include <QIcon>
#include <QImage>
#include <QList>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "ScopedApplicationPalette.h"

namespace QtRocket::Gui
{

namespace
{

using SimulationIconsTest = Test::GuiTest;
using Status              = Simulation::Status;
using Type                = FlightEvent::Type;
using Palette             = Test::ScopedApplicationPalette;

/// OpenRocket's PNGs are 16 pixels square.
constexpr QSize kPngSize(16, 16);

/// @p image in one format for comparing pixels.
[[nodiscard]] QImage comparable(const QImage& image)
{
    return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

/// @p icon drawn at the size of OpenRocket's PNGs.
[[nodiscard]] QImage drawn(const QIcon& icon)
{
    return comparable(icon.pixmap(kPngSize).toImage());
}

/// The PNG of the resource @p path as it is.
[[nodiscard]] QImage png(const QString& path)
{
    return comparable(QImage(path));
}

/// "<path>" for each of @p paths whose icon (made by @p iconOf) is null, has another size than
/// the PNG's or draws anything but the PNG's own pixels.
template <class Value, class IconOf, class PathOf>
[[nodiscard]] QStringList iconsNotDrawnAsTheirPng(const QList<Value>& values, IconOf iconOf,
                                                  PathOf pathOf)
{
    QStringList wrong;
    for (const Value& value : values)
    {
        const QIcon   icon = iconOf(value);
        const QString path = pathOf(value);
        if (icon.isNull() || icon.availableSizes() != QList<QSize>{kPngSize} ||
            drawn(icon) != png(path))
        {
            wrong.append(path);
        }
    }
    return wrong;
}

[[nodiscard]] QList<Status> allStatuses()
{
    return {Simulation::kAllStatuses.begin(), Simulation::kAllStatuses.end()};
}

[[nodiscard]] QList<Type> typesWithAnImage()
{
    QList<Type> types;
    for (const Type type : FlightEvent::kAllTypes)
    {
        if (type != Type::ALTITUDE && type != Type::SIM_WARN)
        {
            types.append(type);
        }
    }
    return types;
}

[[nodiscard]] QList<MessagePriority> allPriorities()
{
    return {kAllPriorities.begin(), kAllPriorities.end()};
}

/// A SIM_WARN event at 1 s whose warning has @p priority.
[[nodiscard]] FlightEvent warningEvent(MessagePriority priority)
{
    return {Type::SIM_WARN, 1.0, nullptr,
            FlightEvent::warningData(Warning::fromString("A warning", priority))};
}

TEST_F(SimulationIconsTest, TheStatusIconsAreIconsJavasStatusMap)
{
    // Icons.SIMULATION_STATUS_ICON_MAP: SIM_UPTODATE, SIM_OUTDATED, SIM_CANTRUN, SIM_ABORTED.
    const QString tick    = QStringLiteral(":/icons/status/tick.png");
    const QString refresh = QStringLiteral(":/icons/status/refresh.png");
    EXPECT_EQ(SimulationIcons::statusResourcePath(Status::UPTODATE), tick);
    EXPECT_EQ(SimulationIcons::statusResourcePath(Status::LOADED), tick);
    EXPECT_EQ(SimulationIcons::statusResourcePath(Status::EXTERNAL), tick);
    EXPECT_EQ(SimulationIcons::statusResourcePath(Status::NOT_SIMULATED), refresh);
    EXPECT_EQ(SimulationIcons::statusResourcePath(Status::OUTDATED), refresh);
    EXPECT_EQ(SimulationIcons::statusResourcePath(Status::CANT_RUN),
              QStringLiteral(":/icons/status/sim_cantrun.png"));
    EXPECT_EQ(SimulationIcons::statusResourcePath(Status::ABORTED),
              QStringLiteral(":/icons/events/event-exception.png"));
}

TEST_F(SimulationIconsTest, TheStatusIconsAreDrawnAsTheirPngsInBothThemes)
{
    const auto icon = [](Status status) { return SimulationIcons::statusIcon(status); };
    const auto path = [](Status status) { return SimulationIcons::statusResourcePath(status); };
    {
        const Palette light(Palette::light());
        EXPECT_EQ(iconsNotDrawnAsTheirPng(allStatuses(), icon, path), QStringList());
    }
    const Palette dark(Palette::dark());
    EXPECT_EQ(iconsNotDrawnAsTheirPng(allStatuses(), icon, path), QStringList());
}

TEST_F(SimulationIconsTest, TheEventImagesAreEventGraphicsFiles)
{
    EXPECT_EQ(SimulationIcons::eventResourcePath(Type::LAUNCH),
              QStringLiteral(":/icons/events/event-launch.png"));
    EXPECT_EQ(SimulationIcons::eventResourcePath(Type::RECOVERY_DEVICE_DEPLOYMENT),
              QStringLiteral(":/icons/events/event-recovery-device-deployment.png"));
    EXPECT_EQ(SimulationIcons::eventResourcePath(Type::SIMULATION_END),
              QStringLiteral(":/icons/events/event-simulation-end.png"));
    // SIM_ABORT has EXCEPTION's image; ALTITUDE has none, SIM_WARN its warning's.
    EXPECT_EQ(SimulationIcons::eventResourcePath(Type::SIM_ABORT),
              QStringLiteral(":/icons/events/event-exception.png"));
    EXPECT_EQ(SimulationIcons::eventResourcePath(Type::EXCEPTION),
              QStringLiteral(":/icons/events/event-exception.png"));
    EXPECT_EQ(SimulationIcons::eventResourcePath(Type::ALTITUDE), QString());
    EXPECT_EQ(SimulationIcons::eventResourcePath(Type::SIM_WARN), QString());
    EXPECT_TRUE(SimulationIcons::eventIcon(Type::ALTITUDE).isNull());
    EXPECT_TRUE(SimulationIcons::eventIcon(Type::SIM_WARN).isNull());
}

TEST_F(SimulationIconsTest, EveryEventImageIsDrawnAsItsPng)
{
    const auto icon = [](Type type) { return SimulationIcons::eventIcon(type); };
    const auto path = [](Type type) { return SimulationIcons::eventResourcePath(type); };
    EXPECT_EQ(iconsNotDrawnAsTheirPng(typesWithAnImage(), icon, path), QStringList());
    const auto warning = [](MessagePriority priority) {
        return SimulationIcons::warningIcon(priority);
    };
    const auto warningPath = [](MessagePriority priority) {
        return SimulationIcons::warningResourcePath(priority);
    };
    EXPECT_EQ(iconsNotDrawnAsTheirPng(allPriorities(), warning, warningPath), QStringList());
}

TEST_F(SimulationIconsTest, ASimWarnEventsImageIsItsWarningsPriority)
{
    // EventGraphics' MESSAGE_IMAGES: pix/icons/warning_low|normal|high.png.
    EXPECT_EQ(SimulationIcons::warningResourcePath(MessagePriority::LOW),
              QStringLiteral(":/icons/status/warning_low.png"));
    EXPECT_EQ(SimulationIcons::warningResourcePath(MessagePriority::NORMAL),
              QStringLiteral(":/icons/status/warning_normal.png"));
    EXPECT_EQ(SimulationIcons::warningResourcePath(MessagePriority::HIGH),
              QStringLiteral(":/icons/status/warning_high.png"));
    EXPECT_EQ(drawn(SimulationIcons::eventIcon(warningEvent(MessagePriority::LOW))),
              png(SimulationIcons::warningResourcePath(MessagePriority::LOW)));
    EXPECT_EQ(drawn(SimulationIcons::eventIcon(warningEvent(MessagePriority::HIGH))),
              png(SimulationIcons::warningResourcePath(MessagePriority::HIGH)));
    // Any other event: the image of its type.
    EXPECT_EQ(drawn(SimulationIcons::eventIcon(FlightEvent(Type::APOGEE, 2.0))),
              png(SimulationIcons::eventResourcePath(Type::APOGEE)));
    EXPECT_TRUE(SimulationIcons::eventIcon(
                    FlightEvent(Type::ALTITUDE, 2.0, nullptr,
                                FlightEvent::AltitudeChange{.previous = 1.0, .current = 2.0}))
                    .isNull());
}

TEST_F(SimulationIconsTest, NamesNineteenPngs)
{
    // 3 status icons, 13 event images (event-exception.png also the ABORTED status's) and the 3
    // warnings' images.
    const QStringList paths = SimulationIcons::resourcePaths();
    EXPECT_EQ(paths.size(), 19);
    EXPECT_EQ(paths.filter(QStringLiteral(":/icons/events/")).size(), 13);
    EXPECT_EQ(paths.filter(QStringLiteral(":/icons/status/")).size(), 6);
}

}  // namespace

}  // namespace QtRocket::Gui
