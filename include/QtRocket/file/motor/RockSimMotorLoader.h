#pragma once

#include <string_view>
#include <vector>

#include "QtRocket/file/motor/AbstractMotorLoader.h"
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
/// The XML is read as OpenRocket's SimpleSAX reads it, including a quirk of its DelegatorHandler:
/// an element whose handler ignores it (an unknown element, or any element inside <comments> or
/// <eng-data>) leaves its text buffer and attributes on the stacks, so its parent is closed with
/// the text that follows the ignored element and with that element's attributes. So
/// "<comments>a<b/>c</comments>" gives the description "c", and <eng-data t="0" f="0"><x/>
/// </eng-data> is an illegal data point.
///
/// Failures (ErrorCode::PARSE, OpenRocket's messages): malformed XML, a byte-order mark before
/// the XML (Java's "Content is not allowed in prolog."), duplicate attributes, a missing or
/// non-numeric required attribute ("Manufacturer missing", "Invalid diameter <value>", ...), a
/// propellant mass above the initial mass, a second <data> in an engine, a data point without a
/// numeric time or thrust, an engine without data points, and the one-point curves that
/// finalizeThrustCurve() rejects. Deviation: pugixml keeps an undeclared entity reference
/// ("&nbsp;") as literal text where Java's parser rejects the file, and its messages for
/// malformed XML are its own.
class RockSimMotorLoader final : public AbstractMotorLoader
{
public:
    /// The character set of RockSim engine files (CHARSET_NAME).
    static constexpr std::string_view kCharsetName = "UTF-8";

    /// Any delay this long or longer is a plugged motor (DELAY_LIMIT).
    static constexpr int kDelayLimit = 90;

    RockSimMotorLoader() = default;

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
