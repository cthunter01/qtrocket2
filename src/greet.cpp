#include "QtRocket/greet.h"

#include <format>
#include <string>
#include <string_view>

namespace QtRocket
{

std::string greet(std::string_view name)
{
    return std::format("Hello, {}!", name.empty() ? "world" : name);
}

}  // namespace QtRocket
