#include "QtRocket/file/GeneralRocketSaver.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/document/DecalImage.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/file/motor/RockSimMotorWriter.h"
#include "QtRocket/file/openrocket/OpenRocketSaver.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/InsideColorComponentHandler.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The name of the image of the decal of @p appearance, appended to @p names when there is one.
void addDecalName(std::vector<std::string>& names, const std::optional<Appearance>& appearance)
{
    if (appearance.has_value() && appearance->getTexture().has_value())
    {
        names.push_back(appearance->getTexture()->getImageName());
    }
}

/// The names of the decal images the components of @p rocket show, the rocket included: of the
/// decal of each component's appearance and of its inside appearance, each name once, sorted
/// with Strings::javaCompareTo() (Java: a TreeSet of the images, which compare by name).
[[nodiscard]] std::vector<std::string> usedDecalNames(const Rocket& rocket)
{
    std::vector<std::string> names;
    for (const RocketComponent& component : rocket.subtree(true))
    {
        addDecalName(names, component.getAppearance());
        if (const auto* const inside = dynamic_cast<const InsideColorComponent*>(&component))
        {
            addDecalName(names, inside->getInsideColorComponentHandler().getInsideAppearance());
        }
    }
    std::ranges::sort(names, [](const std::string& a, const std::string& b) {
        return Strings::javaCompareTo(a, b) < 0;
    });
    const auto duplicates = std::ranges::unique(names);
    names.erase(duplicates.begin(), duplicates.end());
    return names;
}

/// The thrust-curve motors of @p rocket (Java: collectUniqueMotors()): of every component that is
/// a motor mount (isMotorMount()), in pre-order, the motor of each flight configuration of the
/// rocket that has one, a thrust-curve motor with a digest; the first of each digest.
[[nodiscard]] std::vector<std::shared_ptr<const ThrustCurveMotor>> uniqueMotors(
    const Rocket& rocket)
{
    std::vector<std::shared_ptr<const ThrustCurveMotor>> motors;
    std::set<std::string, std::less<>>                   digests;
    const std::vector<FlightConfigurationId>             ids = rocket.getIds();
    for (const RocketComponent& component : rocket.subtree(true))
    {
        const auto* const mount = dynamic_cast<const MotorMount*>(&component);
        if (mount == nullptr || !mount->isMotorMount())
        {
            continue;
        }
        for (const FlightConfigurationId& fcid : ids)
        {
            const MotorConfiguration& config = mount->getMotorConfig(fcid);
            if (config.isEmpty())
            {
                continue;
            }
            std::shared_ptr<const ThrustCurveMotor> motor =
                std::dynamic_pointer_cast<const ThrustCurveMotor>(config.getMotor());
            if (motor != nullptr && !motor->getDigest().empty() &&
                digests.insert(motor->getDigest()).second)
            {
                motors.push_back(std::move(motor));
            }
        }
    }
    return motors;
}

/// The text of rocket.ork (Java: saveInternal()): OpenRocketSaver's for the types OPENROCKET and
/// WAVEFRONT_OBJ, with its warnings and errors; UNSUPPORTED_FORMAT for the others.
[[nodiscard]] Result<std::string> designText(OpenRocketDocument&   document,
                                             const StorageOptions& options,
                                             const Preferences& preferences, WarningSet& warnings,
                                             ErrorSet& errors)
{
    switch (options.getFileType())
    {
        case StorageOptions::FileType::ROCKSIM:  // HOOK(rocksim): RockSimSaver
        case StorageOptions::FileType::RASAERO:  // HOOK(rasaero): RASAeroSaver
            return fail(ErrorCode::UNSUPPORTED_FORMAT,
                        GeneralRocketSaver::unsupportedFileType(options.getFileType()));
        case StorageOptions::FileType::OPENROCKET:
            break;
        case StorageOptions::FileType::WAVEFRONT_OBJ:
        {
            // Java hands OpenRocketSaver these options, and it writes its format whatever they
            // say; QtRocket's OpenRocketSaver asks for the format it writes. It reads nothing else
            // of them but whether the simulation data are saved.
            StorageOptions design;
            design.setFileType(StorageOptions::FileType::OPENROCKET);
            design.setSaveSimulationData(options.getSaveSimulationData());
            design.setExplicitlySet(options.isExplicitlySet());
            return OpenRocketSaver::save(document, design, preferences, warnings, errors);
        }
    }
    return OpenRocketSaver::save(document, options, preferences, warnings, errors);
}

}  // namespace

std::string GeneralRocketSaver::fileBeyondLoaderBound(std::size_t bytes)
{
    return std::format(
        "The saved file is {} bytes long; QtRocket loads a file of at most {} bytes and will not "
        "open this one.",
        bytes, GeneralRocketLoader::kMaxFileBytes);
}

std::string GeneralRocketSaver::unsupportedFileType(StorageOptions::FileType fileType)
{
    return std::format("QtRocket cannot save a design in the {} format yet.", name(fileType));
}

GeneralRocketSaver::GeneralRocketSaver(const Preferences& preferences)
  : GeneralRocketSaver(preferences, Options{})
{
}

GeneralRocketSaver::GeneralRocketSaver(const Preferences& preferences, Options options)
  : m_preferences(&preferences), m_options(options)
{
}

Result<void> GeneralRocketSaver::save(const std::filesystem::path& file,
                                      OpenRocketDocument&          document)
{
    return save(file, document, document.getDefaultStorageOptions());
}

Result<void> GeneralRocketSaver::save(const std::filesystem::path& file,
                                      OpenRocketDocument& document, StorageOptions& options)
{
    // Java: the temporary file is made in the directory of the file first, the archive is
    // written into it, and it is renamed over the file.
    return replaceFileWith(file,
                           [this, &document, &options] { return saveToBytes(document, options); });
}

Result<std::vector<std::byte>> GeneralRocketSaver::saveToBytes(OpenRocketDocument& document,
                                                               StorageOptions&     options)
{
    m_warnings.clear();
    m_errors.clear();
    m_missingDecal.reset();

    ZipWriter archive;
    archive.setEntryTime(m_options.entryTime);
    {
        // In a block of its own: the text goes as soon as the archive holds it.
        const Result<std::string> text =
            designText(document, options, *m_preferences, m_warnings, m_errors);
        if (!text.has_value())
        {
            return std::unexpected(text.error());
        }
        archive.add("rocket.ork", std::as_bytes(std::span(*text)));
    }

    // Save the file preview image, if any.
    const std::optional<std::vector<std::byte>>& preview = options.getPreviewImage();
    if (preview.has_value() && !preview->empty())
    {
        archive.add("preview.png", *preview);
    }

    // Now the decal images.
    if (const Result<void> decals = addDecals(document, archive); !decals.has_value())
    {
        return std::unexpected(decals.error());
    }

    // The thrust curves of the motors.
    for (const std::shared_ptr<const ThrustCurveMotor>& motor : uniqueMotors(document.getRocket()))
    {
        const std::string rse = RockSimMotorWriter::write(*motor);
        archive.add(std::format("thrustcurves/{}.rse", motor->getDigest()),
                    std::as_bytes(std::span(rse)));
    }

    Result<std::vector<std::byte>> bytes = archive.finish();
    if (!bytes.has_value())
    {
        return bytes;
    }
    if (bytes->size() > GeneralRocketLoader::kMaxFileBytes)
    {
        m_warnings.add(fileBeyondLoaderBound(bytes->size()));
    }
    options.clearPreviewImage();
    return bytes;
}

Result<void> GeneralRocketSaver::addDecals(OpenRocketDocument& document, ZipWriter& archive)
{
    for (const std::string& name : usedDecalNames(document.getRocket()))
    {
        const std::shared_ptr<DecalImage> image = document.findDecalImage(name);
        if (image == nullptr)
        {
            return decalNotFound(name);
        }
        if (image->isIgnored())
        {
            image->setIgnored(false);
            continue;
        }
        const Result<std::vector<std::byte>> bytes = image->getBytes();
        if (!bytes.has_value())
        {
            if (bytes.error().code == ErrorCode::NOT_FOUND)
            {
                m_missingDecal = image;
            }
            return std::unexpected(bytes.error());
        }
        archive.add(image->getName(), *bytes);
    }
    return {};
}

std::int64_t GeneralRocketSaver::estimateFileSize(const OpenRocketDocument& document,
                                                  const StorageOptions&     options)
{
    switch (options.getFileType())
    {
        case StorageOptions::FileType::ROCKSIM:  // HOOK(rocksim): RockSimSaver.estimateFileSize()
        case StorageOptions::FileType::RASAERO:  // HOOK(rasaero): RASAeroSaver.estimateFileSize()
            return 0;
        case StorageOptions::FileType::OPENROCKET:
        case StorageOptions::FileType::WAVEFRONT_OBJ:
            break;
    }
    return OpenRocketSaver::estimateFileSize(document, options);
}

}  // namespace QtRocket
