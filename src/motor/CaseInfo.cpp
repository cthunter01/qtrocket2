#include "QtRocket/motor/CaseInfo.h"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace QtRocket
{

namespace
{

/// A case's label and the cases compatible with it, in the order OpenRocket lists them.
struct CaseData
{
    std::string_view        label;
    std::array<CaseInfo, 3> compatible;
    std::size_t             compatibleCount;
};

using enum CaseInfo;

/// Indexed by the enum's declaration order.
constexpr std::array<CaseData, kAllCaseInfos.size()> kCases{{
    {.label = "RMS-29/100", .compatible = {RMS29_100, RMS29_120, RMS29_180}, .compatibleCount = 3},
    {.label = "RMS-29/120", .compatible = {RMS29_120, RMS29_180, RMS29_240}, .compatibleCount = 3},
    {.label = "RMS-29/180", .compatible = {RMS29_180, RMS29_240, RMS29_360}, .compatibleCount = 3},
    {.label = "RMS-29/240", .compatible = {RMS29_240, RMS29_360}, .compatibleCount = 2},
    {.label = "RMS-29/360", .compatible = {RMS29_360}, .compatibleCount = 1},

    {.label = "RMS-38/120", .compatible = {RMS38_120, RMS38_240, RMS38_360}, .compatibleCount = 3},
    {.label = "RMS-38/240", .compatible = {RMS38_240, RMS38_360, RMS38_480}, .compatibleCount = 3},
    {.label = "RMS-38/360", .compatible = {RMS38_360, RMS38_480, RMS38_600}, .compatibleCount = 3},
    {.label = "RMS-38/480", .compatible = {RMS38_480, RMS38_600, RMS38_720}, .compatibleCount = 3},
    {.label = "RMS-38/600", .compatible = {RMS38_600, RMS38_720}, .compatibleCount = 2},
    {.label = "RMS-38/720", .compatible = {RMS38_720}, .compatibleCount = 1},

    {.label = "RMS-54/426", .compatible = {RMS54_426, RMS54_852, RMS54_1280}, .compatibleCount = 3},
    {.label           = "RMS-54/852",
     .compatible      = {RMS54_852, RMS54_1280, RMS54_1706},
     .compatibleCount = 3},
    {.label           = "RMS-54/1280",
     .compatible      = {RMS54_1280, RMS54_1706, RMS54_2560},
     .compatibleCount = 3},
    {.label           = "RMS-54/1706",
     .compatible      = {RMS54_1706, RMS54_2560, RMS54_2800},
     .compatibleCount = 3},
    {.label = "RMS-54/2560", .compatible = {RMS54_2560, RMS54_2800}, .compatibleCount = 2},
    {.label = "RMS-54/2800", .compatible = {RMS54_2800}, .compatibleCount = 1},

    {.label = "Pro29-1G", .compatible = {PRO29_1, PRO29_2, PRO29_3}, .compatibleCount = 3},
    {.label = "Pro29-2G", .compatible = {PRO29_2, PRO29_3, PRO29_4}, .compatibleCount = 3},
    {.label = "Pro29-3G", .compatible = {PRO29_3, PRO29_4, PRO29_5}, .compatibleCount = 3},
    {.label = "Pro29-4G", .compatible = {PRO29_4, PRO29_5, PRO29_6}, .compatibleCount = 3},
    {.label = "Pro29-5G", .compatible = {PRO29_5, PRO29_6, PRO29_6XL}, .compatibleCount = 3},
    {.label = "Pro29-6G", .compatible = {PRO29_6, PRO29_6XL}, .compatibleCount = 2},
    {.label = "Pro29-6GXL", .compatible = {PRO29_6XL}, .compatibleCount = 1},

    {.label = "Pro38-1G", .compatible = {PRO38_1, PRO38_2, PRO38_3}, .compatibleCount = 3},
    {.label = "Pro38-2G", .compatible = {PRO38_2, PRO38_3, PRO38_4}, .compatibleCount = 3},
    // PRO38_3 carries PRO38_4's label in OpenRocket.
    {.label = "Pro38-4G", .compatible = {PRO38_3, PRO38_4, PRO38_5}, .compatibleCount = 3},
    {.label = "Pro38-4G", .compatible = {PRO38_4, PRO38_5, PRO38_6}, .compatibleCount = 3},
    {.label = "Pro38-5G", .compatible = {PRO38_5, PRO38_6, PRO38_6XL}, .compatibleCount = 3},
    {.label = "Pro38-6G", .compatible = {PRO38_6, PRO38_6XL}, .compatibleCount = 2},
    {.label = "Pro38-6GXL", .compatible = {PRO38_6XL}, .compatibleCount = 1},

    {.label = "Pro54-1G", .compatible = {PRO54_1, PRO54_2, PRO54_3}, .compatibleCount = 3},
    {.label = "Pro54-2G", .compatible = {PRO54_2, PRO54_3, PRO54_4}, .compatibleCount = 3},
    {.label = "Pro54-3G", .compatible = {PRO54_3, PRO54_4, PRO54_5}, .compatibleCount = 3},
    {.label = "Pro54-4G", .compatible = {PRO54_4, PRO54_5, PRO54_6}, .compatibleCount = 3},
    {.label = "Pro54-5G", .compatible = {PRO54_5, PRO54_6, PRO54_6XL}, .compatibleCount = 3},
    {.label = "Pro54-6G", .compatible = {PRO54_6, PRO54_6XL}, .compatibleCount = 2},
    {.label = "Pro54-6GXL", .compatible = {PRO54_6XL}, .compatibleCount = 1},
}};

/// The table row of @p caseInfo, or nullptr for a value outside the enum.
[[nodiscard]] const CaseData* find(CaseInfo caseInfo) noexcept
{
    const auto index = static_cast<std::size_t>(caseInfo);
    return index < kCases.size() ? &kCases.at(index) : nullptr;
}

}  // namespace

std::string_view toString(CaseInfo caseInfo) noexcept
{
    const CaseData* data = find(caseInfo);
    return data != nullptr ? data->label : std::string_view{};
}

std::optional<CaseInfo> parseCaseInfo(std::string_view label) noexcept
{
    // OpenRocket fills a map label -> case in declaration order, so a later case replaces an
    // earlier one with the same label: the last match wins.
    std::optional<CaseInfo> result;
    for (const CaseInfo caseInfo : kAllCaseInfos)
    {
        if (toString(caseInfo) == label)
        {
            result = caseInfo;
        }
    }
    return result;
}

std::span<const CaseInfo> compatibleCases(CaseInfo caseInfo) noexcept
{
    const CaseData* data = find(caseInfo);
    if (data == nullptr)
    {
        return {};
    }
    return std::span<const CaseInfo>(data->compatible).first(data->compatibleCount);
}

}  // namespace QtRocket
