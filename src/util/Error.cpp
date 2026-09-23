#include "QtRocket/util/Error.h"

#include <filesystem>
#include <format>
#include <string>
#include <string_view>

namespace QtRocket
{

std::string_view toString(ErrorCode code) noexcept
{
    switch (code)
    {
        case ErrorCode::UNKNOWN:
            return "UNKNOWN";
        case ErrorCode::IO:
            return "IO";
        case ErrorCode::PARSE:
            return "PARSE";
        case ErrorCode::UNSUPPORTED_FORMAT:
            return "UNSUPPORTED_FORMAT";
        case ErrorCode::UNSUPPORTED_VERSION:
            return "UNSUPPORTED_VERSION";
        case ErrorCode::INVALID_ARGUMENT:
            return "INVALID_ARGUMENT";
        case ErrorCode::NOT_FOUND:
            return "NOT_FOUND";
        case ErrorCode::DATABASE:
            return "DATABASE";
        case ErrorCode::CANCELLED:
            return "CANCELLED";
        case ErrorCode::SIMULATION_ABORTED:
            return "SIMULATION_ABORTED";
    }
    return "UNKNOWN";
}

std::string Error::toString() const
{
    const std::string file = std::filesystem::path(where.file_name()).filename().string();
    return std::format("{}: {} ({}:{})", QtRocket::toString(code), message, file, where.line());
}

}  // namespace QtRocket
