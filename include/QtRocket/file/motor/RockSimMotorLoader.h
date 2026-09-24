#pragma once

#include <string_view>
#include <vector>

#include "QtRocket/file/motor/AbstractMotorLoader.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// Reads RockSim engine files, .rse (OpenRocket's RockSimMotorLoader): XML with one <engine> per
/// motor inside <engine-database> and <engine-list> (a <version> element and unknown elements are
/// skipped). An engine's attributes give the manufacturer (mfg), the code (code, its delay
/// removed for the designation), the delays (delays, comma-separated: a delay of 90 or more is a
/// plugged motor, and the whole attribute "P" or "plugged" is one), the diameter and length (dia,
/// len, in mm), the initial and propellant masses (initWt, propWt, in g), the type (Type:
/// single-use, reloadable or hybrid; an explicit "unknown" stays unknown, anything else takes the
/// manufacturer's type) and whether the mass and the CG are to be computed (auto-calc-mass,
/// auto-calc-cg: anything but "0" or "false" means yes). <comments> become the description
/// (several are joined with blank lines) and <data> holds <eng-data t f m cg> points (s, N, g,
/// mm). A mass or CG that is missing, not a number or infinite, anywhere in the data, switches
/// on its computation: the mass from AbstractMotorLoader::calculateMass(), the CG at half the
/// length. The digest covers the times, the masses (MASS_PER_TIME, or the initial and burnout
/// masses when computed), the CGs when given, and the thrusts.
///
/// The XML is read with SimpleSax, which accepts and rejects documents as OpenRocket's parser
/// does (see XmlScanner) and keeps a quirk of its DelegatorHandler: an element whose handler
/// ignores it (an unknown element, or any element inside <comments> or <eng-data>) leaves its
/// text buffer and attributes on the stacks, so its parent is closed with the text that follows
/// the ignored element and with that element's attributes. So "<comments>a<b/>c</comments>"
/// gives the description "c", and <eng-data t="0" f="0"><x/></eng-data> is an illegal data point.
///
/// Failures (ErrorCode::PARSE, OpenRocket's messages): XML the JDK's parser rejects, with its
/// message ("Content is not allowed in prolog." for a byte-order mark before the XML, "The
/// reference to entity \"T\" must end with the ';' delimiter." for a bare '&', ...), a missing or
/// non-numeric required attribute ("Manufacturer missing", "Invalid diameter <value>", ...), a
/// propellant mass above the initial mass, a second <data> in an engine, a data point without a
/// numeric time or thrust, an engine without data points, and the one-point curves that
/// finalizeThrustCurve() rejects. The elements before an XML error are read first, as the JDK's
/// parser reports them while it reads, so an engine that fails there fails the file with its own
/// message. Deviation: a document type declaration with an external subset or with markup
/// declarations fails with ErrorCode::UNSUPPORTED_FORMAT (see XmlScanner), where OpenRocket would
/// read the external subset and apply the declarations; RockSim and ThrustCurve write none.
class RockSimMotorLoader final : public AbstractMotorLoader
{
public:
    /// The character set of RockSim engine files (CHARSET_NAME).
    static constexpr std::string_view kCharsetName = "UTF-8";

    /// Any delay this long or longer is a plugged motor (DELAY_LIMIT).
    static constexpr int kDelayLimit = 90;

    RockSimMotorLoader() = default;

    using AbstractMotorLoader::load;

    /// The motors of @p text, a RockSim engine file already decoded, and in @p warnings the
    /// warnings of OpenRocket's handlers (load(Reader, String), public in OpenRocket for the
    /// ThrustCurve download, MotorBurnFile; OpenRocket discards the warnings).
    [[nodiscard]] static Result<std::vector<ThrustCurveMotor::Builder>> loadText(
        std::string_view text, std::string_view filename, WarningSet& warnings);

protected:
    [[nodiscard]] Result<std::vector<ThrustCurveMotor::Builder>> loadText(
        std::string_view text, std::string_view filename) const override;

    [[nodiscard]] Charset getDefaultCharset() const noexcept override { return Charset::UTF_8; }

private:
    // OpenRocket's element handlers (RSEHandler, RSEMotorHandler, RSEMotorDataHandler), defined in
    // the source file. They are nested, as in Java, so that they can use the protected helpers.
    class RseHandler;
    class RseMotorHandler;
    class RseMotorDataHandler;
};

}  // namespace QtRocket
