#include "QtRocket/motor/CaseInfo.h"

#include <cstddef>
#include <optional>
#include <set>
#include <span>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

namespace
{

using QtRocket::CaseInfo;
using QtRocket::compatibleCases;
using QtRocket::kAllCaseInfos;
using QtRocket::parseCaseInfo;

std::vector<CaseInfo> compatible(CaseInfo caseInfo)
{
    const std::span<const CaseInfo> cases = compatibleCases(caseInfo);
    return {cases.begin(), cases.end()};
}

TEST(CaseInfo, Labels)
{
    EXPECT_EQ(toString(CaseInfo::RMS29_100), "RMS-29/100");
    EXPECT_EQ(toString(CaseInfo::RMS29_360), "RMS-29/360");
    EXPECT_EQ(toString(CaseInfo::RMS38_720), "RMS-38/720");
    EXPECT_EQ(toString(CaseInfo::RMS54_426), "RMS-54/426");
    EXPECT_EQ(toString(CaseInfo::RMS54_2800), "RMS-54/2800");
    EXPECT_EQ(toString(CaseInfo::PRO29_1), "Pro29-1G");
    EXPECT_EQ(toString(CaseInfo::PRO29_6XL), "Pro29-6GXL");
    EXPECT_EQ(toString(CaseInfo::PRO38_2), "Pro38-2G");
    EXPECT_EQ(toString(CaseInfo::PRO38_4), "Pro38-4G");
    EXPECT_EQ(toString(CaseInfo::PRO54_6XL), "Pro54-6GXL");
}

TEST(CaseInfo, Pro38ThreeCarriesTheFourGrainLabel)
{
    // OpenRocket labels PRO38_3 "Pro38-4G", like PRO38_4; its label map keeps the later one.
    EXPECT_EQ(toString(CaseInfo::PRO38_3), "Pro38-4G");
    EXPECT_EQ(parseCaseInfo("Pro38-4G"), CaseInfo::PRO38_4);
    EXPECT_EQ(parseCaseInfo("Pro38-3G"), std::nullopt);
}

TEST(CaseInfo, ParseRoundTripsEveryOtherCase)
{
    for (const CaseInfo caseInfo : kAllCaseInfos)
    {
        if (caseInfo == CaseInfo::PRO38_3)
        {
            continue;
        }
        EXPECT_EQ(parseCaseInfo(toString(caseInfo)), caseInfo) << toString(caseInfo);
    }
}

TEST(CaseInfo, ParseIsExact)
{
    EXPECT_EQ(parseCaseInfo("rms-29/100"), std::nullopt);
    EXPECT_EQ(parseCaseInfo(" RMS-29/100"), std::nullopt);
    EXPECT_EQ(parseCaseInfo("RMS-29/100 "), std::nullopt);
    EXPECT_EQ(parseCaseInfo(""), std::nullopt);
    EXPECT_EQ(parseCaseInfo("RMS-29/150"), std::nullopt);
}

TEST(CaseInfo, AllCasesInDeclarationOrder)
{
    EXPECT_EQ(kAllCaseInfos.size(), 38U);
    EXPECT_EQ(kAllCaseInfos.front(), CaseInfo::RMS29_100);
    EXPECT_EQ(kAllCaseInfos.back(), CaseInfo::PRO54_6XL);
    for (std::size_t i = 0; i < kAllCaseInfos.size(); i++)
    {
        EXPECT_EQ(static_cast<std::size_t>(kAllCaseInfos.at(i)), i);
    }
    // Every label but the duplicated one is distinct.
    std::set<std::string_view> labels;
    for (const CaseInfo caseInfo : kAllCaseInfos)
    {
        labels.insert(toString(caseInfo));
    }
    EXPECT_EQ(labels.size(), kAllCaseInfos.size() - 1);
}

TEST(CaseInfo, CompatibleCases)
{
    using enum CaseInfo;
    EXPECT_EQ(compatible(RMS29_100), (std::vector{RMS29_100, RMS29_120, RMS29_180}));
    EXPECT_EQ(compatible(RMS29_240), (std::vector{RMS29_240, RMS29_360}));
    EXPECT_EQ(compatible(RMS29_360), (std::vector{RMS29_360}));
    EXPECT_EQ(compatible(RMS38_480), (std::vector{RMS38_480, RMS38_600, RMS38_720}));
    EXPECT_EQ(compatible(RMS54_1280), (std::vector{RMS54_1280, RMS54_1706, RMS54_2560}));
    EXPECT_EQ(compatible(RMS54_1706), (std::vector{RMS54_1706, RMS54_2560, RMS54_2800}));
    EXPECT_EQ(compatible(PRO29_5), (std::vector{PRO29_5, PRO29_6, PRO29_6XL}));
    EXPECT_EQ(compatible(PRO38_2), (std::vector{PRO38_2, PRO38_3, PRO38_4}));
    EXPECT_EQ(compatible(PRO38_3), (std::vector{PRO38_3, PRO38_4, PRO38_5}));
    EXPECT_EQ(compatible(PRO54_6), (std::vector{PRO54_6, PRO54_6XL}));
    EXPECT_EQ(compatible(PRO54_6XL), (std::vector{PRO54_6XL}));
}

/// The cases compatible with @p caseInfo are itself and the next ones in declaration order.
void expectCompatibleCasesFollow(CaseInfo caseInfo)
{
    const std::vector<CaseInfo> cases = compatible(caseInfo);
    ASSERT_FALSE(cases.empty());
    EXPECT_EQ(cases.front(), caseInfo);
    EXPECT_LE(cases.size(), 3U);
    for (std::size_t i = 0; i < cases.size(); i++)
    {
        EXPECT_EQ(static_cast<std::size_t>(cases.at(i)), static_cast<std::size_t>(caseInfo) + i);
    }
}

TEST(CaseInfo, EveryCaseIsCompatibleWithItselfFirst)
{
    for (const CaseInfo caseInfo : kAllCaseInfos)
    {
        expectCompatibleCasesFollow(caseInfo);
    }
}

}  // namespace
