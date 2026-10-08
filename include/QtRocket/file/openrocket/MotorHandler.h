#pragma once

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// Reads one <motor> element of a motor mount and finds the motor it names (OpenRocket's
/// file/openrocket/importt/MotorHandler). The motor mount's handler makes one per <motor>, and
/// when the element closes asks it for the motor (getMotor()), then for the ejection delay
/// (getDelay()) and the nozzle exit diameter (getNozzleExitDiameter()), in that order, which is
/// the order of their warnings.
///
/// The children, each plain text that is trimmed (String.trim()) before it is read:
/// - <type>: "single", "reload", "hybrid" or "unknown" (motorTypeFromOrkName()); anything else
///   gives "Unknown motor type '<text>', ignoring." and the type is no criterion of the search.
/// - <manufacturer>, <designation>: the text.
/// - <digest>: the text, but only in a file of format 1.4 or later (kMotorDigestVersion): older
///   formats computed the digest in another way, and their digest is ignored.
/// - <diameter>, <length>: Double.parseDouble; a text that is no number, and a NaN, give
///   "Illegal motor diameter specified, ignoring." (the same text for the length, as in
///   OpenRocket). Neither is used: the motor is searched without them.
/// - <delay>: "none" is the plugged delay (Motor::kPluggedDelay); else Double.parseDouble, and
///   "Illegal motor delay specified, ignoring." for a text that is no number, a NaN or a
///   negative infinity (see the deviations).
/// - <nozzleexitdiameter>: Double.parseDouble; a text that is no number, a value that is not
///   finite and a negative one give "Illegal nozzle exit diameter specified, assuming unknown."
///   and 0.
/// - any other child: AbstractElementHandler's warnings for its text and its attributes.
///
/// getMotor() resolves the motor in three steps:
/// 1. The context's motor finder is asked (type, manufacturer, designation, the digest; never
///    the diameter or the length). Its motor is taken when the file gives no digest (none, or
///    an empty one) or when the motor is compatible with the digest
///    (MotorDigest::isDigestCompatible()); the finder's warnings are passed on.
/// 2. Otherwise, with a digest, the thrust curve the file carries with it is read: the
///    attachment "thrustcurves/<digest>.rse" of the context's attachment factory, with
///    GeneralMotorLoader. The first motor of it that is compatible with the digest is taken,
///    rebuilt with the file's digest as its own, so that a later save refers to it as the file
///    did; the finder's warnings are then dropped. An attachment that is missing
///    (ErrorCode::NOT_FOUND) is passed over silently: most files rely on the database. One that
///    is there and of no use adds a warning: "Embedded motor attachment '<name>' contains no
///    motors.", "Embedded motor attachment '<name>' contains no motor matching digest
///    '<digest>'." or "Unable to load embedded motor attachment '<name>': <reason>" (the reason
///    being the failure's message: of reading the attachment, of the .rse reader, or of a motor
///    of the file that cannot be built, which ends the search in that file).
/// 3. Otherwise the finder's motor after all, which may be none, with the finder's warnings
///    after the attachment's: a database motor whose thrust curve is not the file's is used
///    without a word, as in OpenRocket.
///
/// Deviations from OpenRocket:
/// - A delay of negative infinity ("-Infinity", "-1e400") is not taken: it gives the warning of
///   an illegal delay, and getDelay() then answers as for a missing one. OpenRocket stores it,
///   and a simulation with it cannot run. A positive infinity is the plugged delay however it
///   is written ("Infinity", "1e400"), as in OpenRocket.
/// - A digest that holds a path separator ('/' or '\') or ".." is never made into an attachment
///   name: step 2 is passed over as for a missing attachment. OpenRocket builds the name
///   unchecked, so that a file could name any .rse file outside its "thrustcurves" directory
///   ("../../x") when its attachments are files next to the design.
/// - A motor file that makes OpenRocket's reader throw something other than an IOException or
///   an IllegalArgumentException (a thrust curve of one point: IndexOutOfBoundsException) ends
///   OpenRocket's load of the whole design with that exception. Here it is the third warning,
///   as every other failure of the reader.
/// - When a failure has no message the reason is the name of its ErrorCode ("IO"); Java prints
///   the exception's class name ("IOException").
/// - Without a motor finder in the context getMotor() is a BugError (Java: a
///   NullPointerException); whoever makes the context sets one.
class MotorHandler final : public AbstractElementHandler
{
public:
    /// The file version from which the digest of a motor counts: 1.4, where the digest format
    /// of today was introduced (MOTOR_DIGEST_VERSION).
    static constexpr int kMotorDigestVersion = 104;

    /// A handler that reads the context @p context, which must outlive it.
    explicit MotorHandler(const DocumentLoadingContext& context) noexcept;
    /// A temporary context would dangle.
    explicit MotorHandler(const DocumentLoadingContext&& context) = delete;

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

    /// The motor of this <motor> element, or null when there is none (getMotor(); see the class
    /// comment). Every call searches again.
    /// @throws BugError when the context has no motor finder
    [[nodiscard]] std::shared_ptr<const Motor> getMotor(WarningSet& warnings) const;

    /// The ejection delay to use (getDelay()): the one read, or, when none was read or the one
    /// read was not legal, Motor::kPluggedDelay with the warning "Motor delay not specified,
    /// assuming no ejection charge.".
    [[nodiscard]] double getDelay(WarningSet& warnings) const;

    /// The nozzle exit diameter stored with the motor selection, in metres, or zero when the
    /// file does not give it (getNozzleExitDiameter()). Never negative and always finite.
    [[nodiscard]] double getNozzleExitDiameter() const noexcept { return m_nozzleExitDiameter; }

    // What the element said, for the motor mount's handler and for tests (Java's fields are
    // private; getMotor() is all that reads them there).

    /// The motor type read, or none.
    [[nodiscard]] std::optional<Motor::Type> getType() const noexcept { return m_type; }
    /// The manufacturer read, or none.
    [[nodiscard]] const std::optional<std::string>& getManufacturer() const noexcept
    {
        return m_manufacturer;
    }
    /// The designation read, or none.
    [[nodiscard]] const std::optional<std::string>& getDesignation() const noexcept
    {
        return m_designation;
    }
    /// The digest read, or none: also none in a file older than kMotorDigestVersion.
    [[nodiscard]] const std::optional<std::string>& getDigest() const noexcept { return m_digest; }
    /// The diameter read, in metres, or NaN. It plays no part in the search.
    [[nodiscard]] double getDiameter() const noexcept { return m_diameter; }
    /// The length read, in metres, or NaN. It plays no part in the search.
    [[nodiscard]] double getLength() const noexcept { return m_length; }

private:
    /// The motor of the thrust curve the file carries for @p motorDigest, or null
    /// (loadMotorFromZip()); a warning in @p warnings when the attachment is there and of no
    /// use.
    [[nodiscard]] std::shared_ptr<const Motor> loadMotorFromZip(std::string_view motorDigest,
                                                                WarningSet&      warnings) const;

    // closeElement() in three parts, each for some of the children.

    /// Reads @p content, trimmed, as the <type>, <manufacturer>, <designation> or <digest> that
    /// @p element is; false when it is none of them.
    bool readText(std::string_view element, std::string_view content, WarningSet& warnings);
    /// Reads @p content, trimmed, as the <diameter>, <length>, <delay> or <nozzleexitdiameter>
    /// that @p element is; false when it is none of them.
    bool readNumber(std::string_view element, std::string_view content, WarningSet& warnings);
    /// Reads @p content, trimmed, as a <delay>.
    void readDelay(std::string_view content, WarningSet& warnings);

    const DocumentLoadingContext* m_context;
    std::optional<Motor::Type>    m_type;
    std::optional<std::string>    m_manufacturer;
    std::optional<std::string>    m_designation;
    std::optional<std::string>    m_digest;
    double                        m_diameter{std::numeric_limits<double>::quiet_NaN()};
    double                        m_length{std::numeric_limits<double>::quiet_NaN()};
    double                        m_delay{std::numeric_limits<double>::quiet_NaN()};
    double                        m_nozzleExitDiameter{0.0};
};

}  // namespace QtRocket
