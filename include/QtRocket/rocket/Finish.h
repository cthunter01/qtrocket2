#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace QtRocket
{

/// The surface finish of an external component (OpenRocket's ExternalComponent.Finish), with the
/// surface roughness it stands for. ComponentPreset's FINISH key holds one; ExternalComponent
/// (ported with the concrete components) keeps its finish as one, NORMAL by default.
///
/// POLISHED keeps its historical 2 micrometres and is shown as "Aircraft sheet-metal";
/// FINISHPOLISHED (0.5 micrometres) is what the user sees as the polished surface, as in
/// OpenRocket.
enum class Finish
{
    ROUGH,            ///< 500 micrometres
    ROUGHUNFINISHED,  ///< 250 micrometres
    UNFINISHED,       ///< 150 micrometres
    NORMAL,           ///< regular paint, 60 micrometres
    SMOOTH,           ///< 20 micrometres
    OPTIMUM,          ///< 5 micrometres
    POLISHED,         ///< aircraft sheet-metal, 2 micrometres
    FINISHPOLISHED,   ///< 0.5 micrometres
    MIRROR,           ///< 0
};

/// Every finish, in declaration order (Finish.values()).
inline constexpr std::array<Finish, 9> kAllFinishes{
    Finish::ROUGH,    Finish::ROUGHUNFINISHED, Finish::UNFINISHED,
    Finish::NORMAL,   Finish::SMOOTH,          Finish::OPTIMUM,
    Finish::POLISHED, Finish::FINISHPOLISHED,  Finish::MIRROR};

/// The surface roughness in metres (Finish.getRoughnessSize()).
[[nodiscard]] constexpr double roughnessSize(Finish finish) noexcept
{
    switch (finish)
    {
        case Finish::ROUGH:
            return 500.0e-6;
        case Finish::ROUGHUNFINISHED:
            return 250.0e-6;
        case Finish::UNFINISHED:
            return 150.0e-6;
        case Finish::NORMAL:
            return 60.0e-6;
        case Finish::SMOOTH:
            return 20.0e-6;
        case Finish::OPTIMUM:
            return 5.0e-6;
        case Finish::POLISHED:
            return 2.0e-6;
        case Finish::FINISHPOLISHED:
            return 0.5e-6;
        case Finish::MIRROR:
            return 0.0e-6;
    }
    return 60.0e-6;
}

/// The constant's name, e.g. "FINISHPOLISHED" (Java: name()), which the preset digest writes.
[[nodiscard]] std::string_view finishName(Finish finish) noexcept;

/// The finish named exactly @p name (Finish.valueOf()), or nullopt.
[[nodiscard]] std::optional<Finish> finishFromName(std::string_view name) noexcept;

/// The .ork spelling the external component saver writes in <finish>: the lower-cased name
/// ("normal", "roughunfinished", ...).
[[nodiscard]] std::string_view orkName(Finish finish) noexcept;

/// The finish @p text names, matched as DocumentConfig.findEnum() does; nullopt for anything
/// else.
[[nodiscard]] std::optional<Finish> finishFromOrkName(std::string_view text);

/// The translation key of the finish's name, e.g. "ExternalComponent.Regularpaint".
[[nodiscard]] std::string_view displayKey(Finish finish) noexcept;

/// The English name, e.g. "Regular paint", "Aircraft sheet-metal".
[[nodiscard]] std::string_view displayName(Finish finish) noexcept;

/// Finish.toString(): the English name and the roughness in the default unit of the roughness
/// unit group, e.g. "Regular paint (60 μm)".
[[nodiscard]] std::string toString(Finish finish);

}  // namespace QtRocket
