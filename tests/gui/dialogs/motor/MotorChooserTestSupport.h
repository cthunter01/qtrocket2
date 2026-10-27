#pragma once

// Helpers of the motor chooser's tests (tests/gui/dialogs/motor): the table as the pins of
// tests/core/motor/chooser/MotorChooserPins.h hash it, the sort keys as they write them, and the
// motor mounts of the probe's scenarios.

#include <memory>
#include <string>
#include <string_view>

#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/Rocket.h"

namespace QtRocket
{
class MotorTableSorter;
class ThrustCurveMotorSet;
class ThrustCurveMotorSetDatabase;
}  // namespace QtRocket

namespace QtRocket::Gui
{
class MotorTableModel;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

/// The rows of @p model in its order, each "<model row>\t<the six column texts, tab-separated>\n"
/// (the text whose MD5 the pins' scenarios give).
[[nodiscard]] std::string motorTableRows(const MotorTableModel& model);

/// The MD5 of motorTableRows() in hex.
[[nodiscard]] std::string motorTableMd5(const MotorTableModel& model);

/// The sort keys of @p sorter as the pins write them: "<column><A|D>" each, the primary first,
/// separated by spaces ("4A 2A 0A").
[[nodiscard]] std::string sortKeysText(const MotorTableSorter& sorter);

/// The set of @p database whose manufacturer matches @p manufacturer (Manufacturer::matches(),
/// as the probes' findSet()) and whose designation is @p designation.
/// @throws BugError when there is none
[[nodiscard]] const ThrustCurveMotorSet& findMotorSet(const ThrustCurveMotorSetDatabase& database,
                                                      std::string_view manufacturer,
                                                      std::string_view designation);

/// The 28 mm mount of the probe's scenarios: an inner tube of outer radius 14.5 mm and inner
/// radius 14 mm, 0.1 m long, in a body tube of a rocket's stage, with a flight configuration of
/// its own (the probe's "mount 28 mm").
class InnerTubeMount
{
public:
    InnerTubeMount();

    [[nodiscard]] InnerTube&                   mount() noexcept { return *m_mount; }
    [[nodiscard]] const FlightConfigurationId& fcid() const noexcept { return m_fcid; }

private:
    Rocket                m_rocket;
    InnerTube*            m_mount{nullptr};
    FlightConfigurationId m_fcid;
};

/// A body tube that is a motor mount, in a rocket's stage, whose flight configurations
/// configure() makes (the probe's bodyTubeMount() and configure()).
class BodyTubeMount
{
public:
    BodyTubeMount();

    [[nodiscard]] BodyTube& mount() noexcept { return *m_mount; }

    /// A new flight configuration whose override gives the mount @p motor with the ejection delay
    /// @p delay.
    FlightConfigurationId configure(std::shared_ptr<const ThrustCurveMotor> motor, double delay);

    /// The same with the nozzle exit diameter @p nozzleExitDiameter (at most the motor's
    /// diameter: a BugError otherwise).
    FlightConfigurationId configure(std::shared_ptr<const ThrustCurveMotor> motor, double delay,
                                    double nozzleExitDiameter);

    /// A new flight configuration in which the mount has no motor (the mount's default).
    FlightConfigurationId addEmptyConfiguration();

private:
    Rocket    m_rocket;
    BodyTube* m_mount{nullptr};
};

}  // namespace QtRocket::Gui::Test
