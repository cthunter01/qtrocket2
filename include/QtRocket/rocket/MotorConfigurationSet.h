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
/// too: the configurations point at their mount, which never moves. The new set's default is
/// the source's default made anew for the new mount, so a mount's default ignition (its event,
/// delay and override flag, which the motor tab of the component dialog edits and an .ork file
/// stores) survives every copy of the mount: an undo state, an undo or redo (Rocket::loadFrom()),
/// a copy and paste, a duplicate, a simulation's copy of the rocket, and each tube of a split
/// cluster (InnerTube::makeIndividualClusterComponent(), a copy with new ids).
///
/// Deviations from OpenRocket:
/// - The (source, newMount) constructor keeps the source's default (QtRocket fixes M3's bug 6,
///   the user's answer U-M3-5 of 2026-10-24). Java's makes a fresh default, AUTOMATIC with no
///   delay and no override, and its loop over the source's values reaches the default only
///   through set(), which ignores the default id: so in OpenRocket an undo, a redo, a copy and
///   paste of a mount lose the default ignition the user set (probe of part C5, M3 run 1: a
///   default of BURNOUT + 2.5 s comes back from copy(), undo() and redo() as AUTOMATIC + 0 s
///   without an override), and so does each tube of a split cluster (Java's
///   makeIndividualClusterComponent() goes through copy(); QtRocket's split tubes keep the
///   cluster's default ignition). The default's motor, ejection delay and nozzle exit diameter are
///   kept too; nothing sets them on a default (it never has a motor), so they are the empty ones.
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
    /// is a configuration of @p newMount with the settings of @p sourceSet's default, its
    /// ignition among them (a deviation: Java makes a new empty default; see the class comment).
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
