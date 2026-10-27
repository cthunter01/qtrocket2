#pragma once

// Helpers of the motor chooser's tests (tests/gui/dialogs/motor, tests/gui/main): the table as the
// pins of tests/core/motor/chooser/MotorChooserPins.h hash it, the sort keys as they write them,
// the motor mounts of the probe's scenarios, and the chooser's combo boxes and table worked as the
// user works them.

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <QPoint>
#include <QString>
#include <Qt>

#include "QtRocket/gui/util/UserPrompts.h"
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

class QComboBox;
class QWidget;

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

/// The motor chooser's answer @p choice (UserPrompts::chooseMotor()); a test failure, and a choice
/// without a motor, when there is none (bugprone-unchecked-optional-access does not see an
/// ASSERT_TRUE(choice.has_value())).
[[nodiscard]] UserPrompts::MotorChoice chosenMotor(
    const std::optional<UserPrompts::MotorChoice>& choice);

/// Chooses the item @p index of @p box as a click in its popup does (QComboBox's itemSelected():
/// the item made current, then activated(), also for the item that is current already). Choosing
/// through the real popup offscreen is unreliable (a delayed hide during which keys are lost).
void chooseComboItem(QComboBox& box, int index);

/// A double click of @p button at @p position of @p widget as the platform sends it: a click, then
/// a press, the double click and the release (QTest::mouseDClick() sends the double click alone and
/// leaves the button down in QTest's state).
void doubleClickAt(QWidget& widget, QPoint position, Qt::MouseButton button = Qt::LeftButton);

/// Gives the editable @p box the focus and types @p text over its editor's text.
void typeIntoComboBox(QComboBox& box, const QString& text);

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

/// Two motor mounts of one rocket, inner tubes of its body tube: an 18 mm mount 0.075 m long and a
/// 29 mm mount 0.15 m long (inner radii 9 and 14.5 mm, walls 0.5 mm), and a flight configuration in
/// which both have no motor until configure() gives them one (the probe's RESELECT mounts).
class TwoInnerTubeMounts
{
public:
    TwoInnerTubeMounts();

    [[nodiscard]] InnerTube&                   mount18() const noexcept { return *m_mount18; }
    [[nodiscard]] InnerTube&                   mount29() const noexcept { return *m_mount29; }
    [[nodiscard]] const FlightConfigurationId& fcid() const noexcept { return m_fcid; }

    /// Gives @p mount (one of the two) @p motor in fcid(), with the ejection delay 0.
    void configure(InnerTube& mount, std::shared_ptr<const ThrustCurveMotor> motor) const;

private:
    Rocket                m_rocket;
    InnerTube*            m_mount18{nullptr};
    InnerTube*            m_mount29{nullptr};
    FlightConfigurationId m_fcid;
};

}  // namespace QtRocket::Gui::Test
