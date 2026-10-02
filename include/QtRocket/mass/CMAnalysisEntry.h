#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>

#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

class Motor;
class RocketComponent;

/// One row of MassCalculator::getCMAnalysis() (OpenRocket's masscalc/CMAnalysisEntry): a
/// component, a motor or the whole rocket, with the mass of one instance and the centre of mass
/// of all its instances (the weight is their total mass). The component analysis dialog shows
/// these rows.
///
/// The fields are public, as in Java. Java's source is an Object, a RocketComponent or a Motor;
/// here it is a variant of a non-owning component pointer (valid while the component is in its
/// tree) and a shared motor (motors are immutable and shared). Java's third case, any other
/// object with a null name, cannot be constructed here.
class CMAnalysisEntry
{
public:
    /// What a row describes.
    using Source = std::variant<const RocketComponent*, std::shared_ptr<const Motor>>;

    /// A row for @p component, named after it (its getName()), with no mass yet: eachMass NaN,
    /// totalCM Coordinate::kNaN.
    explicit CMAnalysisEntry(const RocketComponent& component);

    /// A row for @p motor, named after its designation, with no mass yet.
    /// @throws BugError when @p motor is null (Java: NullPointerException).
    explicit CMAnalysisEntry(std::shared_ptr<const Motor> motor);

    /// The key of a component's row in the analysis map: its hashCode(), as Java's
    /// analysisMap.put(component.hashCode(), ...).
    [[nodiscard]] static std::int32_t keyOf(const RocketComponent& component) noexcept;

    /// The key of a motor's row: the Java hashCode() of its designation.
    [[nodiscard]] static std::int32_t keyOf(const Motor& motor) noexcept;

    /// The component of a component row, else nullptr.
    [[nodiscard]] const RocketComponent* getComponent() const noexcept;

    /// The motor of a motor row, else nullptr.
    [[nodiscard]] const Motor* getMotor() const noexcept;

    /// Sets eachMass to @p newMass when it is still NaN (the first instance seen); later calls
    /// change nothing.
    void updateEachMass(double newMass) noexcept;

    /// Sets totalCM to @p newCM when it is still NaN (any component NaN), else to the
    /// mass-weighted average of totalCM and @p newCM (Coordinate::average).
    void updateAverageCM(const Coordinate& newCM) noexcept;

    /// Adds @p aggregateCM (the centre of mass of all active instances of an assembly in one
    /// pass) with updateAverageCM(), then sets eachMass to totalCM's weight divided by
    /// @p instanceCount. Assembly rows are built from separate structure and motor passes, so both
    /// contributions add up before the row shows the launch mass.
    void updateAssemblyMass(const Coordinate& aggregateCM, int instanceCount) noexcept;

    /// The component's name or the motor's designation.
    std::string name;
    /// The component or motor the row describes.
    Source source;
    /// The mass of one instance (kg); NaN until set.
    double eachMass;
    /// The centre of mass of all instances, weight = their total mass; Coordinate::kNaN until set.
    Coordinate totalCM;
};

/// The rows of MassCalculator::getCMAnalysis(), keyed by CMAnalysisEntry::keyOf() (Java: a
/// HashMap<Integer, CMAnalysisEntry>; a std::map iterates in key order on every platform).
using CMAnalysisMap = std::map<std::int32_t, CMAnalysisEntry>;

}  // namespace QtRocket
