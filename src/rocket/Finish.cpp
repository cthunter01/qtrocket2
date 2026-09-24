#include "QtRocket/rocket/Finish.h"

#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

std::string_view finishName(Finish finish) noexcept
{
    switch (finish)
    {
        case Finish::ROUGH:
            return "ROUGH";
        case Finish::ROUGHUNFINISHED:
            return "ROUGHUNFINISHED";
        case Finish::UNFINISHED:
            return "UNFINISHED";
        case Finish::NORMAL:
            return "NORMAL";
        case Finish::SMOOTH:
            return "SMOOTH";
        case Finish::OPTIMUM:
            return "OPTIMUM";
        case Finish::POLISHED:
            return "POLISHED";
        case Finish::FINISHPOLISHED:
            return "FINISHPOLISHED";
        case Finish::MIRROR:
            return "MIRROR";
    }
    return "NORMAL";
}

std::optional<Finish> finishFromName(std::string_view name) noexcept
{
    for (const Finish finish : kAllFinishes)
    {
        if (finishName(finish) == name)
        {
            return finish;
        }
    }
    return std::nullopt;
}

std::string_view orkName(Finish finish) noexcept
{
    switch (finish)
    {
        case Finish::ROUGH:
            return "rough";
        case Finish::ROUGHUNFINISHED:
            return "roughunfinished";
        case Finish::UNFINISHED:
            return "unfinished";
        case Finish::NORMAL:
            return "normal";
        case Finish::SMOOTH:
            return "smooth";
        case Finish::OPTIMUM:
            return "optimum";
        case Finish::POLISHED:
            return "polished";
        case Finish::FINISHPOLISHED:
            return "finishpolished";
        case Finish::MIRROR:
            return "mirror";
    }
    return "normal";
}

std::optional<Finish> finishFromOrkName(std::string_view text)
{
    for (const Finish finish : kAllFinishes)
    {
        if (Strings::orkEnumNameMatches(text, finishName(finish)))
        {
            return finish;
        }
    }
    return std::nullopt;
}

std::string_view displayKey(Finish finish) noexcept
{
    switch (finish)
    {
        case Finish::ROUGH:
            return "ExternalComponent.Rough";
        case Finish::ROUGHUNFINISHED:
            return "ExternalComponent.Roughunfinished";
        case Finish::UNFINISHED:
            return "ExternalComponent.Unfinished";
        case Finish::NORMAL:
            return "ExternalComponent.Regularpaint";
        case Finish::SMOOTH:
            return "ExternalComponent.Smoothpaint";
        case Finish::OPTIMUM:
            return "ExternalComponent.Optimumpaint";
        case Finish::POLISHED:
            return "ExternalComponent.Polished";
        case Finish::FINISHPOLISHED:
            return "ExternalComponent.Finishedpolished";
        case Finish::MIRROR:
            return "ExternalComponent.Mirror";
    }
    return "ExternalComponent.Regularpaint";
}

std::string_view displayName(Finish finish) noexcept
{
    switch (finish)
    {
        case Finish::ROUGH:
            return "Rough";
        case Finish::ROUGHUNFINISHED:
            return "Rough unfinished";
        case Finish::UNFINISHED:
            return "Unfinished";
        case Finish::NORMAL:
            return "Regular paint";
        case Finish::SMOOTH:
            return "Smooth paint";
        case Finish::OPTIMUM:
            return "Optimum paint";
        case Finish::POLISHED:
            return "Aircraft sheet-metal";
        case Finish::FINISHPOLISHED:
            return "Finished/polished surface";
        case Finish::MIRROR:
            return "Mirror surface";
    }
    return "Regular paint";
}

std::string toString(Finish finish)
{
    return std::string(displayName(finish)) + " (" +
           unitGroup(UnitGroupId::ROUGHNESS).toStringUnit(roughnessSize(finish)) + ")";
}

}  // namespace QtRocket
