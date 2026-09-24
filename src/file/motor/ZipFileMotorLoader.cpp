#include "QtRocket/file/motor/ZipFileMotorLoader.h"

#include <cstddef>
#include <expected>
#include <iterator>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/file/motor/GeneralMotorLoader.h"
#include "QtRocket/file/motor/MotorLoader.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

ZipFileMotorLoader::ZipFileMotorLoader()
  : m_ownLoader(std::make_unique<GeneralMotorLoader>()), m_loader(m_ownLoader.get())
{
}

ZipFileMotorLoader::ZipFileMotorLoader(const MotorLoader& loader) noexcept : m_loader(&loader) { }

ZipFileMotorLoader::~ZipFileMotorLoader() = default;

Result<std::vector<ThrustCurveMotor::Builder>> ZipFileMotorLoader::load(
    std::span<const std::byte> data, std::string_view /*filename*/) const
{
    std::vector<ThrustCurveMotor::Builder> motors;

    // ZipInputStream.getNextEntry() finds no entry unless the data starts with a local file header.
    if (!ZipArchive::looksLikeZip(data))
    {
        return motors;
    }
    Result<ZipArchive> archive = ZipArchive::fromBytes(data);
    if (!archive)
    {
        return std::unexpected(std::move(archive.error()));
    }

    // ZipArchive skips the directory entries.
    for (const ZipArchive::Entry& entry : archive->entries())
    {
        Result<std::vector<ThrustCurveMotor::Builder>> loaded =
            m_loader->load(entry.data, entry.name);
        if (!loaded)
        {
            if (loaded.error().code == ErrorCode::UNSUPPORTED_FORMAT)
            {
                continue;  // Could not read ZIP entry (OpenRocket logs this)
            }
            return std::unexpected(std::move(loaded.error()));
        }
        motors.insert(motors.end(), std::make_move_iterator(loaded->begin()),
                      std::make_move_iterator(loaded->end()));
    }

    return motors;
}

}  // namespace QtRocket
