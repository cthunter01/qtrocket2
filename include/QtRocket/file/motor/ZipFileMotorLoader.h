#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "QtRocket/file/motor/MotorLoader.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// Reads the motors of every file in a ZIP archive (OpenRocket's ZipFileMotorLoader): each entry
/// that is not a directory goes to the entry loader under its full name inside the archive
/// ("dir/test2.rse"), in the archive's order, and the motors are concatenated. An entry of a type
/// the entry loader does not know (ErrorCode::UNSUPPORTED_FORMAT) is skipped; any other failure
/// fails the whole archive, as in OpenRocket.
///
/// Like Java's ZipInputStream, data that does not start with a local file header ("PK\3\4",
/// including empty data) holds no entries and gives no motors rather than a failure; a corrupt
/// archive fails with ErrorCode::PARSE (Deviation: the archive is read through its central
/// directory, with ZipArchive, where ZipInputStream reads the local headers in turn).
class ZipFileMotorLoader final : public MotorLoader
{
public:
    /// A loader that reads the entries with its own GeneralMotorLoader.
    ZipFileMotorLoader();

    /// A loader that reads the entries with @p loader, which must outlive it (GeneralMotorLoader
    /// passes itself, so that archives inside archives are read too).
    explicit ZipFileMotorLoader(const MotorLoader& loader) noexcept;

    ZipFileMotorLoader(const ZipFileMotorLoader&)            = delete;
    ZipFileMotorLoader(ZipFileMotorLoader&&)                 = delete;
    ZipFileMotorLoader& operator=(const ZipFileMotorLoader&) = delete;
    ZipFileMotorLoader& operator=(ZipFileMotorLoader&&)      = delete;
    ~ZipFileMotorLoader() override;

    [[nodiscard]] Result<std::vector<ThrustCurveMotor::Builder>> load(
        std::span<const std::byte> data, std::string_view filename) const override;

private:
    /// The GeneralMotorLoader of the default constructor; empty when the loader was given.
    std::unique_ptr<MotorLoader> m_ownLoader;
    const MotorLoader*           m_loader;
};

}  // namespace QtRocket
