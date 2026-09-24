#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include "QtRocket/file/motor/MotorLoader.h"
#include "QtRocket/file/motor/RaspMotorLoader.h"
#include "QtRocket/file/motor/RockSimMotorLoader.h"
#include "QtRocket/file/motor/ZipFileMotorLoader.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// Reads a motor file of any supported type, chosen by the file name's extension (OpenRocket's
/// GeneralMotorLoader): "eng" for RaspMotorLoader, "rse" for RockSimMotorLoader, "zip" for a
/// ZipFileMotorLoader that reads its entries with this loader, compared ignoring case. The
/// extension is what follows the last '.', when that is not the first character. Any other name
/// fails with ErrorCode::UNSUPPORTED_FORMAT and "Unknown file type, filename=<name>" (OpenRocket's
/// UnknownFileTypeException).
///
/// The zip loader refers back to this one, so a GeneralMotorLoader can be neither copied nor
/// moved; make one where it is needed, as OpenRocket does.
class GeneralMotorLoader final : public MotorLoader
{
public:
    /// The supported extensions, "rse", "eng" and "zip" (getSupportedExtensions()).
    static constexpr std::array<std::string_view, 3> kSupportedExtensions{"rse", "eng", "zip"};

    GeneralMotorLoader() noexcept;

    GeneralMotorLoader(const GeneralMotorLoader&)            = delete;
    GeneralMotorLoader(GeneralMotorLoader&&)                 = delete;
    GeneralMotorLoader& operator=(const GeneralMotorLoader&) = delete;
    GeneralMotorLoader& operator=(GeneralMotorLoader&&)      = delete;
    ~GeneralMotorLoader() override                           = default;

    /// The motors of @p data, read by the loader for @p filename's extension.
    [[nodiscard]] Result<std::vector<ThrustCurveMotor::Builder>> load(
        std::span<const std::byte> data, std::string_view filename) const override;

    /// Whether @p filename has one of the supported extensions.
    [[nodiscard]] bool canLoad(std::string_view filename) const override;

    /// kSupportedExtensions (getSupportedExtensions()).
    [[nodiscard]] static std::span<const std::string_view> getSupportedExtensions() noexcept
    {
        return kSupportedExtensions;
    }

private:
    /// The loader for @p filename (selectLoader()), or the unknown-file-type failure.
    [[nodiscard]] Result<const MotorLoader*> selectLoader(std::string_view filename) const;

    RaspMotorLoader    m_raspLoader;
    RockSimMotorLoader m_rockSimLoader;
    ZipFileMotorLoader m_zipLoader;
};

}  // namespace QtRocket
