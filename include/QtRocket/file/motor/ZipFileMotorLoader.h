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
/// ("dir/test2.rse"), in the order of the local headers, and the motors are concatenated. An
/// entry the entry loader does not read (MotorLoader::canLoad(), or a load() that fails with
/// ErrorCode::UNSUPPORTED_FORMAT) is skipped; any other failure fails the whole archive, as in
/// OpenRocket.
///
/// The archive is read as Java's ZipInputStream reads it (see ZipInputStream): entry by entry
/// from the local headers, so data that does not start with a local header ("PK\3\4", including
/// empty data and data too short for a header) holds no entries and gives no motors, a truncated
/// archive or one without a central directory gives the entries it holds, and every entry,
/// skipped ones included, is checked against its CRC and sizes (ErrorCode::PARSE with Java's
/// messages). An entry is extracted only when it is loaded, one at a time.
///
/// Deviations: archives nested more than kMaxNesting deep fail with ErrorCode::PARSE ("ZIP
/// archives nested too deeply"), where OpenRocket recurses until its thread's stack overflows
/// (about a thousand levels); an entry's CRC and sizes are checked before its loader sees it,
/// so a corrupt entry fails with the ZIP error even where OpenRocket's loader fails first on its
/// content; running out of memory while extracting an entry fails with ErrorCode::IO instead of
/// ending the program.
class ZipFileMotorLoader final : public MotorLoader
{
public:
    /// A loader that reads the entries with its own GeneralMotorLoader.
    ZipFileMotorLoader();

    /// The deepest nesting of archives read: an archive inside this many others fails.
    static constexpr int kMaxNesting = 16;

    /// A loader that reads the entries with @p loader, which must outlive it (GeneralMotorLoader
    /// passes itself, so that archives inside archives are read too).
    explicit ZipFileMotorLoader(const MotorLoader& loader) noexcept;
    /// A temporary loader would dangle.
    explicit ZipFileMotorLoader(const MotorLoader&& loader) = delete;

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
