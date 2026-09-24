#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/motor/CaseInfo.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class Manufacturer;

/// A motor defined by a sampled thrust curve (OpenRocket's ThrustCurveMotor): time points
/// starting at zero, the thrust at each, and the motor's centre of mass at each with its mass as
/// the weight. Immutable; made by a Builder, whose build() checks the data exactly as OpenRocket
/// does and computes the estimates. Copies are independent values, so OpenRocket's copy/clone
/// methods have no counterpart; motors are shared as std::shared_ptr<const ThrustCurveMotor> and
/// compared by identity (OpenRocket defines no equals() for them), so there is no operator==.
///
/// Values between the samples come from a pseudo-index: the index of the last time point at or
/// before the time plus the fraction of the way to the next one, snapped to 0 or 1 within
/// 1e-4 of either (SNAP_DISTANCE).
class ThrustCurveMotor final : public Motor
{
public:
    /// The largest thrust a curve may have, in N (MAX_THRUST).
    static constexpr double kMaxThrust = 10.0e6;

    /// Collects a motor's data and builds it (ThrustCurveMotor.Builder); defined below.
    class Builder;

    /// The manufacturer (never null: an unset one is "Unknown").
    [[nodiscard]] const Manufacturer& getManufacturer() const noexcept { return *m_manufacturer; }

    /// The time points of the thrust curve.
    [[nodiscard]] const std::vector<double>& getTimePoints() const noexcept { return m_time; }

    /// The thrust at each time point.
    [[nodiscard]] const std::vector<double>& getThrustPoints() const noexcept { return m_thrust; }

    /// The CG at each time point, with the motor's total mass as the weight.
    [[nodiscard]] const std::vector<Coordinate>& getCGPoints() const noexcept { return m_cg; }

    /// The standard ejection delays; Motor::kPluggedDelay marks a plugged option.
    [[nodiscard]] const std::vector<double>& getStandardDelays() const noexcept { return m_delays; }

    /// The case information text, e.g. "RMS-29/180".
    [[nodiscard]] const std::string& getCaseInfo() const noexcept { return m_caseInfo; }

    /// parseCaseInfo(getCaseInfo()) (getCaseInfoEnum()).
    [[nodiscard]] std::optional<CaseInfo> getCaseInfoEnum() const noexcept;

    /// The cases this motor's case can be swapped for, or none when the case is unknown.
    [[nodiscard]] std::span<const CaseInfo> getCompatibleCases() const noexcept;

    [[nodiscard]] const std::string& getPropellantInfo() const noexcept { return m_propellantInfo; }

    /// The initial (total) mass the database gives, or 0 when unset.
    [[nodiscard]] double getInitialMass() const noexcept { return m_initialMass; }

    [[nodiscard]] double getUnitLongitudinalInertia() const noexcept
    {
        return m_unitLongitudinalInertia;
    }
    [[nodiscard]] double getUnitRotationalInertia() const noexcept
    {
        return m_unitRotationalInertia;
    }

    /// thrustcurve.org's motor id, when the motor comes from its database.
    [[nodiscard]] const std::string& getTcMotorId() const noexcept { return m_tcMotorId; }
    [[nodiscard]] const std::string& getInfoUrl() const noexcept { return m_infoUrl; }
    /// The number of data files thrustcurve.org has for the motor, when known.
    [[nodiscard]] std::optional<int> getDataFiles() const noexcept { return m_dataFiles; }
    [[nodiscard]] const std::string& getUpdatedOn() const noexcept { return m_updatedOn; }
    [[nodiscard]] const std::string& getDataSource() const noexcept { return m_dataSource; }
    [[nodiscard]] bool               isSparky() const noexcept { return m_sparky; }
    [[nodiscard]] bool               isAvailable() const noexcept { return m_available; }

    /// The time of the last sample.
    [[nodiscard]] double getCutOffTime() const noexcept { return m_time.back(); }

    /// The number of samples (getDataSize() and getSampleSize()).
    [[nodiscard]] std::size_t getDataSize() const noexcept { return m_time.size(); }
    [[nodiscard]] std::size_t getSampleSize() const noexcept { return m_time.size(); }

    /// The launch mass less the burnout mass.
    [[nodiscard]] double getPropellantMass() const noexcept;

    /// The time interpolated at the pseudo-index of @p motorTime, which is @p motorTime itself
    /// within the curve, the last time point after it, and NaN before ignition (for testing).
    [[nodiscard]] double getTime(double motorTime) const;

    // Motor
    [[nodiscard]] Type               getMotorType() const override { return m_type; }
    [[nodiscard]] const std::string& getCode() const override { return m_code; }
    [[nodiscard]] const std::string& getCommonName() const override { return m_commonName; }
    [[nodiscard]] std::string        getCommonName(double delay) const override;
    [[nodiscard]] const std::string& getDesignation() const override { return m_designation; }
    [[nodiscard]] std::string        getDesignation(double delay) const override;
    [[nodiscard]] const std::string& getDescription() const override { return m_description; }
    [[nodiscard]] double             getDiameter() const override { return m_diameter; }
    [[nodiscard]] double             getLength() const override { return m_length; }
    [[nodiscard]] const std::string& getDigest() const override { return m_digest; }
    [[nodiscard]] double             getLaunchCGx() const override { return m_cg.front().x; }
    [[nodiscard]] double             getBurnoutCGx() const override { return m_cg.back().x; }
    [[nodiscard]] double             getLaunchMass() const override { return m_cg.front().weight; }
    [[nodiscard]] double             getBurnoutMass() const override { return m_cg.back().weight; }
    [[nodiscard]] double getBurnTimeEstimate() const override { return m_burnTimeEstimate; }
    [[nodiscard]] double getAverageThrustEstimate() const override { return m_averageThrust; }
    [[nodiscard]] double getMaxThrustEstimate() const override { return m_maxThrust; }
    [[nodiscard]] double getTotalImpulseEstimate() const override { return m_totalImpulse; }
    [[nodiscard]] double getBurnTime() const override { return m_time.back(); }
    [[nodiscard]] double getThrust(double motorTime) const override;
    [[nodiscard]] double getTotalMass(double motorTime) const override;
    [[nodiscard]] double getPropellantMass(double motorTime) const override;
    [[nodiscard]] double getCMx(double motorTime) const override;
    [[nodiscard]] double getUnitIxx() const override { return m_unitRotationalInertia; }
    [[nodiscard]] double getUnitIyy() const override { return m_unitLongitudinalInertia; }
    [[nodiscard]] double getUnitIzz() const override { return m_unitLongitudinalInertia; }

    /// The order motors are listed in (compareTo()): by manufacturer display name (collated,
    /// Strings::javaPrimaryCollatorCompare), then designation (DesignationComparator), then
    /// diameter and then length, each as (int)((this - other) * 1e6), so differences below a
    /// micrometre count as equal. Returns negative, zero or positive.
    [[nodiscard]] int compareTo(const ThrustCurveMotor& other) const;

    /// A description for logs and messages: "ThrustCurveMotor[<manufacturer> <designation>,
    /// digest=<digest>]". Deviation: OpenRocket prints Object.toString(), the class name and an
    /// identity hash.
    [[nodiscard]] std::string toString() const;

    /// A delay as a string: @p plugged for Motor::kPluggedDelay, otherwise the delay rounded to
    /// tenths (half to even) and written as an integer when it is one ("5", "2.5"), else as
    /// Java's Double.toString writes it (getDelayString()).
    [[nodiscard]] static std::string getDelayString(double delay, std::string_view plugged = "P");

private:
    ThrustCurveMotor();

    /// The pseudo-index of @p motorTime: NaN before ignition (or for NaN), else the index of the
    /// last time point at or before it plus the snapped fraction to the next point.
    [[nodiscard]] double getPseudoIndex(double motorTime) const;

    /// The CG point interpolated at @p pseudoIndex, weight included.
    [[nodiscard]] Coordinate interpolateCenterOfMassAtIndex(double pseudoIndex) const;

    /// computeStatistics(): the maximum thrust, the burn time between the points where the thrust
    /// crosses kMarginalThrust of the maximum, the total impulse and the average thrust over the
    /// burn time.
    void computeStatistics();

    std::string             m_digest;
    const Manufacturer*     m_manufacturer;
    std::string             m_code;
    std::string             m_commonName;
    std::string             m_designation;
    std::string             m_description;
    Type                    m_type{Type::UNKNOWN};
    std::vector<double>     m_delays;
    double                  m_diameter{0.0};
    double                  m_length{0.0};
    std::vector<double>     m_time;
    std::vector<double>     m_thrust;
    std::vector<Coordinate> m_cg;
    std::string             m_caseInfo;
    std::string             m_propellantInfo;
    std::string             m_tcMotorId;
    std::string             m_infoUrl;
    std::optional<int>      m_dataFiles;
    std::string             m_updatedOn;
    std::string             m_dataSource;
    bool                    m_sparky{false};
    double                  m_initialMass{0.0};
    double                  m_maxThrust{0.0};
    double                  m_burnTimeEstimate{0.0};
    double                  m_averageThrust{0.0};
    double                  m_totalImpulse{0.0};
    bool                    m_available{true};
    double                  m_unitRotationalInertia{0.0};
    double                  m_unitLongitudinalInertia{0.0};
};

/// Collects a motor's data and builds it (ThrustCurveMotor.Builder). Every field starts as
/// OpenRocket's does: empty strings and arrays, the manufacturer "Unknown", type UNKNOWN, zero
/// dimensions and initial mass, available, not sparky, no data file count.
class ThrustCurveMotor::Builder
{
public:
    Builder() = default;

    Builder& setCaseInfo(std::string value);
    Builder& setCGPoints(std::vector<Coordinate> cg);
    Builder& setDescription(std::string description);
    Builder& setCode(std::string code);
    Builder& setCommonName(std::string name);
    Builder& setDesignation(std::string designation);
    Builder& setDiameter(double value);
    Builder& setDigest(std::string digest);
    Builder& setInitialMass(double value);
    Builder& setLength(double value);
    Builder& setManufacturer(const Manufacturer& manufacturer);
    Builder& setMotorType(Motor::Type type);
    Builder& setPropellantInfo(std::string value);
    Builder& setTcMotorId(std::string value);
    Builder& setInfoUrl(std::string value);
    Builder& setDataFiles(std::optional<int> value);
    Builder& setUpdatedOn(std::string value);
    Builder& setDataSource(std::string value);
    Builder& setSparky(bool value);
    Builder& setStandardDelays(std::vector<double> delays);
    Builder& setThrustPoints(std::vector<double> thrust);
    Builder& setTimePoints(std::vector<double> time);
    Builder& setAvailability(bool available);

    /// The motor, or a failure (ErrorCode::INVALID_ARGUMENT, with OpenRocket's
    /// IllegalArgumentException message) when the time, thrust and CG arrays differ in length;
    /// there are fewer than two points; the times are not strictly increasing; the first time is
    /// not zero (within MathUtil::kEpsilon); a thrust is negative, NaN or above kMaxThrust; a CG
    /// point has a NaN, lies before the motor's top (x < 0) or past its length, or has a negative
    /// mass; or the type is not a Motor::Type. The checks run in that order. The built motor has
    /// the solid cylinder's unit inertias, the code as its designation when none was set, its
    /// common name reduced to the letter and number when it has the usual form ("B6-0" becomes
    /// "B6") or taken from the designation with simplifyDesignation() when empty, and the burn
    /// time, average and maximum thrust and total impulse estimates. The builder is left
    /// unchanged (OpenRocket's build() returns and keeps mutating one instance), so building
    /// again, after changing a field or not, gives a new motor.
    [[nodiscard]] Result<ThrustCurveMotor> build() const;

    /// Reduces a designation to the impulse class letter and average thrust when, trimmed, it has
    /// the form `^[0-9]*[ -]*([A-Z][0-9]+).*` ("241H115-KS" gives "H115"); otherwise returns it
    /// trimmed and without whitespace ("Micro Maxx II" gives "MicroMaxxII"). The class letter
    /// must be upper case, and a line break after the number defeats the form, as Java's '.'
    /// matches none.
    [[nodiscard]] static std::string simplifyDesignation(std::string_view designation);

private:
    /// The motor being built; build() validates and finishes a copy of it.
    ThrustCurveMotor m_motor;
};

}  // namespace QtRocket
