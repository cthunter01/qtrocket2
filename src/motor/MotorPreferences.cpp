#include "QtRocket/motor/MotorPreferences.h"

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::MotorPreferences
{

namespace Keys = PreferenceKeys;

// ---- the check boxes

bool isHideSimilar(const Preferences& preferences)
{
    return preferences.getBoolean(Keys::kMotorHideSimilar, true);
}

void setHideSimilar(Preferences& preferences, bool hide)
{
    preferences.putBoolean(Keys::kMotorHideSimilar, hide);
}

bool isHideUnavailable(const Preferences& preferences)
{
    return preferences.getBoolean(Keys::kMotorHideUnavailable, true);
}

void setHideUnavailable(Preferences& preferences, bool hide)
{
    preferences.putBoolean(Keys::kMotorHideUnavailable, hide);
}

bool isLimitDiameter(const Preferences& preferences)
{
    return preferences.getBoolean(Keys::kMotorFilterLimitDiameter, false);
}

void setLimitDiameter(Preferences& preferences, bool limit)
{
    preferences.putBoolean(Keys::kMotorFilterLimitDiameter, limit);
}

bool isLimitLength(const Preferences& preferences)
{
    return preferences.getBoolean(Keys::kMotorFilterLimitLength, false);
}

void setLimitLength(Preferences& preferences, bool limit)
{
    preferences.putBoolean(Keys::kMotorFilterLimitLength, limit);
}

// ---- the excluded manufacturers

std::vector<const Manufacturer*> getExcludedMotorManufacturers(const Preferences& preferences)
{
    std::vector<const Manufacturer*> manufacturers;
    const Preferences* const node = preferences.findNode(Keys::kExcludedMotorManufacturersNode);
    if (node == nullptr)
    {
        return manufacturers;
    }
    for (const std::string& key : node->keys())
    {
        manufacturers.push_back(&Manufacturer::getManufacturer(key));
    }
    return manufacturers;
}

void setExcludedMotorManufacturers(Preferences&                         preferences,
                                   std::span<const Manufacturer* const> manufacturers)
{
    Preferences& node = preferences.getNode(Keys::kExcludedMotorManufacturersNode);
    node.clear();
    for (const Manufacturer* const manufacturer : manufacturers)
    {
        if (manufacturer == nullptr)
        {
            bug("MotorPreferences::setExcludedMotorManufacturers(): a manufacturer is null");
        }
        node.putBoolean(manufacturer->getSimpleName(), true);
    }
}

// ---- the preferred thrust curves

bool isPreferredThrustCurve(const Preferences& preferences, std::string_view digest)
{
    const Preferences* const node = preferences.findNode(Keys::kPreferredThrustCurveMotorNode);
    return node != nullptr && node->getBoolean(digest, false);
}

std::shared_ptr<const ThrustCurveMotor> findPreferredThrustCurve(const Preferences& preferences,
                                                                 const ThrustCurveMotorSet& set)
{
    const Preferences* const node = preferences.findNode(Keys::kPreferredThrustCurveMotorNode);
    if (node == nullptr)
    {
        return nullptr;
    }
    for (const std::shared_ptr<const ThrustCurveMotor>& motor : set.getMotors())
    {
        if (node->getBoolean(motor->getDigest(), false))
        {
            return motor;
        }
    }
    return nullptr;
}

void storePreferredThrustCurve(Preferences& preferences, const ThrustCurveMotorSet& set,
                               const ThrustCurveMotor& chosen)
{
    Preferences& node = preferences.getNode(Keys::kPreferredThrustCurveMotorNode);
    for (const std::shared_ptr<const ThrustCurveMotor>& motor : set.getMotors())
    {
        node.putBoolean(motor->getDigest(), motor.get() == &chosen);
    }
}

}  // namespace QtRocket::MotorPreferences
