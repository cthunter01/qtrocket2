#include "QtRocket/file/openrocket/MotorHandler.h"

#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/file/AttachmentFactory.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/MotorFinder.h"
#include "QtRocket/file/motor/GeneralMotorLoader.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorDigest.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// Double.parseDouble, or NaN for a text that is no number.
[[nodiscard]] double parseOrNaN(std::string_view text) noexcept
{
    return Strings::javaParseDouble(text).value_or(kNaN);
}

/// Whether @p digest may stand in the name of an attachment: not when it holds a path
/// separator or "..", with which the name would leave the "thrustcurves" directory.
[[nodiscard]] bool isSafeInAttachmentName(std::string_view digest) noexcept
{
    return !digest.contains('/') && !digest.contains('\\') && !digest.contains("..");
}

/// "Unable to load embedded motor attachment '<name>': <reason>" for the failure @p error.
[[nodiscard]] Warning::Other unableToLoad(std::string_view attachmentName, const Error& error)
{
    std::string_view reason = error.message;
    if (Strings::trim(reason).empty())
    {
        reason = toString(error.code);
    }
    return Warning::fromString(
        std::format("Unable to load embedded motor attachment '{}': {}", attachmentName, reason));
}

}  // namespace

MotorHandler::MotorHandler(const DocumentLoadingContext& context) noexcept : m_context(&context) { }

Result<ElementHandler*> MotorHandler::openElement(std::string_view /*element*/,
                                                  const Attributes& /*attributes*/,
                                                  WarningSet& /*warnings*/)
{
    return &PlainTextHandler::instance();
}

std::shared_ptr<const Motor> MotorHandler::getMotor(WarningSet& warnings) const
{
    const MotorFinder* finder = m_context->getMotorFinder();
    if (finder == nullptr)
    {
        bug("The loading context has no motor finder");
    }

    // First try to locate an equivalent motor in the motor database.
    WarningSet                   databaseWarnings;
    std::shared_ptr<const Motor> databaseMotor = finder->findMotor(
        m_type, m_manufacturer, m_designation, kNaN, kNaN, m_digest, databaseWarnings);
    const bool hasDigest = m_digest.has_value() && !m_digest->empty();
    if (databaseMotor != nullptr &&
        (!hasDigest || MotorDigest::isDigestCompatible(*databaseMotor, *m_digest)))
    {
        warnings.addAll(databaseWarnings);
        return databaseMotor;
    }

    // Try loading from embedded .rse file in the zip archive.
    if (hasDigest)
    {
        std::shared_ptr<const Motor> zipMotor = loadMotorFromZip(*m_digest, warnings);
        if (zipMotor != nullptr)
        {
            return zipMotor;
        }
    }

    // Retain the approximate database fallback for files without a usable embedded curve.
    warnings.addAll(databaseWarnings);
    return databaseMotor;
}

std::shared_ptr<const Motor> MotorHandler::loadMotorFromZip(std::string_view motorDigest,
                                                            WarningSet&      warnings) const
{
    if (!isSafeInAttachmentName(motorDigest))
    {
        // Not OpenRocket's: see the class comment.
        return nullptr;
    }
    const std::string attachmentName = std::format("thrustcurves/{}.rse", motorDigest);
    const std::shared_ptr<Attachment> attachment =
        m_context->getAttachmentFactory()->getAttachment(attachmentName);
    QTROCKET_ASSERT(attachment != nullptr);

    const Result<std::vector<std::byte>> bytes = attachment->getBytes();
    if (!bytes)
    {
        if (bytes.error().code != ErrorCode::NOT_FOUND)
        {
            warnings.add(unableToLoad(attachmentName, bytes.error()));
        }
        // Missing attachments are expected for files that rely on the motor database.
        return nullptr;
    }

    const GeneralMotorLoader                             loader;
    const Result<std::vector<ThrustCurveMotor::Builder>> motors =
        loader.load(*bytes, std::format("{}.rse", motorDigest));
    if (!motors)
    {
        warnings.add(unableToLoad(attachmentName, motors.error()));
        return nullptr;
    }
    if (motors->empty())
    {
        warnings.add(Warning::fromString(
            std::format("Embedded motor attachment '{}' contains no motors.", attachmentName)));
        return nullptr;
    }

    for (const ThrustCurveMotor::Builder& builder : *motors)
    {
        const Result<ThrustCurveMotor> motor = builder.build();
        if (!motor)
        {
            warnings.add(unableToLoad(attachmentName, motor.error()));
            return nullptr;
        }
        if (MotorDigest::isDigestCompatible(*motor, motorDigest))
        {
            // Keep the digest referenced by the ORK file stable across future saves.
            ThrustCurveMotor::Builder withDigest = builder;
            withDigest.setDigest(std::string(motorDigest));
            Result<ThrustCurveMotor> rebuilt = withDigest.build();
            if (!rebuilt)
            {
                warnings.add(unableToLoad(attachmentName, rebuilt.error()));
                return nullptr;
            }
            return std::make_shared<const ThrustCurveMotor>(std::move(*rebuilt));
        }
    }

    warnings.add(Warning::fromString(
        std::format("Embedded motor attachment '{}' contains no motor matching digest '{}'.",
                    attachmentName, motorDigest)));
    return nullptr;
}

double MotorHandler::getDelay(WarningSet& warnings) const
{
    if (std::isnan(m_delay))
    {
        warnings.add(
            Warning::fromString("Motor delay not specified, assuming no ejection charge."));
        return Motor::kPluggedDelay;
    }
    return m_delay;
}

Result<void> MotorHandler::closeElement(std::string_view element, const Attributes& attributes,
                                        std::string_view content, WarningSet& warnings)
{
    content = Strings::trim(content);

    if (readText(element, content, warnings) || readNumber(element, content, warnings))
    {
        return {};
    }
    return AbstractElementHandler::closeElement(element, attributes, content, warnings);
}

bool MotorHandler::readText(std::string_view element, std::string_view content,
                            WarningSet& warnings)
{
    if (element == "type")
    {
        // Motor type
        m_type = motorTypeFromOrkName(content);
        if (!m_type.has_value())
        {
            warnings.add(
                Warning::fromString(std::format("Unknown motor type '{}', ignoring.", content)));
        }
    }
    else if (element == "manufacturer")
    {
        // Manufacturer
        m_manufacturer = std::string(content);
    }
    else if (element == "designation")
    {
        // Designation
        m_designation = std::string(content);
    }
    else if (element == "digest")
    {
        // Digest is used only for file versions saved using the same digest algorithm
        if (m_context->getFileVersion() >= kMotorDigestVersion)
        {
            m_digest = std::string(content);
        }
    }
    else
    {
        return false;
    }
    return true;
}

bool MotorHandler::readNumber(std::string_view element, std::string_view content,
                              WarningSet& warnings)
{
    if (element == "diameter")
    {
        // Diameter
        m_diameter = parseOrNaN(content);
        if (std::isnan(m_diameter))
        {
            warnings.add(Warning::fromString("Illegal motor diameter specified, ignoring."));
        }
    }
    else if (element == "length")
    {
        // Length (OpenRocket's warning names the diameter here too)
        m_length = parseOrNaN(content);
        if (std::isnan(m_length))
        {
            warnings.add(Warning::fromString("Illegal motor diameter specified, ignoring."));
        }
    }
    else if (element == "delay")
    {
        readDelay(content, warnings);
    }
    else if (element == "nozzleexitdiameter")
    {
        m_nozzleExitDiameter = parseOrNaN(content);
        if (!std::isfinite(m_nozzleExitDiameter) || m_nozzleExitDiameter < 0)
        {
            warnings.add(
                Warning::fromString("Illegal nozzle exit diameter specified, assuming unknown."));
            m_nozzleExitDiameter = 0.0;
        }
    }
    else
    {
        return false;
    }
    return true;
}

void MotorHandler::readDelay(std::string_view content, WarningSet& warnings)
{
    // Delay
    m_delay = kNaN;
    if (content == "none")
    {
        m_delay = Motor::kPluggedDelay;
        return;
    }
    m_delay = parseOrNaN(content);
    // Not OpenRocket's: a negative infinity is no delay a motor can have (see the class
    // comment). The positive one is the plugged delay.
    if (m_delay == -std::numeric_limits<double>::infinity())
    {
        m_delay = kNaN;
    }
    if (std::isnan(m_delay))
    {
        warnings.add(Warning::fromString("Illegal motor delay specified, ignoring."));
    }
}

}  // namespace QtRocket
