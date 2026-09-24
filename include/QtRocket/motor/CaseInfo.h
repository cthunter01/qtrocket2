#pragma once

#include <array>
#include <optional>
#include <span>
#include <string_view>

namespace QtRocket
{

/// The reloadable motor hardware (cases) OpenRocket offers substitutes for (OpenRocket's
/// CaseInfo), in declaration order.
enum class CaseInfo
{
    RMS29_100,
    RMS29_120,
    RMS29_180,
    RMS29_240,
    RMS29_360,

    RMS38_120,
    RMS38_240,
    RMS38_360,
    RMS38_480,
    RMS38_600,
    RMS38_720,

    RMS54_426,
    RMS54_852,
    RMS54_1280,
    RMS54_1706,
    RMS54_2560,
    RMS54_2800,

    PRO29_1,
    PRO29_2,
    PRO29_3,
    PRO29_4,
    PRO29_5,
    PRO29_6,
    PRO29_6XL,

    PRO38_1,
    PRO38_2,
    PRO38_3,  ///< labelled "Pro38-4G", as in OpenRocket (a typo there: parse() never gives it)
    PRO38_4,
    PRO38_5,
    PRO38_6,
    PRO38_6XL,

    PRO54_1,
    PRO54_2,
    PRO54_3,
    PRO54_4,
    PRO54_5,
    PRO54_6,
    PRO54_6XL,
};

/// CaseInfo.values(), in declaration order.
inline constexpr std::array<CaseInfo, 38> kAllCaseInfos{
    CaseInfo::RMS29_100,  CaseInfo::RMS29_120,  CaseInfo::RMS29_180,  CaseInfo::RMS29_240,
    CaseInfo::RMS29_360,  CaseInfo::RMS38_120,  CaseInfo::RMS38_240,  CaseInfo::RMS38_360,
    CaseInfo::RMS38_480,  CaseInfo::RMS38_600,  CaseInfo::RMS38_720,  CaseInfo::RMS54_426,
    CaseInfo::RMS54_852,  CaseInfo::RMS54_1280, CaseInfo::RMS54_1706, CaseInfo::RMS54_2560,
    CaseInfo::RMS54_2800, CaseInfo::PRO29_1,    CaseInfo::PRO29_2,    CaseInfo::PRO29_3,
    CaseInfo::PRO29_4,    CaseInfo::PRO29_5,    CaseInfo::PRO29_6,    CaseInfo::PRO29_6XL,
    CaseInfo::PRO38_1,    CaseInfo::PRO38_2,    CaseInfo::PRO38_3,    CaseInfo::PRO38_4,
    CaseInfo::PRO38_5,    CaseInfo::PRO38_6,    CaseInfo::PRO38_6XL,  CaseInfo::PRO54_1,
    CaseInfo::PRO54_2,    CaseInfo::PRO54_3,    CaseInfo::PRO54_4,    CaseInfo::PRO54_5,
    CaseInfo::PRO54_6,    CaseInfo::PRO54_6XL,
};

/// The case's label (CaseInfo.toString()), the case info string motors carry: "RMS-29/100",
/// "Pro29-6GXL", ...
[[nodiscard]] std::string_view toString(CaseInfo caseInfo) noexcept;

/// CaseInfo.parse: the case labelled exactly @p label, or nullopt. Where two cases share a label
/// ("Pro38-4G"), the later one wins, as in OpenRocket's label map.
[[nodiscard]] std::optional<CaseInfo> parseCaseInfo(std::string_view label) noexcept;

/// The cases a motor for @p caseInfo also fits (getCompatibleCases()): the case itself and the
/// next longer ones of the same diameter and family, at most three in all.
[[nodiscard]] std::span<const CaseInfo> compatibleCases(CaseInfo caseInfo) noexcept;

}  // namespace QtRocket
