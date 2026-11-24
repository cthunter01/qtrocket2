// The multi-level wind's summary of the launch conditions (OpenRocket's SimulationConditionsPanel
// addMultiLevelSettings() and updateWindLevelSummary()): its texts are the SUMMARY lines of the
// probe tools/openrocket-probes/simulation-conditions (SimulationConditionsPins.h), which calls
// Java's updateWindLevelSummary() for the same levels, but for OpenRocket bug 36 (the highest
// altitude of levels below sea level); and it follows every change of the levels (bug 36: Java's
// only when the wind editor closes).

#include "QtRocket/gui/dialogs/simulation/WindLevelSummary.h"

#include <memory>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/unit/UnitGroup.h"
#include "dialogs/simulation/SimulationConditionsPins.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui
{

namespace
{

using WindLevelSummaryTest = Test::GuiTest;
using Level                = MultiLevelPinkNoiseWindModel::LevelWindModel;

/// The probe's line of @p model's summary.
[[nodiscard]] std::string summaryLine(std::string_view                    title,
                                      const MultiLevelPinkNoiseWindModel& model)
{
    return "SUMMARY " + std::string(title) + " [" +
           QtStrings::toUtf8(WindLevelSummary::summaryText(model)) + "]";
}

/// Adds the level @p altitude, @p speed, @p direction, @p deviation to @p model (a test failure
/// when the model refuses it).
void addLevel(MultiLevelPinkNoiseWindModel& model, double altitude, double speed, double direction,
              double deviation)
{
    EXPECT_TRUE(model.addWindLevel(altitude, speed, direction, deviation).has_value()) << altitude;
}

/// The probe's summaries(): the same level sets in the same order.
[[nodiscard]] std::vector<std::string> summaryLines()
{
    const InMemoryPreferences    preferences;
    MultiLevelPinkNoiseWindModel model(preferences);
    std::vector<std::string>     lines;
    lines.push_back(summaryLine("initial", model));
    model.clearLevels();
    lines.push_back(summaryLine("no levels", model));
    addLevel(model, 0, 3, std::numbers::pi / 2, 0.3);
    lines.push_back(summaryLine("one level", model));
    addLevel(model, 500, 7.5, std::numbers::pi, 1.0);
    addLevel(model, 1250.5, 12.25, 0.0, 1.0);
    lines.push_back(summaryLine("three levels", model));
    model.clearLevels();
    addLevel(model, -50, 0, 0, 0.0);
    addLevel(model, -10, 0, 0, 0.0);
    lines.push_back(summaryLine("negative altitudes, calm", model));
    model.clearLevels();
    addLevel(model, -200, 4, 0, 0.0);
    lines.push_back(summaryLine("one negative level", model));
    model.clearLevels();
    addLevel(model, 0, 2, 0, 0.0);
    addLevel(model, 1000, 10, 0, 0.0);
    const QtRocket::Test::DefaultUnitsGuard units;
    EXPECT_TRUE(unitGroup(UnitGroupId::DISTANCE).setDefaultUnit("ft"));
    EXPECT_TRUE(unitGroup(UnitGroupId::VELOCITY).setDefaultUnit("mph"));
    EXPECT_TRUE(unitGroup(UnitGroupId::WINDSPEED).setDefaultUnit("km/h"));
    lines.push_back(summaryLine("two levels in ft and mph (wind speed km/h)", model));
    return lines;
}

/// The probe's SUMMARY lines, with QtRocket's highest altitude of levels below sea level
/// (OpenRocket bug 36: Java's starts from Double.MIN_VALUE, above them, and shows 0 m).
[[nodiscard]] std::vector<std::string> pinnedSummaryLines()
{
    std::vector<std::string> lines;
    for (const std::string_view line : Test::SimulationConditionsPins::kTabAfterReset)
    {
        if (line.starts_with("SUMMARY "))
        {
            lines.emplace_back(line);
        }
    }
    const std::string java =
        "SUMMARY negative altitudes, calm [<html>2 wind level(s)<br>Altitude "
        "range: -50 m - 0 m<br>Speed range: 0 m/s - 0 m/s</html>]";
    for (std::string& line : lines)
    {
        if (line == java)
        {
            line =
                "SUMMARY negative altitudes, calm [<html>2 wind level(s)<br>Altitude range: "
                "-50 m - -10 m<br>Speed range: 0 m/s - 0 m/s</html>]";
        }
    }
    return lines;
}

TEST_F(WindLevelSummaryTest, TheSummariesAreJavasButForTheHighestAltitude)
{
    const std::vector<std::string> pinned = pinnedSummaryLines();
    ASSERT_EQ(pinned.size(), 7U);
    EXPECT_EQ(summaryLines(), pinned);
}

TEST_F(WindLevelSummaryTest, TheSummaryFollowsEveryChangeOfTheLevels)
{
    const InMemoryPreferences    preferences;
    MultiLevelPinkNoiseWindModel model(preferences);
    const WindLevelSummary       summary(model, nullptr);
    const QLabel&                label = summary.summaryLabel();
    EXPECT_EQ(label.text(), WindLevelSummary::summaryText(model));
    addLevel(model, 300, 6, 0, 0.6);
    EXPECT_EQ(label.text(), QStringLiteral("<html>2 wind level(s)<br>Altitude range: 0 m - 300 m"
                                           "<br>Speed range: 2 m/s - 6 m/s</html>"));
    Level& high = *model.getLevels().back();
    high.setAltitude(400);
    high.setSpeed(8);
    EXPECT_EQ(label.text(), QStringLiteral("<html>2 wind level(s)<br>Altitude range: 0 m - 400 m"
                                           "<br>Speed range: 2 m/s - 8 m/s</html>"));
    model.removeWindLevel(400);
    EXPECT_EQ(label.text(),
              QStringLiteral("<html>1 wind level(s)<br>Altitude: 0 m<br>Speed: 2 m/s</html>"));
    model.clearLevels();
    EXPECT_EQ(label.text(), QStringLiteral("No wind levels defined"));
    model.resetLevels(preferences);
    EXPECT_EQ(label.text(),
              QStringLiteral("<html>1 wind level(s)<br>Altitude: 0 m<br>Speed: 2 m/s</html>"));
}

TEST_F(WindLevelSummaryTest, TheSummaryKeepsItsTextOnceTheModelHasGone)
{
    const InMemoryPreferences preferences;
    auto                      model = std::make_unique<MultiLevelPinkNoiseWindModel>(preferences);
    WindLevelSummary          summary(*model, nullptr);
    const QString             text = summary.summaryLabel().text();
    EXPECT_TRUE(summary.hasModel());
    model.reset();
    EXPECT_FALSE(summary.hasModel());
    summary.refresh();
    EXPECT_EQ(summary.summaryLabel().text(), text);
}

TEST_F(WindLevelSummaryTest, EditLevelsAsksForTheWindEditor)
{
    const InMemoryPreferences    preferences;
    MultiLevelPinkNoiseWindModel model(preferences);
    const WindLevelSummary       summary(model, nullptr);
    const QSignalSpy             requests(&summary, &WindLevelSummary::editLevelsRequested);
    QPushButton&                 button = summary.editButton();
    EXPECT_EQ(button.text(), QStringLiteral("Edit levels"));
    EXPECT_FALSE(button.icon().isNull());
    EXPECT_EQ(button.objectName(), QStringLiteral("editButton"));
    EXPECT_EQ(summary.summaryLabel().objectName(), QStringLiteral("summaryLabel"));
    button.click();
    EXPECT_EQ(requests.count(), 1);
}

}  // namespace

}  // namespace QtRocket::Gui
