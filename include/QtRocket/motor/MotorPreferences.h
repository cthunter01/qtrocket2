#pragma once

#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace QtRocket
{

class Manufacturer;
class Preferences;
class ThrustCurveMotor;
class ThrustCurveMotorSet;

/// What OpenRocket's motor chooser keeps in the preferences, as free functions on a Preferences
/// (as material/MaterialPreferences.h does for materials): the manufacturers it hides
/// (SwingPreferences.getExcludedMotorManufacturers() and setExcludedMotorManufacturers(),
/// SwingPreferences.java:1067-1097), the thrust curve last chosen in each motor set
/// (ThrustCurveMotorSelectionPanel.selectedMotor() and selectMotor(), :565-584 and :776-798), and
/// the flags of its check boxes (ThrustCurveMotorSelectionPanel.java:266-285,
/// MotorFilterPanel.java:124-125, 446, 454), under Java's keys (PreferenceKeys.h). The name
/// column's choice is Preferences::getMotorNameColumn(); "Hide motors already used in the mount" is
/// not stored.
///
/// Every getter reads, and creates no node and stores nothing. (SwingPreferences' getBoolean()
/// stores the default of an absent key; QtRocket's stores do not, see SettingsPreferences.)
namespace MotorPreferences
{

// ---- the check boxes

/// "Hide very similar thrust curves" (kMotorHideSimilar, "MotorHideSimilar"), default true.
[[nodiscard]] bool isHideSimilar(const Preferences& preferences);
void               setHideSimilar(Preferences& preferences, bool hide);
/// "Hide motors which are not in regular production" (kMotorHideUnavailable,
/// "MotorHideUnavailable"), default true.
[[nodiscard]] bool isHideUnavailable(const Preferences& preferences);
void               setHideUnavailable(Preferences& preferences, bool hide);
/// "Limit motor diameter to mount diameter" (kMotorFilterLimitDiameter,
/// "motorFilterLimitDiameter"), default false.
[[nodiscard]] bool isLimitDiameter(const Preferences& preferences);
void               setLimitDiameter(Preferences& preferences, bool limit);
/// "Limit motor length to mount length" (kMotorFilterLimitLength, "motorFilterLimitLength"),
/// default false.
[[nodiscard]] bool isLimitLength(const Preferences& preferences);
void               setLimitLength(Preferences& preferences, bool limit);

// ---- the excluded manufacturers

/// SwingPreferences.getExcludedMotorManufacturers(): Manufacturer::getManufacturer() of every
/// key of the node "excludedMotorManufacturers" (kExcludedMotorManufacturersNode), in the order
/// of the node's keys (sorted), whatever the values (a key stored as "false" by hand is
/// excluded too). A key is looked up as any name of a manufacturer, so "aerotech" and "AT" are
/// both AeroTech, which the list then holds twice; a key that names no known manufacturer
/// registers a new one, as Java's getManufacturer() does (its null check never fails). Empty
/// when the node does not exist.
[[nodiscard]] std::vector<const Manufacturer*> getExcludedMotorManufacturers(
    const Preferences& preferences);
/// SwingPreferences.setExcludedMotorManufacturers(): removes every key of the node (created when
/// missing), then stores "true" under the simple name of each of @p manufacturers ("Cesaroni
/// Technology" for Cesaroni Technology Inc.).
/// @throws BugError when a manufacturer is null (Java: a NullPointerException), after the keys
///         before it were stored.
void setExcludedMotorManufacturers(Preferences&                         preferences,
                                   std::span<const Manufacturer* const> manufacturers);

// ---- the preferred thrust curves

/// Whether the thrust curve with the digest @p digest is the one last chosen in its set: the
/// boolean stored under @p digest in the node "PreferredThrustCurveMotors"
/// (kPreferredThrustCurveMotorNode), read as Preferences::getBoolean() does ("true" and "false"
/// without regard to case; false when absent or anything else).
[[nodiscard]] bool isPreferredThrustCurve(const Preferences& preferences, std::string_view digest);

/// The loop of ThrustCurveMotorSelectionPanel.selectMotor(): the first motor of @p set, in the
/// set's order, for which isPreferredThrustCurve() holds, or nullptr when there is none. (Java's
/// selectMotor() asks only when the set has more than one motor, and takes the best curve by
/// ThrustCurveMotorComparator when this finds none; that choice is motor/chooser's.)
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> findPreferredThrustCurve(
    const Preferences& preferences, const ThrustCurveMotorSet& set);

/// ThrustCurveMotorSelectionPanel.selectedMotor(), which OpenRocket calls when the chooser's OK
/// takes @p chosen: stores, for every motor of @p set in its order, "true" under its digest when
/// it is @p chosen (the same object) and "false" otherwise. A @p chosen that is no motor of the
/// set makes them all "false" (Java finds the set from the motor, so that does not happen there).
void storePreferredThrustCurve(Preferences& preferences, const ThrustCurveMotorSet& set,
                               const ThrustCurveMotor& chosen);

}  // namespace MotorPreferences

}  // namespace QtRocket
