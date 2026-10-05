#pragma once

#include <memory>
#include <typeinfo>

#include "QtRocket/simulation/listeners/AbstractSimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

/// The base of a concrete simulation listener: AbstractSimulationListener (or @p Base, another
/// listener class to build on) with the clone() that Java's Object.clone() gives every
/// listener, a copy of the object made with @p Derived's copy constructor. It has no
/// counterpart in OpenRocket, where Object.clone() needs no help.
///
/// The simulation runs on clones of the listeners it is given (see SimulationListener,
/// "Clones"), so a listener records what its caller wants to read afterwards in an object its
/// clones share, held by std::shared_ptr:
///
///     class StepCounter final : public CloneableSimulationListener<StepCounter>
///     {
///     public:
///         void postStep(SimulationStatus& status) override { ++*m_steps; }
///         [[nodiscard]] int steps() const noexcept { return *m_steps; }
///     private:
///         std::shared_ptr<int> m_steps{std::make_shared<int>(0)};
///     };
///
/// used as `auto counter = std::make_shared<StepCounter>();`, given to the simulation and asked
/// for steps() afterwards. A plain `int m_steps` would be copied into each clone and counted
/// there, and the caller's own object would still say 0 (as a primitive field of a Java
/// listener does).
///
/// @p Derived must be the class that derives from this one, must be copy-constructible, and is
/// the class clone() copies: a class derived further from @p Derived needs its own clone()
/// (derive it through CloneableSimulationListener<Further, Derived>), which clone() checks
/// (BugError).
///
/// The constructors are private and @p Derived is a friend, so that only @p Derived can be
/// built on CloneableSimulationListener<Derived> (a class that names another class there does
/// not compile). @p Base must be default-constructible.
template <class Derived, class Base = AbstractSimulationListener>
class CloneableSimulationListener : public Base
{
public:
    CloneableSimulationListener& operator=(const CloneableSimulationListener&) = delete;
    CloneableSimulationListener& operator=(CloneableSimulationListener&&)      = delete;
    ~CloneableSimulationListener() override                                    = default;

    /// A copy of this listener, made with @p Derived's copy constructor (Java: clone()).
    /// @throws BugError when the object is of a class derived from @p Derived that does not
    ///         override clone()
    [[nodiscard]] std::shared_ptr<SimulationListener> clone() const override
    {
        // Java: Object.clone(), a shallow copy of the object's own class.
        const auto* self = dynamic_cast<const Derived*>(this);
        if (self == nullptr || typeid(*this) != typeid(Derived))
        {
            bug("clone() is not overridden by a class derived from a cloneable listener");
        }
        return std::make_shared<Derived>(*self);
    }

private:
    friend Derived;
    CloneableSimulationListener()                                   = default;
    CloneableSimulationListener(const CloneableSimulationListener&) = default;
    CloneableSimulationListener(CloneableSimulationListener&&)      = default;
};

}  // namespace QtRocket
