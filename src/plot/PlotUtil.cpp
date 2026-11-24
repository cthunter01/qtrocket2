#include "QtRocket/plot/PlotUtil.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket::PlotUtil
{

std::string formatHtmlString(std::string_view input)
{
    // Remove the HTML-like tags from the final string
    return Strings::removeHtmlTags(input);
}

std::vector<std::string> generateSeriesLabels(std::span<const std::string> names)
{
    std::vector<std::string> series;
    series.reserve(names.size());
    // We need to generate unique strings for each of the branches.  Since the branch names are
    // based on the stage name there is no guarantee they are unique.  In order to address this,
    // we first assume all the names are unique, then go through them looking for duplicates.
    for (const std::string& name : names)
    {
        series.push_back(formatHtmlString(name));
    }
    // check for duplicates:
    for (std::size_t i = 0; i < series.size(); ++i)
    {
        const std::string stagename        = series[i];
        const auto        numberDuplicates = std::ranges::count(series, stagename);
        if (numberDuplicates > 1)
        {
            std::size_t    index = i;
            std::ptrdiff_t count = 1;
            // Java's loop runs past the last branch when a duplicate comes before i (see the
            // header): stop there.
            while (count <= numberDuplicates && index < series.size())
            {
                series[index] = stagename + "(" + std::to_string(count) + ")";
                count++;
                ++index;
                while (index < series.size() && stagename != series[index])
                {
                    ++index;
                }
            }
        }
    }
    return series;
}

std::vector<std::string> generateSeriesLabels(const FlightData& data)
{
    std::vector<std::string> names;
    names.reserve(data.getBranchCount());
    for (const std::shared_ptr<FlightDataBranch>& branch : data.getBranches())
    {
        names.push_back(branch->getName());
    }
    return generateSeriesLabels(names);
}

}  // namespace QtRocket::PlotUtil
