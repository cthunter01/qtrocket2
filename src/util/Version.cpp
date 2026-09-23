#include "QtRocket/util/Version.h"

#include <string_view>

namespace QtRocket
{

std::string_view version() noexcept
{
    return QTROCKET_VERSION;
}

std::string_view creatorString() noexcept
{
    return "QtRocket " QTROCKET_VERSION;
}

}  // namespace QtRocket
