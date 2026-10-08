#include "QtRocket/simulation/extension/example/RollControl.h"

#include <cmath>
#include <format>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/extension/AbstractSimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

namespace
{

/// Java's RollControl.RollControlListener, with the settings of the extension as they were when
/// it was made (see the class comment of RollControl). The state is Java's: primitive fields,
/// which every clone gets a copy of, and the fin set, which the clones share.
class RollControlListener final : public CloneableSimulationListener<RollControlListener>
{
public:
    explicit RollControlListener(const RollControl& extension)
      : m_controlFinName(extension.getControlFinName()),
        m_startTime(extension.getStartTime()),
        m_setPoint(extension.getSetPoint()),
        m_finRate(extension.getFinRate()),
        m_maxFinAngle(extension.getMaxFinAngle()),
        m_kp(extension.getKP()),
        m_ki(extension.getKI())
    {
    }

    void startSimulation(SimulationStatus& status) override
    {
        // Find the fin set
        m_finset = nullptr;
        for (RocketComponent* c : status.getConfiguration().getActiveComponents())
        {
            auto* finset = dynamic_cast<FinSet*>(c);
            if (finset != nullptr && c->getName() == m_controlFinName)
            {
                m_finset = finset;
                break;
            }
        }
        if (m_finset == nullptr)
        {
            throw SimulationException(
                std::format("A fin set with name '{}' was not found", m_controlFinName));
        }

        // remember the initial fin position so we can set it back after running the
        // simulation
        m_initialFinPosition = m_finset->getCantAngle();
    }

    [[nodiscard]] std::optional<FlightConditions> postFlightConditions(
        SimulationStatus& /*status*/, const FlightConditions& flightConditions) override
    {
        // Store the current roll rate for later use
        m_rollRate = flightConditions.getRollRate();
        return std::nullopt;
    }

    void postStep(SimulationStatus& status) override
    {
        // Activate PID controller only after a specific time
        if (status.getSimulationTime() < m_startTime)
        {
            m_prevTime = status.getSimulationTime();
            return;
        }

        // Determine time step
        const double deltaT = status.getSimulationTime() - m_prevTime;
        m_prevTime          = status.getSimulationTime();

        // PID controller
        const double error = m_setPoint - m_rollRate;

        const double p = m_kp * error;
        m_intState += error * deltaT;
        const double i = m_ki * m_intState;

        const double value = p + i;

        // Limit the fin turn rate
        if (m_finPosition < value)
        {
            m_finPosition = MathUtil::javaMin(m_finPosition + (m_finRate * deltaT), value);
        }
        else
        {
            m_finPosition = MathUtil::javaMax(m_finPosition - (m_finRate * deltaT), value);
        }

        // Clamp the fin angle between bounds
        if (std::abs(m_finPosition) > m_maxFinAngle)
        {
            m_finPosition = MathUtil::clamp(m_finPosition, -m_maxFinAngle, m_maxFinAngle);
        }

        // Set the control fin cant and store the data
        controlledFinSet().setCantAngle(m_finPosition);
        const std::shared_ptr<FlightDataBranch>& branch = status.getFlightDataBranch();
        if (branch == nullptr)
        {
            bug("The simulation status has no flight data branch");
        }
        branch->setValue(RollControl::finCantType(), m_finPosition);
    }

    void endSimulation(SimulationStatus& /*status*/,
                       const SimulationException* /*exception*/) override
    {
        controlledFinSet().setCantAngle(m_initialFinPosition);
    }

private:
    /// The fin set startSimulation() found (Java: a NullPointerException without one). The
    /// engine calls the later hooks only after every startSimulation() has returned.
    [[nodiscard]] FinSet& controlledFinSet() const
    {
        if (m_finset == nullptr)
        {
            bug("The roll control listener was not started: it has no fin set");
        }
        return *m_finset;
    }

    std::string m_controlFinName;
    double      m_startTime;
    double      m_setPoint;
    double      m_finRate;
    double      m_maxFinAngle;
    double      m_kp;
    double      m_ki;

    /// A component of the rocket the engine simulates, which lives as long as the engine and
    /// the flight data of the run.
    FinSet* m_finset{nullptr};

    double m_rollRate{0};

    double m_prevTime{0};
    double m_intState{0};

    double m_initialFinPosition{0};
    double m_finPosition{0};
};

}  // namespace

RollControl::RollControl() : AbstractSimulationExtension(std::string(kId), "Roll Control")
{
    // Java: the static field FIN_CANT_TYPE, made when the class is loaded.
    static_cast<void>(finCantType());
}

const FlightDataType& RollControl::finCantType()
{
    // save fin cant angle as a FlightDataType
    static const FlightDataType& s_finCantType =
        FlightDataType::getType(kFinCantTypeName, kFinCantTypeSymbol, UnitGroupId::ANGLE);
    return s_finCantType;
}

void RollControl::initialize(SimulationConditions& conditions)
{
    conditions.getSimulationListenerList().push_back(std::make_shared<RollControlListener>(*this));
}

std::string RollControl::getName() const
{
    return "Roll Control";
}

std::optional<std::string> RollControl::getDescription() const
{
    // The symbol is a small alpha (U+03B1) and "fc": the alpha as its two UTF-8 bytes, in a
    // literal of its own, since a hexadecimal escape would take the 'f' and the 'c' as digits.
    return "Use a PID control to control a rocket's roll.  The current cant angle of the control "
           "finset is published to flight data as \xCE\xB1"
           "fc. "
           "Since this extension modifies design parameters during the simulation, it causes the "
           "simulation to run <b>much</b> more slowly.";
}

std::vector<const FlightDataType*> RollControl::getFlightDataTypes() const
{
    return {&finCantType()};
}

std::vector<SimulationExtension::InputNumber> RollControl::getInputNumbers() const
{
    constexpr std::string_view kName = "Roll Control";
    return {inputNumber(kName, "startTime", getStartTime()),
            inputNumber(kName, "setPoint", getSetPoint()),
            inputNumber(kName, "finRate", getFinRate()),
            inputNumber(kName, "maxFinAngle", getMaxFinAngle()),
            inputNumber(kName, "KP", getKP()),
            inputNumber(kName, "KI", getKI())};
}

std::unique_ptr<SimulationExtension> RollControl::clone() const
{
    return std::make_unique<RollControl>(*this);
}

std::string RollControl::getControlFinName() const
{
    return m_config.getString("controlFinName", "CONTROL");
}

void RollControl::setControlFinName(std::string_view name)
{
    m_config.put("controlFinName", name);
    fireChangeEvent();
}

double RollControl::getStartTime() const
{
    return m_config.getDouble("startTime", 0.5);
}

void RollControl::setStartTime(double startTime)
{
    m_config.put("startTime", startTime);
    fireChangeEvent();
}

// Desired roll rate (rad/sec)
double RollControl::getSetPoint() const
{
    return m_config.getDouble("setPoint", 0.0);
}

void RollControl::setSetPoint(double rollRate)
{
    m_config.put("setPoint", rollRate);
    fireChangeEvent();
}

// Maximum control fin turn rate (rad/sec)
double RollControl::getFinRate() const
{
    return m_config.getDouble("finRate", 10 * std::numbers::pi / 180);
}

void RollControl::setFinRate(double finRate)
{
    m_config.put("finRate", finRate);
    fireChangeEvent();
}

// Maximum control fin angle (rad)
double RollControl::getMaxFinAngle() const
{
    return m_config.getDouble("maxFinAngle", 15 * std::numbers::pi / 180);
}

void RollControl::setMaxFinAngle(double maxFin)
{
    m_config.put("maxFinAngle", maxFin);
    fireChangeEvent();
}

double RollControl::getKP() const
{
    return m_config.getDouble("KP", 0.007);
}

void RollControl::setKP(double kp)
{
    m_config.put("KP", kp);
    fireChangeEvent();
}

double RollControl::getKI() const
{
    return m_config.getDouble("KI", 0.2);
}

void RollControl::setKI(double ki)
{
    m_config.put("KI", ki);
    fireChangeEvent();
}

}  // namespace QtRocket
