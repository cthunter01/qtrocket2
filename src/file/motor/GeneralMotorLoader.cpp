#include "QtRocket/file/motor/GeneralMotorLoader.h"

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/file/motor/MotorLoader.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

// Must use this loader in order to avoid recursive instantiation
GeneralMotorLoader::GeneralMotorLoader() noexcept : m_zipLoader(*this) { }

Result<std::vector<ThrustCurveMotor::Builder>> GeneralMotorLoader::load(
    std::span<const std::byte> data, std::string_view filename) const
{
    Result<const MotorLoader*> loader = selectLoader(filename);
    if (!loader)
    {
        return std::unexpected(std::move(loader.error()));
    }
    return (*loader)->load(data, filename);
}

Result<const MotorLoader*> GeneralMotorLoader::selectLoader(std::string_view filename) const
{
    std::string_view  ext;
    const std::size_t point = filename.rfind('.');
    if (point != std::string_view::npos && point > 0)
    {
        ext = filename.substr(point + 1);
    }

    if (Strings::javaEqualsIgnoreCase(ext, "eng"))
    {
        return &m_raspLoader;
    }
    if (Strings::javaEqualsIgnoreCase(ext, "rse"))
    {
        return &m_rockSimLoader;
    }
    if (Strings::javaEqualsIgnoreCase(ext, "zip"))
    {
        return &m_zipLoader;
    }

    return fail(ErrorCode::UNSUPPORTED_FORMAT,
                "Unknown file type, filename=" + std::string(filename));
}

}  // namespace QtRocket
