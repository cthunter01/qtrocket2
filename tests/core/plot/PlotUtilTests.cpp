#include "QtRocket/plot/PlotUtil.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"

// The labels are pinned to what OpenRocket's Util.generateSeriesLabels() gives (the "LABELS"
// lines of tools/openrocket-probes/plot-model's expected.txt).

namespace
{

using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::PlotUtil::formatHtmlString;
using QtRocket::PlotUtil::generateSeriesLabels;
using Labels = std::vector<std::string>;

TEST(PlotUtil, FormatHtmlStringRemovesTheTags)
{
    EXPECT_EQ(formatHtmlString("<html>Motor burnout<br>without liftoff</html>"),
              "Motor burnoutwithout liftoff");
    EXPECT_EQ(formatHtmlString("Booster<i> stage</i>"), "Booster stage");
    EXPECT_EQ(formatHtmlString("a<b"), "a<b");
    EXPECT_EQ(formatHtmlString(""), "");
}

TEST(PlotUtil, UniqueNamesStay)
{
    EXPECT_EQ(generateSeriesLabels(Labels{}), Labels{});
    EXPECT_EQ(generateSeriesLabels(Labels{"Sustainer"}), Labels{"Sustainer"});
    EXPECT_EQ(generateSeriesLabels(Labels{"Sustainer", "Booster"}),
              (Labels{"Sustainer", "Booster"}));
}

TEST(PlotUtil, DuplicateNamesAreNumberedInOrder)
{
    EXPECT_EQ(generateSeriesLabels(Labels{"Sustainer", "Booster stage", "Booster stage"}),
              (Labels{"Sustainer", "Booster stage(1)", "Booster stage(2)"}));
    EXPECT_EQ(generateSeriesLabels(Labels{"A", "B", "A"}), (Labels{"A(1)", "B", "A(2)"}));
    EXPECT_EQ(generateSeriesLabels(Labels{"A", "A", "A"}), (Labels{"A(1)", "A(2)", "A(3)"}));
    EXPECT_EQ(generateSeriesLabels(Labels{"A", "A", "B", "B", "A"}),
              (Labels{"A(1)", "A(2)", "B(1)", "B(2)", "A(3)"}));
    EXPECT_EQ(generateSeriesLabels(Labels{"", ""}), (Labels{"(1)", "(2)"}));
}

TEST(PlotUtil, NamesAreComparedWithoutTheirTags)
{
    EXPECT_EQ(generateSeriesLabels(Labels{"<b>Stage</b>", "Stage"}),
              (Labels{"Stage(1)", "Stage(2)"}));
    EXPECT_EQ(generateSeriesLabels(Labels{"a<b", "a<b"}), (Labels{"a<b(1)", "a<b(2)"}));
    EXPECT_EQ(generateSeriesLabels(Labels{"Sustainer", "Booster<i> stage</i>", "Booster stage"}),
              (Labels{"Sustainer", "Booster stage(1)", "Booster stage(2)"}));
}

TEST(PlotUtil, ANumberedLabelCountsAgainstALaterName)
{
    // As in Java: "A(1)" made for the second branch equals the first branch's name, which keeps
    // it.
    EXPECT_EQ(generateSeriesLabels(Labels{"A(1)", "A", "A"}), (Labels{"A(1)", "A(1)", "A(2)"}));
}

TEST(PlotUtil, ADuplicateBeforeANameDoesNotRunPastTheLastBranch)
{
    // Java throws IndexOutOfBoundsException for both ("Index 3 out of bounds for length 3"); the
    // numbering stops at the last branch (see the header).
    EXPECT_EQ(generateSeriesLabels(Labels{"A", "A(1)", "A"}), (Labels{"A(1)", "A(1)(1)", "A(2)"}));
    EXPECT_EQ(generateSeriesLabels(Labels{"A", "A", "A(1)"}), (Labels{"A(1)", "A(2)", "A(1)(1)"}));
}

/// A branch named @p name with a time column and no rows.
[[nodiscard]] std::shared_ptr<FlightDataBranch> namedBranch(std::string name)
{
    const std::vector<const FlightDataType*> types{
        &FlightDataType::builtin(QtRocket::FlightDataTypeId::TYPE_TIME)};
    return std::make_shared<FlightDataBranch>(std::move(name), types);
}

TEST(PlotUtil, TheLabelsOfFlightDataAreThoseOfItsBranchNames)
{
    const FlightData data{namedBranch("Sustainer"), namedBranch("Booster stage"),
                          namedBranch("Booster stage")};
    EXPECT_EQ(generateSeriesLabels(data),
              (Labels{"Sustainer", "Booster stage(1)", "Booster stage(2)"}));
    EXPECT_EQ(generateSeriesLabels(FlightData{}), Labels{});
}

}  // namespace
