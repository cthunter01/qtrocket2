#include "QtRocket/file/GeneralRocketSaver.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <new>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
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
#include "QtRocket/file/openrocket/OrkWriter.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
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

std::string GeneralRocketSaver::attachmentsBeyondLoaderBound(std::size_t count, std::size_t longest)
{
    return std::format(
        "Attachments of the saved file (decal images or thrust curves) are longer than QtRocket "
        "reads: {} of more than {} bytes, the longest {} bytes long. QtRocket will not read them "
        "when it opens this file.",
        count, Attachment::kMaxAttachmentBytes, longest);
}

std::string GeneralRocketSaver::fileWasReadOnly(const std::filesystem::path& file)
{
    return std::format("The file '{}' was read-only; QtRocket replaced it and left it read-only.",
                       pathToUtf8(file));
}

std::string GeneralRocketSaver::unsupportedFileType(StorageOptions::FileType fileType)
{
    return std::format("QtRocket cannot save a design in the {} format yet.", name(fileType));
}

std::string GeneralRocketSaver::decalImageRenamed(std::string_view name, std::string_view stored)
{
    return std::format(
        R"(The decal image "{}" is saved as "{}": a design file cannot hold its name as it is.)",
        Strings::toValidUtf8(name), stored);
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
    // What a failure before the archive is made reports is this save's, not the last one's.
    clearLastSave();
    // Java: the temporary file is made in the directory of the file first, the archive is
    // written into it, and it is renamed over the file.
    const Result<FileReplacement> saved =
        replaceFileWith(file, [this, &document, &options] { return archive(document, options); });
    if (!saved.has_value())
    {
        return std::unexpected(saved.error());
    }
    // QtRocket's own (the user's answer of 2026-10-16): Java replaces a read-only file without
    // a word.
    if (saved->wasReadOnly)
    {
        m_warnings.add(fileWasReadOnly(file));
    }
    // Only when the file is there (Java clears it after the archive was written).
    options.clearPreviewImage();
    return {};
}

Result<std::vector<std::byte>> GeneralRocketSaver::saveToBytes(OpenRocketDocument& document,
                                                               StorageOptions&     options)
{
    Result<std::vector<std::byte>> bytes = archive(document, options);
    if (bytes.has_value())
    {
        options.clearPreviewImage();
    }
    return bytes;
}

void GeneralRocketSaver::clearLastSave()
{
    m_warnings.clear();
    m_errors.clear();
    m_missingDecal.reset();
}

Result<std::vector<std::byte>> GeneralRocketSaver::archive(OpenRocketDocument&   document,
                                                           const StorageOptions& options)
{
    clearLastSave();
    try
    {
        return buildArchive(document, options);
    }
    catch (const std::bad_alloc&)
    {
        // A design whose decals are large images of few bytes in the file it was loaded from
        // may need more memory than there is (QtRocket's own; Java streams the archive).
        return fail(ErrorCode::IO, std::string(kOutOfMemory));
    }
}

Result<std::vector<std::byte>> GeneralRocketSaver::buildArchive(OpenRocketDocument&   document,
                                                                const StorageOptions& options)
{
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
    LargeAttachments large;
    if (const Result<void> decals = addDecals(document, archive, large); !decals.has_value())
    {
        return std::unexpected(decals.error());
    }

    // The thrust curves of the motors.
    addThrustCurves(document, archive, large);

    Result<std::vector<std::byte>> bytes = archive.finish();
    if (!bytes.has_value())
    {
        return bytes;
    }
    // The bounds of the archive that QtRocket's loader has, in the order in which it meets them
    // (decision O12 of tier 10 and the user's answer of 2026-10-16; see the class comment).
    if (bytes->size() > GeneralRocketLoader::kMaxFileBytes)
    {
        m_warnings.add(fileBeyondLoaderBound(bytes->size()));
    }
    if (large.count > 0)
    {
        m_warnings.add(attachmentsBeyondLoaderBound(large.count, large.longest));
    }
    return bytes;
}

void GeneralRocketSaver::LargeAttachments::add(std::size_t bytes) noexcept
{
    if (bytes > Attachment::kMaxAttachmentBytes)
    {
        ++count;
        longest = std::max(longest, bytes);
    }
}

Result<void> GeneralRocketSaver::addDecals(OpenRocketDocument& document, ZipWriter& archive,
                                           LargeAttachments& large)
{
    for (const std::string& name : OpenRocketSaver::decalImageNames(document.getRocket()))
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
        // The name every <decal> of the image has in rocket.ork (OrkWriter::decalName()), whose
        // dropped characters the design's text has counted.
        std::size_t       dropped = 0;
        const std::string stored  = OrkWriter::decalImageName(image->getName(), dropped);
        if (stored != image->getName())
        {
            m_warnings.add(decalImageRenamed(image->getName(), stored));
        }
        large.add(bytes->size());
        archive.add(stored, *bytes);
    }
    return {};
}

void GeneralRocketSaver::addThrustCurves(const OpenRocketDocument& document, ZipWriter& archive,
                                         LargeAttachments& large)
{
    for (const std::shared_ptr<const ThrustCurveMotor>& motor : uniqueMotors(document.getRocket()))
    {
        // Decision E3 of tier 10 for the texts of the motor (RockSimMotorWriter escapes the
        // markup but keeps what XML cannot hold): a file both programs can read.
        std::size_t       dropped = 0;
        const std::string rse = OrkWriter::sanitizeText(RockSimMotorWriter::write(*motor), dropped);
        if (dropped > 0)
        {
            m_warnings.add(OpenRocketSaver::kDroppedCharacters);
        }
        large.add(rse.size());
        archive.add(std::format("thrustcurves/{}.rse", motor->getDigest()),
                    std::as_bytes(std::span(rse)));
    }
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
