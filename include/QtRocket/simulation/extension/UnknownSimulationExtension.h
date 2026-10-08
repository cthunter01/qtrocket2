#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "QtRocket/simulation/extension/AbstractSimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/util/Config.h"

namespace QtRocket
{

class SimulationConditions;

/// A simulation extension that nobody provides: the id and the configuration a .ork file gives
/// for an extension whose id no SimulationExtensionProvider knows. It has no counterpart in
/// OpenRocket, whose .ork reader adds the warning "Simulation extension with id '<id>' not
/// found." and drops such an extension, so that saving the design loses it. Here the reader
/// (SingleSimulationHandler) adds the same warning and keeps the extension as one of these: the
/// id and the configuration are there for a later save, entry for entry.
///
/// What it cannot do is act on a simulation, and a simulation that says it has an extension
/// must not silently fly without it: initialize() throws a SimulationException with
/// notFoundText(id), so Simulation::simulate() fails with ErrorCode::SIMULATION_ABORTED until
/// the extension is taken out of the simulation.
///
/// The name is the last part of the id, as for an OpenRocket extension that does not name
/// itself ("x.y.Wobble" gives "Wobble"). There is no description and no flight data type, and
/// the extension is not safe for a Monte Carlo run.
class UnknownSimulationExtension final : public AbstractSimulationExtension
{
public:
    /// An extension with the id @p id and a copy of @p config.
    explicit UnknownSimulationExtension(std::string id, const Config& config = {});

    /// "Simulation extension with id '<id>' not found.": OpenRocket's warning for an extension
    /// id that no provider knows (importt/SingleSimulationHandler), which is also the message
    /// of initialize()'s exception.
    [[nodiscard]] static std::string notFoundText(std::string_view id);

    /// Always throws: the extension cannot act.
    /// @throws SimulationException with notFoundText(getId())
    void initialize(SimulationConditions& conditions) override;

    /// A copy with the same id and a copy of the configuration.
    [[nodiscard]] std::unique_ptr<SimulationExtension> clone() const override;
};

}  // namespace QtRocket
