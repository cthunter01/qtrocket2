#pragma once

#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/util/ModId.h"

namespace QtRocket
{

/// A model of the atmosphere (OpenRocket's models/atmosphere/AtmosphericModel): the conditions at
/// any altitude. The simulation asks it for the conditions at the rocket's altitude every step.
///
/// getConditions() is const and must be safe to call from several threads at once: OpenRocket
/// shares one standard model between every simulation (SimulationOptions.ISA_ATMOSPHERIC_MODEL).
///
/// Java's AtmosphericModel extends Monitorable; here modId() is a pure virtual member, so every
/// model satisfies the Monitorable concept. Models are polymorphic and held through pointers:
/// copying and moving are left to the concrete classes (protected here, so a model cannot be
/// sliced).
class AtmosphericModel
{
public:
    virtual ~AtmosphericModel() = default;

    /// The conditions at @p altitude, in metres above sea level (or above the launch site,
    /// depending on the model).
    [[nodiscard]] virtual AtmosphericConditions getConditions(double altitude) const = 0;

    /// The id of the model's current state (Monitorable).
    [[nodiscard]] virtual ModId modId() const = 0;

protected:
    AtmosphericModel()                                   = default;
    AtmosphericModel(const AtmosphericModel&)            = default;
    AtmosphericModel& operator=(const AtmosphericModel&) = default;
    AtmosphericModel(AtmosphericModel&&)                 = default;
    AtmosphericModel& operator=(AtmosphericModel&&)      = default;
};

}  // namespace QtRocket
