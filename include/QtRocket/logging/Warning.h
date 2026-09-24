#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/MessagePriority.h"

namespace QtRocket
{

/// A user-visible warning (OpenRocket: logging/Warning). The concrete warnings are the nested
/// classes declared here and defined below; the fixed-text warnings are the static constants at
/// the end. Add a warning to a WarningSet, which copies it.
class Warning : public Message
{
public:
    ~Warning() override = default;

    class LargeAOA;
    class RecoveryHighSpeedDeployment;
    class HighSpeedMainDeployment;
    class LowSpeedMainDeployment;
    class LowSpeedDrogueDeployment;
    class RecoveryDrogueWithoutMain;
    class EventAfterLanding;
    class MissingMotor;
    class Other;

    /// A warning with that text and priority (Java: fromString()). The .ork loader builds every
    /// warning whose type it does not know this way, then sets the priority it read.
    [[nodiscard]] static Other fromString(std::string text, MessagePriority priority);
    /// fromString() with MessagePriority::NORMAL.
    [[nodiscard]] static Other fromString(std::string text);

    // The fixed-text warnings, with OpenRocket's English text and priority (Java: the static
    // finals of Warning). They are const: a set copies them, and the copy carries the sources.
    //
    // WARNING: they are dynamically initialised in Warning.cpp (each allocates its text and draws
    // a random id), so they must not be used during static initialisation: a `static const
    // WarningSet` or a namespace-scope table built from them in another translation unit may run
    // before they exist (the static initialisation order fiasco). Use them from functions only;
    // taking their address is fine at any time.

    /// The body diameter is discontinuous (Java: DIAMETER_DISCONTINUITY).
    static const Other kDiameterDiscontinuity;
    /// A component assembly has an open forward end (Java: OPEN_AIRFRAME_FORWARD).
    static const Other kOpenAirframeForward;
    /// There is a gap in the airframe (Java: AIRFRAME_GAP).
    static const Other kAirframeGap;
    /// Airframe components overlap (Java: AIRFRAME_OVERLAP).
    static const Other kAirframeOverlap;
    /// An inline pod set is completely forward of its parent (Java: PODSET_FORWARD).
    static const Other kPodsetForward;
    /// An inline pod set overlaps its parent (Java: PODSET_OVERLAP).
    static const Other kPodsetOverlap;
    /// The fins are thick compared to the body (Java: THICK_FIN).
    static const Other kThickFin;
    /// The fins have jagged edges (Java: JAGGED_EDGED_FIN).
    static const Other kJaggedEdgedFin;
    /// The fins have zero area (Java: ZERO_AREA_FIN).
    static const Other kZeroAreaFin;
    /// Simulation listeners affected the simulation (Java: LISTENERS_AFFECTED).
    static const Other kListenersAffected;
    /// No recovery device is defined in the simulation (Java: NO_RECOVERY_DEVICE).
    static const Other kNoRecoveryDevice;
    /// An invalid parameter was found in a file and ignored (Java: FILE_INVALID_PARAMETER).
    static const Other kFileInvalidParameter;
    /// Too many parallel fins (Java: PARALLEL_FINS).
    static const Other kParallelFins;
    /// Body calculations may be inaccurate at supersonic speeds (Java: SUPERSONIC).
    static const Other kSupersonic;
    /// A recovery device deployed while on the launch guide (Java: RECOVERY_LAUNCH_ROD).
    static const Other kRecoveryLaunchRod;
    /// A stage began to tumble under thrust (Java: TUMBLE_UNDER_THRUST).
    static const Other kTumbleUnderThrust;
    /// Zero-volume bodies may not simulate accurately (Java: ZERO_VOLUME_BODY).
    static const Other kZeroVolumeBody;
    /// Isolated tube fins may not simulate accurately (Java: TUBE_ISOLATED).
    static const Other kTubeIsolated;
    /// Space between tube fins may not simulate accurately (Java: TUBE_SEPARATION).
    static const Other kTubeSeparation;
    /// Overlapping tube fins may not simulate accurately (Java: TUBE_OVERLAP).
    static const Other kTubeOverlap;
    /// A zero-thickness component can cause issues for 3D printing (Java: OBJ_ZERO_THICKNESS).
    static const Other kObjZeroThickness;
    /// Stage separation occurred at other than the last stage (Java: SEPARATION_ORDER).
    static const Other kSeparationOrder;
    /// Stage separation occurred before the rocket cleared the launch guide (Java:
    /// EARLY_SEPARATION).
    static const Other kEarlySeparation;
    /// A simulation branch contains no data (Java: EMPTY_BRANCH).
    static const Other kEmptyBranch;

protected:
    Warning()                              = default;
    Warning(const Warning&)                = default;
    Warning(Warning&&) noexcept            = default;
    Warning& operator=(const Warning&)     = default;
    Warning& operator=(Warning&&) noexcept = default;
};

/// A large angle of attack was encountered (Java: Warning.LargeAOA). Priority LOW: it reports
/// that the aerodynamic model is out of its envelope, not that the flight went wrong. Two of these
/// are equal whatever their angles, and in a set the larger angle replaces the smaller one.
class Warning::LargeAOA final : public Warning
{
public:
    /// @param aoa the angle of attack in radians; NaN when unknown (the .ork loader passes NaN).
    explicit LargeAOA(double aoa);

    [[nodiscard]] double aoa() const noexcept { return m_aoa; }

    /// "Large angle of attack encountered." for NaN, otherwise "... encountered (15.3°)".
    [[nodiscard]] std::string messageDescription() const override;
    /// True when @p other is a LargeAOA with a larger angle, or this angle is NaN.
    [[nodiscard]] bool replaceBy(const Message& other) const override;
    /// Takes the angle of @p other; throws BugError when it is not a LargeAOA.
    void                                   replaceContents(const Message& other) override;
    [[nodiscard]] std::unique_ptr<Message> clone() const override;
    [[nodiscard]] std::string_view         typeName() const noexcept override { return "LargeAOA"; }

private:
    double m_aoa;
};

/// A recovery device deployed at high speed (Java: Warning.RecoveryHighSpeedDeployment). Two of
/// these are equal whatever their speeds (else every time step would add one); the first stays.
class Warning::RecoveryHighSpeedDeployment final : public Warning
{
public:
    /// @param speed the deployment speed in m/s; NaN when unknown
    /// @param chutes the recovery devices that opened
    explicit RecoveryHighSpeedDeployment(double speed, MessageSources chutes = {});

    [[nodiscard]] double speed() const noexcept { return m_speed; }

    [[nodiscard]] std::string messageDescription() const override;
    [[nodiscard]] bool        replaceBy(const Message& /*other*/) const override { return false; }
    [[nodiscard]] std::unique_ptr<Message> clone() const override;
    [[nodiscard]] std::string_view         typeName() const noexcept override
    {
        return "RecoveryHighSpeedDeployment";
    }

private:
    double m_speed;
};

/// A main parachute deployed at high speed although a drogue is present (Java:
/// Warning.HighSpeedMainDeployment). Equality and replacement as RecoveryHighSpeedDeployment.
class Warning::HighSpeedMainDeployment final : public Warning
{
public:
    explicit HighSpeedMainDeployment(double speed, MessageSources chutes = {});

    [[nodiscard]] double speed() const noexcept { return m_speed; }

    [[nodiscard]] std::string messageDescription() const override;
    [[nodiscard]] bool        replaceBy(const Message& /*other*/) const override { return false; }
    [[nodiscard]] std::unique_ptr<Message> clone() const override;
    [[nodiscard]] std::string_view         typeName() const noexcept override
    {
        return "HighSpeedMainDeployment";
    }

private:
    double m_speed;
};

/// A main parachute deployed at low speed although a drogue is present: the rocket may already
/// be nearly stopped (Java: Warning.LowSpeedMainDeployment).
class Warning::LowSpeedMainDeployment final : public Warning
{
public:
    explicit LowSpeedMainDeployment(double speed, MessageSources chutes = {});

    [[nodiscard]] double speed() const noexcept { return m_speed; }

    [[nodiscard]] std::string messageDescription() const override;
    [[nodiscard]] bool        replaceBy(const Message& /*other*/) const override { return false; }
    [[nodiscard]] std::unique_ptr<Message> clone() const override;
    [[nodiscard]] std::string_view         typeName() const noexcept override
    {
        return "LowSpeedMainDeployment";
    }

private:
    double m_speed;
};

/// A drogue deployed too slowly at apogee to inflate properly (Java:
/// Warning.LowSpeedDrogueDeployment).
class Warning::LowSpeedDrogueDeployment final : public Warning
{
public:
    explicit LowSpeedDrogueDeployment(double speed, MessageSources chutes = {});

    [[nodiscard]] double speed() const noexcept { return m_speed; }

    [[nodiscard]] std::string messageDescription() const override;
    [[nodiscard]] bool        replaceBy(const Message& /*other*/) const override { return false; }
    [[nodiscard]] std::unique_ptr<Message> clone() const override;
    [[nodiscard]] std::string_view         typeName() const noexcept override
    {
        return "LowSpeedDrogueDeployment";
    }

private:
    double m_speed;
};

/// The rocket has a drogue but no main parachute (Java: Warning.RecoveryDrogueWithoutMain).
/// Priority HIGH.
class Warning::RecoveryDrogueWithoutMain final : public Warning
{
public:
    RecoveryDrogueWithoutMain();

    [[nodiscard]] std::string messageDescription() const override;
    [[nodiscard]] bool        replaceBy(const Message& /*other*/) const override { return false; }
    [[nodiscard]] std::unique_ptr<Message> clone() const override;
    [[nodiscard]] std::string_view         typeName() const noexcept override
    {
        return "RecoveryDrogueWithoutMain";
    }
};

/// A flight event occurred after landing (Java: Warning.EventAfterLanding). Priority HIGH. Two of
/// these are equal only when they are the same warning (same id), so a set keeps every one.
/// Deviation: Java compares the UUID references with ==, so two warnings given equal ids by
/// separate setID() calls stay unequal there; here equal ids are equal, which is what Java's own
/// findById() (equals()) and clone() (a shared reference) paths rely on and what the .ork loader
/// needs when it sets a saved id back.
///
/// Extension point: OpenRocket stores the FlightEvent itself. simulation/ does not exist yet, so
/// the event is represented by the display name of its type (FlightEvent.Type.toString(), e.g.
/// "Apogee"), which is all the message text needs. The .ork loader creates the warning without an
/// event and attaches it afterwards (Java: setEvent()).
class Warning::EventAfterLanding final : public Warning
{
public:
    explicit EventAfterLanding(std::optional<std::string> eventType = std::nullopt);

    [[nodiscard]] const std::optional<std::string>& eventType() const noexcept
    {
        return m_eventType;
    }
    void setEventType(std::optional<std::string> eventType) { m_eventType = std::move(eventType); }

    /// "Flight Event occurred after landing: " followed by the event type, when there is one.
    [[nodiscard]] std::string messageDescription() const override;
    [[nodiscard]] bool        replaceBy(const Message& /*other*/) const override { return false; }
    [[nodiscard]] std::unique_ptr<Message> clone() const override;
    [[nodiscard]] std::string_view         typeName() const noexcept override
    {
        return "EventAfterLanding";
    }
    /// Same id only; by value, where Java compares the UUID references (see the class comment).
    [[nodiscard]] bool equals(const Message& other) const override;

private:
    std::optional<std::string> m_eventType;
};

/// A motor referenced by a design was not found in the database (Java: Warning.MissingMotor).
/// Priority HIGH. Two of these are equal when every field, the sources and the priority match;
/// NaN fields compare equal to NaN (Java: Double.doubleToLongBits()).
///
/// Extension point: OpenRocket stores Motor.Type. motor/ does not exist yet, so the type is the
/// name of that enum constant ("SINGLE", "RELOAD", "HYBRID" or "UNKNOWN") until motor/ lands.
class Warning::MissingMotor final : public Warning
{
public:
    MissingMotor();

    [[nodiscard]] const std::optional<std::string>& type() const noexcept { return m_type; }
    void setType(std::optional<std::string> type) { m_type = std::move(type); }
    [[nodiscard]] const std::optional<std::string>& manufacturer() const noexcept
    {
        return m_manufacturer;
    }
    void setManufacturer(std::optional<std::string> manufacturer)
    {
        m_manufacturer = std::move(manufacturer);
    }
    [[nodiscard]] const std::optional<std::string>& designation() const noexcept
    {
        return m_designation;
    }
    void setDesignation(std::optional<std::string> designation)
    {
        m_designation = std::move(designation);
    }
    [[nodiscard]] const std::optional<std::string>& digest() const noexcept { return m_digest; }
    void setDigest(std::optional<std::string> digest) { m_digest = std::move(digest); }
    /// Metres; NaN when unknown.
    [[nodiscard]] double diameter() const noexcept { return m_diameter; }
    void                 setDiameter(double diameter) noexcept { m_diameter = diameter; }
    /// Metres; NaN when unknown.
    [[nodiscard]] double length() const noexcept { return m_length; }
    void                 setLength(double length) noexcept { m_length = length; }
    /// Seconds; NaN when unknown.
    [[nodiscard]] double delay() const noexcept { return m_delay; }
    void                 setDelay(double delay) noexcept { m_delay = delay; }

    /// "No motor with designation 'D12' for manufacturer 'Estes' found." (the manufacturer part
    /// only when set). As in Java, a missing designation prints as 'null'.
    [[nodiscard]] std::string messageDescription() const override;
    [[nodiscard]] bool        replaceBy(const Message& /*other*/) const override { return false; }
    [[nodiscard]] std::unique_ptr<Message> clone() const override;
    [[nodiscard]] std::string_view typeName() const noexcept override { return "MissingMotor"; }
    [[nodiscard]] bool             equals(const Message& other) const override;

private:
    std::optional<std::string> m_type;
    std::optional<std::string> m_manufacturer;
    std::optional<std::string> m_designation;
    std::optional<std::string> m_digest;
    double                     m_diameter;
    double                     m_length;
    double                     m_delay;
};

/// A warning that is only its text (Java: Warning.Other). Two of these are equal when their text,
/// sources and priority are equal. Every fixed-text constant of Warning is one of these.
class Warning::Other final : public Warning
{
public:
    explicit Other(std::string description, MessagePriority priority = MessagePriority::NORMAL);

    [[nodiscard]] const std::string& description() const noexcept { return m_description; }

    [[nodiscard]] std::string messageDescription() const override { return m_description; }
    [[nodiscard]] bool        replaceBy(const Message& /*other*/) const override { return false; }
    [[nodiscard]] std::unique_ptr<Message> clone() const override;
    [[nodiscard]] std::string_view         typeName() const noexcept override { return "Other"; }
    [[nodiscard]] bool                     equals(const Message& other) const override;

private:
    std::string m_description;
};

}  // namespace QtRocket
