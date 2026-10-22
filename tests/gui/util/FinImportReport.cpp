#include "FinImportReport.h"

#include <cctype>
#include <cstddef>
#include <filesystem>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"
#include "TestPaths.h"
#include "figure/FinPointTestSupport.h"

namespace QtRocket::Gui::Test
{

namespace
{

/// The point "(x,y)" @p text writes, each coordinate a double as Java writes it; nullopt when
/// it is none.
[[nodiscard]] std::optional<Coordinate> parsePoint(std::string_view text)
{
    if (!text.starts_with('(') || !text.ends_with(')'))
    {
        return std::nullopt;
    }
    text.remove_prefix(1);
    text.remove_suffix(1);
    const std::size_t comma = text.find(',');
    if (comma == std::string_view::npos)
    {
        return std::nullopt;
    }
    const std::optional<double> x = Strings::javaParseDouble(text.substr(0, comma));
    const std::optional<double> y = Strings::javaParseDouble(text.substr(comma + 1));
    if (!x.has_value() || !y.has_value())
    {
        return std::nullopt;
    }
    return Coordinate(*x, *y);
}

}  // namespace

const FinImportReport& FinImportReport::get()
{
    static const FinImportReport kReport = [] {
        const Result<std::string> text = readTextFile(finImage("report.txt"));
        if (!text.has_value())
        {
            ADD_FAILURE() << "cannot read the report: " << text.error().message;
            return FinImportReport(std::string_view());
        }
        return FinImportReport(*text);
    }();
    return kReport;
}

FinImportReport::FinImportReport(std::string_view text)
{
    while (!text.empty())
    {
        const std::size_t      end  = text.find('\n');
        const std::string_view line = text.substr(0, end);
        text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
        if (line.starts_with("== "))
        {
            m_blocks.emplace_back(std::string(line.substr(3)), std::vector<std::string>());
        }
        else if (!m_blocks.empty() && !line.empty())
        {
            m_blocks.back().second.emplace_back(line);
        }
    }
}

std::vector<std::string> FinImportReport::headers() const
{
    std::vector<std::string> result;
    result.reserve(m_blocks.size());
    for (const auto& [header, lines] : m_blocks)
    {
        result.push_back(header);
    }
    return result;
}

std::optional<std::string> FinImportReport::line(std::string_view header,
                                                 std::string_view key) const
{
    for (const auto& [blockHeader, lines] : m_blocks)
    {
        if (blockHeader != header)
        {
            continue;
        }
        for (const std::string& line : lines)
        {
            if (line.starts_with(key) && line.size() > key.size() && line.at(key.size()) == ' ')
            {
                return line.substr(key.size() + 1);
            }
        }
    }
    return std::nullopt;
}

std::filesystem::path finImage(std::string_view name)
{
    return QtRocket::Test::testDataDir() / "fin-images" / pathFromUtf8(name);
}

std::string outlineText(std::span<const Coordinate> points)
{
    return std::format("{}: {}", points.size(), pointsText(points));
}

std::vector<Coordinate> parseOutline(std::string_view text)
{
    const std::size_t colon = text.find(": ");
    if (colon == std::string_view::npos)
    {
        ADD_FAILURE() << "not an outline: " << text;
        return {};
    }
    std::vector<Coordinate> points;
    std::string_view        rest = text.substr(colon + 2);
    while (!rest.empty())
    {
        const std::size_t               space = rest.find(' ');
        const std::optional<Coordinate> point = parsePoint(rest.substr(0, space));
        if (!point.has_value())
        {
            ADD_FAILURE() << "not an outline: " << text;
            return {};
        }
        points.push_back(*point);
        rest.remove_prefix(space == std::string_view::npos ? rest.size() : space + 1);
    }
    if (std::format("{}", points.size()) != text.substr(0, colon))
    {
        ADD_FAILURE() << "the count does not match the points: " << text;
    }
    return points;
}

std::string finImageTestName(std::string_view name)
{
    std::string result;
    for (const char c : name.substr(0, name.rfind('.')))
    {
        result += std::isalnum(static_cast<unsigned char>(c)) != 0 ? c : '_';
    }
    return result;
}

}  // namespace QtRocket::Gui::Test
