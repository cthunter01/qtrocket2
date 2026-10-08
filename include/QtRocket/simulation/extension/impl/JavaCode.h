#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "QtRocket/simulation/extension/AbstractSimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"

namespace QtRocket
{

class SimulationConditions;

/// A simulation listener written in Java, named by its class (OpenRocket's
/// simulation/extension/impl/JavaCode), as a class that never executes anything: QtRocket cannot
/// load a Java class. OpenRocket's .ork reader makes one for every <listener> element of an old
/// file. The class exists so that such a design loads, shows the extension by name and keeps
/// the class name for a later save.
///
/// The configuration (the key of the Config, which a .ork file stores): "className" (""), the
/// fully qualified name of the listener class.
///
/// initialize() does nothing without a class name (one that is empty or blank, as Java's
/// StringUtils.isEmpty() decides), and otherwise throws the SimulationException OpenRocket throws
/// when it cannot find the class, "Could not find class <className>" (the text of
/// SimulationExtension.javacode.classnotfound, a space and the name): no class can be found
/// here. So Simulation::simulate() of a simulation with such an extension fails with
/// ErrorCode::SIMULATION_ABORTED, as it does in an OpenRocket that lacks the class.
///
/// Deviations from OpenRocket:
/// - No class is ever loaded: Java looks the class up, checks that it is a SimulationListener,
///   and asks its injector for an instance, which it adds to the conditions (and has two more
///   errors, for a class that is no listener and for one it cannot instantiate):
///   "HOOK(scripting)" in the source file.
class JavaCode final : public AbstractSimulationExtension
{
public:
    /// OpenRocket's class name: the id a .ork file names the extension by.
    static constexpr std::string_view kId =
        "info.openrocket.core.simulation.extension.impl.JavaCode";

    /// An extension without a class name.
    JavaCode();

    /// Whether there is no class name: the extension then does nothing.
    [[nodiscard]] bool isMonteCarloSafe() const override;

    /// Nothing without a class name.
    /// @throws SimulationException for any class name (see the class comment)
    void initialize(SimulationConditions& conditions) override;

    /// "Java code: <className>", or "Java code: none" without a class name (the texts of
    /// SimulationExtension.javacode.name and .name.none).
    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::unique_ptr<SimulationExtension> clone() const override;

    /// The fully qualified name of the listener class.
    [[nodiscard]] std::string getClassName() const;
    /// Stores the class name in the configuration and emits changed() (always).
    void setClassName(std::string_view className);
};

}  // namespace QtRocket
