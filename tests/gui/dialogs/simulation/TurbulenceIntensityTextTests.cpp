// The words of the turbulence intensity (OpenRocket's simedtdlg.IntensityDesc.* texts, which its
// wind models' getIntensityDescription() look up). The thresholds are the core's
// (PinkNoiseWindModel::getIntensityDescriptionKey()); the launch conditions' description beside
// the turbulence intensity is pinned against OpenRocket's in SimulationConditionsPanelTests.

#include "QtRocket/gui/dialogs/simulation/TurbulenceIntensityText.h"

#include <array>
#include <string_view>
#include <utility>

#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using TurbulenceIntensityTextTest = Test::GuiTest;

TEST_F(TurbulenceIntensityTextTest, EachKeyHasTheTextOfMessagesProperties)
{
    const auto pins = std::to_array<std::pair<std::string_view, std::string_view>>({
        {"simedtdlg.IntensityDesc.None", "None"},
        {"simedtdlg.IntensityDesc.Verylow", "Very low"},
        {"simedtdlg.IntensityDesc.Low", "Low"},
        {"simedtdlg.IntensityDesc.Medium", "Medium"},
        {"simedtdlg.IntensityDesc.High", "High"},
        {"simedtdlg.IntensityDesc.Veryhigh", "Very high"},
        {"simedtdlg.IntensityDesc.Extreme", "Extreme"},
    });
    for (const auto& [key, text] : pins)
    {
        EXPECT_EQ(TurbulenceIntensityText::text(key), QString::fromLatin1(text)) << key;
    }
}

TEST_F(TurbulenceIntensityTextTest, AnyOtherKeyIsABug)
{
    EXPECT_THROW(static_cast<void>(TurbulenceIntensityText::text("simedtdlg.IntensityDesc.Some")),
                 BugError);
}

TEST_F(TurbulenceIntensityTextTest, AModelIsDescribedByItsIntensity)
{
    PinkNoiseWindModel model(1);
    model.setAverage(2);
    model.setStandardDeviation(0.3);
    EXPECT_EQ(TurbulenceIntensityText::of(model), QStringLiteral("High"));
    model.setStandardDeviation(0);
    EXPECT_EQ(TurbulenceIntensityText::of(model), QStringLiteral("None"));
}

}  // namespace

}  // namespace QtRocket::Gui
