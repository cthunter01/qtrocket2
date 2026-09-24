#include "QtRocket/util/BugError.h"

#include <filesystem>
#include <format>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>

namespace QtRocket
{

namespace
{

[[nodiscard]] std::string formatMessage(std::string_view message, const std::source_location& where)
{
    const std::string file = std::filesystem::path(where.file_name()).filename().string();
    return std::format("BUG: {} ({}:{})", message, file, where.line());
}

}  // namespace

BugError::BugError(std::string_view message, std::source_location where)
  : std::logic_error(formatMessage(message, where)), m_where(where)
{
}

void bug(std::string_view message, std::source_location where)
{
    throw BugError(message, where);
}

}  // namespace QtRocket
