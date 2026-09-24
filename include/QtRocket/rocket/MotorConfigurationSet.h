#pragma once

#include <string>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FlightConfigurableParameterSet.h"
#include "QtRocket/rocket/MotorConfiguration.h"

namespace QtRocket
{

class MotorMount;
class Preferences;

/// The motor configurations of one motor mount (OpenRocket's motor.MotorConfigurationSet): a
/// FlightConfigurableParameterSet whose default is always an empty configuration of the mount
/// (no motor) under FlightConfigurationId::defaultValueId().
///
/// Copying: a mount's copy builds its set with the (source, newMount) constructor, which makes
/// every configuration anew for the new mount; the plain copy constructor is deleted, since its
/// clones would still point at the source's mount (Java's inherited copy constructor does that).
/// A mount class therefore writes its copy constructor (see RocketComponent). Moving is deleted
/// too: the configurations point at their mount, which never moves. Note that, as in
/// Java, the source's default configuration is not carried over: the new set's default is a
/// fresh empty one, so a default ignition event or delay set on the source's default is lost in
/// the copy (Java's set() ignores the default id; the loop reaches the default too).
///
/// Deviations from OpenRocket:
/// - setDefault() throws UnsupportedOperationException in Java; here it is deleted, and through
///   a reference to the base class it throws BugError (MotorConfiguration::kFixedDefaultMessage).
/// - toDebug() takes the Preferences that choose the motor names; the base class's toDebug()
///   (the generic format) is hidden.
class MotorConfigurationSet : public FlightConfigurableParameterSet<MotorConfiguration>
{
public:
    /// The event type a change of a mount's motors fires (DEFAULT_MOTOR_EVENT_TYPE).
    static constexpr int kDefaultMotorEventType =
        ComponentChangeEvent::kMotorChange | ComponentChangeEvent::kEventChange;

    /// A set for @p mount with an empty default configuration.
    explicit MotorConfigurationSet(MotorMount& mount);

    /// A set for @p newMount holding, for every override of @p sourceSet, a configuration of
    /// @p newMount with the same flight configuration id and the source's settings. The default
    /// is a new empty configuration of @p newMount (see the class comment).
    MotorConfigurationSet(const FlightConfigurableParameterSet<MotorConfiguration>& sourceSet,
                          MotorMount&                                               newMount);

    MotorConfigurationSet(const MotorConfigurationSet&)            = delete;
    MotorConfigurationSet& operator=(const MotorConfigurationSet&) = delete;
    MotorConfigurationSet(MotorConfigurationSet&&)                 = delete;
    MotorConfigurationSet& operator=(MotorConfigurationSet&&)      = delete;
    ~MotorConfigurationSet()                                       = default;

    /// The default of a motor set cannot change (Java: UnsupportedOperationException); the base
    /// class's setDefault() throws BugError.
    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method): hides it on purpose
    void setDefault(MotorConfiguration value) = delete;

    /// " ====== Dumping MotorConfigurationSet: <n> motors in <mount> ======" and one line per
    /// entry, the default first and marked "[DEF]": the entry's key, the configuration's flight
    /// configuration and mid keys, its motor name and its ignition.
    [[nodiscard]] std::string toDebug(const Preferences& preferences) const;
};

}  // namespace QtRocket
