// FlightConfigurationEditing: the operations of the "Motors & Configuration" tab on a small design
// of two stages, their undo steps and close requests, their events and the configurations they
// name, the questions' requests, multi-cell edits, and QtRocket's fixes of OpenRocket's bugs 1 to 5
// (plan 10.3). OpenRocket's own transcripts of the operations are pinned by
// flight_configuration_operations_golden_tests.cpp.

#include "QtRocket/gui/flightconfig/FlightConfigurationEditing.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QObject>
#include <QString>
#include <QStringList>

#include <gtest/gtest.h>

#include "ForwardingUserPrompts.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorConfigurationSet.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Uuid.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"
#include "motor/TestMotorDatabase.h"

namespace QtRocket::Gui
{

namespace
{

using FlightConfigurationEditingTest = Gui::Test::GuiTest;
using Scope                          = UserPrompts::ConfigurationScope;
using DeployEvent                    = DeploymentConfiguration::DeployEvent;
using SeparationEvent                = StageSeparationConfiguration::SeparationEvent;

/// The design of the tests: a sustainer (nose cone, a body tube acting as a mount with an inner
/// tube, also a mount, and a parachute inside) and a booster (a body tube acting as a mount), three
/// configurations: the first with Estes C6-5 in the body tube, A8-3 in the inner tube and C6-0 in
/// the booster, the second with C6-5 in the body tube only, the third without motors. The undo
/// history starts here.
class Design
{
public:
    Design() : m_editing(*m_document, m_services.services())
    {
        Rocket& r         = rocket();
        auto&   sustainer = dynamic_cast<AxialStage&>(r.getChild(0));
        static_cast<void>(sustainer.addChild(std::make_unique<NoseCone>()));
        auto& body = sustainer.addChild(std::make_unique<BodyTube>());
        body.setMotorMount(true);
        auto& inner = body.addChild(std::make_unique<InnerTube>());
        inner.setMotorMount(true);
        const auto& chute    = body.addChild(std::make_unique<Parachute>());
        auto&       booster  = r.addChild(std::make_unique<AxialStage>());
        auto&       boosters = booster.addChild(std::make_unique<BodyTube>());
        boosters.setMotorMount(true);
        m_body        = body.getId();
        m_inner       = inner.getId();
        m_chute       = chute.getId();
        m_booster     = booster.getId();
        m_boosterTube = boosters.getId();
        for (int i = 0; i < 3; ++i)
        {
            m_ids.push_back(r.createFlightConfiguration().getId());
        }
        setMotor(body, m_ids[0], "C6", 5);
        setMotor(inner, m_ids[0], "A8", 3);
        setMotor(boosters, m_ids[0], "C6", 0);
        setMotor(body, m_ids[1], "C6", 5);
        r.setSelectedConfiguration(m_ids[0]);
        r.fireComponentChangeEvent(ComponentChangeEvent::kMotorChange);
        m_document->clearUndo();
    }

    [[nodiscard]] Rocket&                         rocket() const { return m_document->getRocket(); }
    [[nodiscard]] OpenRocketDocument&             document() const { return *m_document; }
    [[nodiscard]] FlightConfigurationEditing&     editing() noexcept { return m_editing; }
    [[nodiscard]] Gui::Test::ScriptedUserPrompts& prompts() noexcept
    {
        return m_services.prompts();
    }
    [[nodiscard]] Gui::Test::TestGuiServices& services() noexcept { return m_services; }
    [[nodiscard]] const std::vector<FlightConfigurationId>& ids() const noexcept { return m_ids; }
    [[nodiscard]] FlightConfigurationId id(std::size_t i) const { return m_ids.at(i); }

    [[nodiscard]] const Uuid& body() const noexcept { return m_body; }
    [[nodiscard]] const Uuid& inner() const noexcept { return m_inner; }
    [[nodiscard]] const Uuid& chute() const noexcept { return m_chute; }
    [[nodiscard]] const Uuid& booster() const noexcept { return m_booster; }
    [[nodiscard]] const Uuid& boosterTube() const noexcept { return m_boosterTube; }

    [[nodiscard]] MotorMount& mount(const Uuid& id) const
    {
        return dynamic_cast<MotorMount&>(*rocket().findComponent(id));
    }
    [[nodiscard]] MotorConfigurationSet& motors(const Uuid& id) const
    {
        return mount(id).getMotorConfigurationSet();
    }
    [[nodiscard]] auto& deployments() const
    {
        return dynamic_cast<RecoveryDevice&>(*rocket().findComponent(m_chute))
            .getDeploymentConfigurations();
    }
    [[nodiscard]] auto& separations() const
    {
        return dynamic_cast<AxialStage&>(*rocket().findComponent(m_booster))
            .getSeparationConfigurations();
    }

    /// Estes' motor of @p designation in the small motor database.
    [[nodiscard]] std::shared_ptr<const ThrustCurveMotor> estes(std::string_view designation) const
    {
        for (const ThrustCurveMotorSet& set : m_motorDatabase.getMotorSets())
        {
            for (const std::shared_ptr<const ThrustCurveMotor>& motor : set.getMotors())
            {
                if (motor->getManufacturer().getSimpleName() == "Estes" &&
                    motor->getDesignation() == designation)
                {
                    return motor;
                }
            }
        }
        ADD_FAILURE() << "no Estes " << designation;
        return nullptr;
    }

    /// The modification ids of the configurations, in order.
    [[nodiscard]] std::vector<ModId> modIds() const
    {
        std::vector<ModId> ids;
        for (const FlightConfigurationId& id : m_ids)
        {
            if (rocket().containsFlightConfigurationId(id))
            {
                ids.push_back(rocket().getFlightConfiguration(id).getModId());
            }
        }
        return ids;
    }

private:
    void setMotor(MotorMount& mount, const FlightConfigurationId& id, std::string_view designation,
                  double delay) const
    {
        MotorConfiguration configuration{mount, id};
        configuration.setMotor(estes(designation));
        configuration.setEjectionDelay(delay);
        mount.setMotorConfig(std::move(configuration), id);
    }

    Gui::Test::TestGuiServices          m_services;
    ThrustCurveMotorSetDatabase         m_motorDatabase = QtRocket::Test::smallMotorDatabase();
    std::unique_ptr<OpenRocketDocument> m_document = OpenRocketDocumentFactory::createNewRocket();
    FlightConfigurationEditing          m_editing;
    std::vector<FlightConfigurationId>  m_ids;
    Uuid                                m_body;
    Uuid                                m_inner;
    Uuid                                m_chute;
    Uuid                                m_booster;
    Uuid                                m_boosterTube;
};

/// The types of the rocket's change events while it lives.
class EventLog
{
public:
    explicit EventLog(Rocket& rocket)
    {
        m_connection = rocket.addComponentChangeListener(
            [this](const ComponentChangeEvent& event) { m_types.push_back(event.getType()); });
    }

    [[nodiscard]] const std::vector<int>& types() const noexcept { return m_types; }

private:
    std::vector<int>                        m_types;
    ComponentChangeSignal::ScopedConnection m_connection;
};

/// Whether the configuration close request came before the undo position @p description: true
/// when every request found the document's next description still another one.
class CloseRequests
{
public:
    CloseRequests(FlightConfigurationEditing& editing, std::string description)
      : m_description(std::move(description))
    {
        // The connection goes with m_context, before the members the slot uses.
        QObject::connect(&editing, &FlightConfigurationEditing::closeConfigDialogRequested,
                         &m_context, [this, &editing] {
                             ++m_count;
                             m_beforeTheUndoPosition =
                                 m_beforeTheUndoPosition &&
                                 editing.document().getUndoDetail().nextDescription !=
                                     m_description;
                         });
    }

    [[nodiscard]] int  count() const noexcept { return m_count; }
    [[nodiscard]] bool beforeTheUndoPosition() const noexcept { return m_beforeTheUndoPosition; }

private:
    std::string m_description;
    int         m_count{0};
    bool        m_beforeTheUndoPosition{true};
    QObject     m_context;
};

/// The ignition of @p mount's motor in @p id's configuration's own copy of its motors.
[[nodiscard]] std::optional<IgnitionEvent> copiedIgnition(const Rocket&                rocket,
                                                          const FlightConfigurationId& id,
                                                          const MotorMount&            mount)
{
    for (const MotorConfiguration& configuration : rocket.getFlightConfiguration(id).getAllMotors())
    {
        if (&configuration.getMount() == &mount)
        {
            return configuration.getIgnitionEvent();
        }
    }
    return std::nullopt;
}

[[nodiscard]] UserPrompts::IgnitionChoice ignition(Scope scope, IgnitionEvent event, double delay)
{
    return {.scope = scope, .event = event, .delay = delay};
}

[[nodiscard]] UserPrompts::DeploymentChoice deployment(Scope scope, DeployEvent event,
                                                       double altitude, double delay)
{
    DeploymentConfiguration value;
    value.setDeployEvent(event);
    value.setDeployAltitude(altitude);
    value.setDeployDelay(delay);
    return {.scope = scope, .deployment = value};
}

[[nodiscard]] UserPrompts::SeparationChoice separation(Scope scope, SeparationEvent event,
                                                       double altitude, double delay)
{
    StageSeparationConfiguration value;
    value.setSeparationEvent(event);
    value.setSeparationAltitude(altitude);
    value.setSeparationDelay(delay);
    return {.scope = scope, .separation = value};
}

/// @p id's value; a test failure (and the error id) without one.
[[nodiscard]] FlightConfigurationId required(const std::optional<FlightConfigurationId>& id)
{
    if (!id)
    {
        ADD_FAILURE() << "no configuration id";
        return FlightConfigurationId::errorId();
    }
    return *id;
}

/// The follow-up of a New Configuration: records the id it is called with and the events fired
/// before it.
class FollowUp
{
public:
    explicit FollowUp(const EventLog& events) noexcept : m_events(&events) { }

    void operator()(const FlightConfigurationId& id)
    {
        m_id           = id;
        m_eventsBefore = m_events->types();
    }

    [[nodiscard]] FlightConfigurationId   id() const { return required(m_id); }
    [[nodiscard]] const std::vector<int>& eventsBefore() const noexcept { return m_eventsBefore; }

private:
    const EventLog*                      m_events;
    std::optional<FlightConfigurationId> m_id;
    std::vector<int>                     m_eventsBefore;
};

/// An OpenDocuments of @p documents.
[[nodiscard]] FlightConfigurationEditing::OpenDocuments documentsOf(
    std::vector<OpenRocketDocument*> documents)
{
    return [documents = std::move(documents)] { return documents; };
}

/// Runs @p operation on @p design and checks its undo step: one close request before the undo
/// position @p description, which the step then carries, and the operation's answer true.
void expectStep(Design& design, std::string_view description,
                const std::function<bool(FlightConfigurationEditing&)>& operation)
{
    SCOPED_TRACE(description);
    const CloseRequests requests(design.editing(), std::string(description));
    EXPECT_TRUE(operation(design.editing()));
    EXPECT_EQ(requests.count(), 1);
    EXPECT_TRUE(requests.beforeTheUndoPosition());
    EXPECT_EQ(design.document().getUndoDescription(), std::optional<std::string>(description));
}

// ------------------------------------------------------------------- undo steps and requests

TEST_F(FlightConfigurationEditingTest, EveryOperationClosesTheDialogBeforeItsUndoStep)
{
    Design                                   design;
    const std::vector<FlightConfigurationId> first{design.id(0)};
    const std::vector<Uuid>                  body{design.body()};
    design.prompts().answerMotor(
        UserPrompts::MotorChoice{.motor = design.estes("A8"), .delay = 3, .nozzleExitDiameter = 0});
    expectStep(design, "Select motor", [&](auto& e) { return e.selectMotor(first, body); });
    expectStep(design, "Delete motor(s)", [&](auto& e) { return e.deleteMotors(first, body); });
    design.prompts().answerIgnition(ignition(Scope::THIS_CONFIGURATION, IgnitionEvent::LAUNCH, 1));
    expectStep(design, "Select ignition", [&](auto& e) { return e.selectIgnition(first, body); });
    expectStep(design, "Reset ignition", [&](auto& e) { return e.resetIgnition(first, body); });
    const std::vector<Uuid> chute{design.chute()};
    design.prompts().answerDeployment(
        deployment(Scope::THIS_CONFIGURATION, DeployEvent::APOGEE, 200, 0));
    expectStep(design, "Select deployment",
               [&](auto& e) { return e.selectDeployment(first, chute); });
    expectStep(design, "Reset deployment",
               [&](auto& e) { return e.resetDeployment(first, chute); });
    const std::vector<Uuid> booster{design.booster()};
    design.prompts().answerSeparation(
        separation(Scope::THIS_CONFIGURATION, SeparationEvent::APOGEE, 200, 1));
    expectStep(design, "Select separation",
               [&](auto& e) { return e.selectSeparation(first, booster); });
    expectStep(design, "Reset separation",
               [&](auto& e) { return e.resetSeparation(first, booster); });
    design.prompts().answerRenameConfiguration(
        {.action = UserPrompts::RenameAction::RENAME, .name = QStringLiteral("Mine")});
    expectStep(design, "Rename configuration(s)",
               [&](auto& e) { return e.renameConfigurations(first); });
    // Bug 5: New, Duplicate and the motor mount check box have steps of their own.
    expectStep(design, "New Configuration",
               [&](auto& e) { return e.newConfiguration().has_value(); });
    expectStep(design, "Duplicate Configuration",
               [&](auto& e) { return !e.duplicateConfigurations(first).empty(); });
    expectStep(design, "Modify motor mount",
               [&](auto& e) { return e.setMotorMount(design.inner(), false); });
    expectStep(design, "Remove configuration(s)",
               [&](auto& e) { return e.removeConfigurations(first); });
}

TEST_F(FlightConfigurationEditingTest, NothingSelectedDoesNothing)
{
    Design                                   design;
    const CloseRequests                      requests(design.editing(), "");
    const std::vector<FlightConfigurationId> none;
    const std::vector<FlightConfigurationId> first{design.id(0)};
    const std::vector<Uuid>                  noComponent;
    const std::vector<Uuid>                  body{design.body()};
    FlightConfigurationEditing&              e = design.editing();
    EXPECT_FALSE(e.selectMotor(none, body));
    EXPECT_FALSE(e.selectMotor(first, noComponent));
    EXPECT_FALSE(e.deleteMotors(none, body));
    EXPECT_FALSE(e.selectIgnition(first, noComponent));
    EXPECT_FALSE(e.resetIgnition(none, body));
    EXPECT_FALSE(e.selectDeployment(none, std::vector<Uuid>{design.chute()}));
    EXPECT_FALSE(e.resetDeployment(first, noComponent));
    EXPECT_FALSE(e.selectSeparation(none, std::vector<Uuid>{design.booster()}));
    EXPECT_FALSE(e.resetSeparation(first, noComponent));
    EXPECT_FALSE(e.renameConfigurations(none));
    EXPECT_FALSE(e.removeConfigurations(none));
    EXPECT_TRUE(e.duplicateConfigurations(none).empty());
    // A mount that acts as asked already.
    EXPECT_FALSE(e.setMotorMount(design.body(), true));
    EXPECT_EQ(requests.count(), 0);
    EXPECT_FALSE(design.document().isUndoAvailable());
    EXPECT_EQ(design.document().getUndoDetail().nextDescription, std::nullopt);
    EXPECT_EQ(design.rocket().getConfigurationCount(), 3);
}

TEST_F(FlightConfigurationEditingTest, RefusesTheDefaultConfigurationAndOtherComponents)
{
    Design                                   design;
    const std::vector<FlightConfigurationId> defaultId{FlightConfigurationId::defaultValueId()};
    const std::vector<FlightConfigurationId> first{design.id(0)};
    const std::vector<Uuid>                  body{design.body()};
    const std::vector<Uuid>                  chute{design.chute()};
    // Java: IllegalStateException("Attempting to set a motor on the default FCID.").
    EXPECT_THROW(static_cast<void>(design.editing().selectMotor(defaultId, body)), BugError);
    EXPECT_THROW(static_cast<void>(design.editing().selectIgnition(first, chute)), BugError);
    EXPECT_THROW(static_cast<void>(design.editing().selectDeployment(first, body)), BugError);
    EXPECT_THROW(static_cast<void>(design.editing().resetSeparation(first, body)), BugError);
    EXPECT_THROW(static_cast<void>(design.editing().setMotorMount(design.chute(), true)), BugError);
    EXPECT_FALSE(design.document().isUndoAvailable());
}

// ------------------------------------------------------------------- the requests (bugs 1, 2)

TEST_F(FlightConfigurationEditingTest, TheIgnitionQuestionStartsOnTheCellsOverride)
{
    Design             design;
    const Rocket&      rocket      = design.rocket();
    const Preferences& preferences = design.services().services().preferences();
    // A configuration of its own whose ignition is not overridden (a new MotorConfiguration's):
    // "all configurations", where OpenRocket starts on "this configuration" for a cell with a motor
    // (bug 2).
    UserPrompts::IgnitionRequest request = FlightConfigurationEditing::ignitionRequest(
        rocket, design.mount(design.body()), design.id(0), preferences);
    EXPECT_EQ(request.initial.scope, Scope::ALL_CONFIGURATIONS);
    EXPECT_EQ(request.configurationName, QStringLiteral("[A8-3, C6-5; C6-0]"));
    design.motors(design.body()).get(design.id(0)).setIgnitionEvent(IgnitionEvent::BURNOUT);
    request = FlightConfigurationEditing::ignitionRequest(rocket, design.mount(design.body()),
                                                          design.id(0), preferences);
    EXPECT_EQ(request.initial.scope, Scope::THIS_CONFIGURATION);
    EXPECT_EQ(request.initial.event, IgnitionEvent::BURNOUT);
    // No configuration of its own: the mount's default, also when it is overridden.
    design.mount(design.body()).getDefaultMotorConfig().setIgnitionDelay(2);
    request = FlightConfigurationEditing::ignitionRequest(rocket, design.mount(design.body()),
                                                          design.id(2), preferences);
    EXPECT_EQ(request.initial.scope, Scope::ALL_CONFIGURATIONS);
    EXPECT_EQ(request.initial.event, IgnitionEvent::AUTOMATIC);
    EXPECT_EQ(request.initial.delay, 2.0);
    EXPECT_EQ(request.configurationName, QStringLiteral("[No motors]"));
}

TEST_F(FlightConfigurationEditingTest, TheDeploymentAndSeparationQuestionsStartOnTheOverride)
{
    Design             design;
    const Rocket&      rocket      = design.rocket();
    const Preferences& preferences = design.services().services().preferences();
    const auto& device = dynamic_cast<const RecoveryDevice&>(*rocket.findComponent(design.chute()));
    const auto& stage  = dynamic_cast<const AxialStage&>(*rocket.findComponent(design.booster()));
    // Bug 1: OpenRocket always starts on "this configuration".
    EXPECT_EQ(
        FlightConfigurationEditing::deploymentRequest(rocket, device, design.id(1), preferences)
            .initial.scope,
        Scope::ALL_CONFIGURATIONS);
    EXPECT_EQ(
        FlightConfigurationEditing::separationRequest(rocket, stage, design.id(1), preferences)
            .initial.scope,
        Scope::ALL_CONFIGURATIONS);
    DeploymentConfiguration apogee;
    apogee.setDeployEvent(DeployEvent::APOGEE);
    design.deployments().set(design.id(1), apogee);
    StageSeparationConfiguration launch;
    launch.setSeparationEvent(SeparationEvent::LAUNCH);
    design.separations().set(design.id(1), launch);
    const UserPrompts::DeploymentRequest deploymentRequest =
        FlightConfigurationEditing::deploymentRequest(rocket, device, design.id(1), preferences);
    EXPECT_EQ(deploymentRequest.initial.scope, Scope::THIS_CONFIGURATION);
    EXPECT_EQ(deploymentRequest.initial.deployment.getDeployEvent(), DeployEvent::APOGEE);
    // The parachute is in the sustainer, above the booster.
    EXPECT_FALSE(deploymentRequest.bottomStage);
    const UserPrompts::SeparationRequest separationRequest =
        FlightConfigurationEditing::separationRequest(rocket, stage, design.id(1), preferences);
    EXPECT_EQ(separationRequest.initial.scope, Scope::THIS_CONFIGURATION);
    EXPECT_EQ(separationRequest.initial.separation.getSeparationEvent(), SeparationEvent::LAUNCH);
    EXPECT_EQ(separationRequest.configurationName, QStringLiteral("[C6-5; None]"));
}

// ------------------------------------------------------------------- the motors

TEST_F(FlightConfigurationEditingTest, SelectMotorWritesEveryCellThatDiffers)
{
    Design                                   design;
    const std::vector<FlightConfigurationId> rows{design.id(0), design.id(1), design.id(2)};
    const std::vector<Uuid>                  columns{design.body(), design.inner()};
    design.mount(design.body()).getDefaultMotorConfig().setIgnitionEvent(IgnitionEvent::BURNOUT);
    const std::shared_ptr<const ThrustCurveMotor> c6 = design.estes("C6");
    design.prompts().answerMotor(
        UserPrompts::MotorChoice{.motor = c6, .delay = 5, .nozzleExitDiameter = 0.01});
    const EventLog events(design.rocket());
    EXPECT_TRUE(design.editing().selectMotor(rows, columns));
    ASSERT_EQ(design.prompts().motorQuestions().size(), 1U);
    // The chooser is asked about the first cell.
    EXPECT_EQ(design.prompts().motorQuestions().front().mountId, design.body());
    EXPECT_EQ(design.prompts().motorQuestions().front().configurationId, design.id(0));
    for (const FlightConfigurationId& id : rows)
    {
        for (const Uuid& mount : columns)
        {
            const MotorConfiguration& c = design.motors(mount).get(id);
            EXPECT_EQ(c.getMotor(), c6);
            EXPECT_EQ(c.getEjectionDelay(), 5.0);
            EXPECT_EQ(c.getNozzleExitDiameter(), 0.01);
            EXPECT_EQ(c.getFcid(), id);
        }
    }
    // A new configuration takes its ignition from the one it replaces: the mount's default.
    EXPECT_EQ(design.motors(design.body()).get(design.id(2)).getIgnitionEvent(),
              IgnitionEvent::BURNOUT);
    EXPECT_EQ(events.types(), std::vector<int>{ComponentChangeEvent::kMotorChange});
    // Again: every cell has the motor already.
    design.prompts().answerMotor(
        UserPrompts::MotorChoice{.motor = c6, .delay = 5, .nozzleExitDiameter = 0.01});
    EXPECT_FALSE(design.editing().selectMotor(rows, columns));
    EXPECT_EQ(events.types().size(), 1U);
}

TEST_F(FlightConfigurationEditingTest, SelectMotorCancelledChangesNothing)
{
    Design                                   design;
    const std::vector<FlightConfigurationId> rows{design.id(2)};
    const std::vector<Uuid>                  columns{design.body()};
    design.prompts().answerMotor(std::nullopt);
    const EventLog events(design.rocket());
    EXPECT_FALSE(design.editing().selectMotor(rows, columns));
    EXPECT_FALSE(design.motors(design.body()).containsId(design.id(2)));
    EXPECT_TRUE(events.types().empty());
}

TEST_F(FlightConfigurationEditingTest, DeleteMotorsRemovesEveryCell)
{
    Design                                   design;
    const std::vector<FlightConfigurationId> rows{design.id(0), design.id(1)};
    const std::vector<Uuid>                  columns{design.body(), design.inner()};
    const EventLog                           events(design.rocket());
    EXPECT_TRUE(design.editing().deleteMotors(rows, columns));
    for (const FlightConfigurationId& id : rows)
    {
        EXPECT_FALSE(design.motors(design.body()).containsId(id));
        EXPECT_FALSE(design.motors(design.inner()).containsId(id));
    }
    EXPECT_TRUE(design.motors(design.boosterTube()).containsId(design.id(0)));
    EXPECT_FALSE(design.rocket().hasMotors(design.id(1)));
    EXPECT_EQ(events.types(), std::vector<int>{ComponentChangeEvent::kMotorChange});
}

// ------------------------------------------------------------------- the ignition (bugs 3, 4)

TEST_F(FlightConfigurationEditingTest, IgnitionCancelChangesNothing)
{
    Design                                   design;
    const std::vector<FlightConfigurationId> rows{design.id(0), design.id(1), design.id(2)};
    const std::vector<Uuid>                  columns{design.body()};
    design.prompts().answerIgnition(std::nullopt);
    const EventLog events(design.rocket());
    EXPECT_FALSE(design.editing().selectIgnition(rows, columns));
    // Bug 3: OpenRocket's Cancel marks the first cell's ignition overridden, and its loop then
    // copies the values into the other cells (making a configuration of the third's mount default
    // dirty too).
    EXPECT_FALSE(design.motors(design.body()).get(design.id(0)).hasIgnitionOverride());
    EXPECT_FALSE(design.motors(design.body()).get(design.id(1)).hasIgnitionOverride());
    EXPECT_FALSE(design.motors(design.body()).containsId(design.id(2)));
    EXPECT_FALSE(design.mount(design.body()).getDefaultMotorConfig().hasIgnitionOverride());
    EXPECT_TRUE(events.types().empty());
    // The undo position was added (as Java's, before the question).
    EXPECT_EQ(design.document().getUndoDetail().nextDescription, "Select ignition");
}

TEST_F(FlightConfigurationEditingTest, IgnitionForThisConfigurationGivesACellItsOwnConfiguration)
{
    Design                                   design;
    const std::vector<FlightConfigurationId> rows{design.id(2)};
    const std::vector<Uuid>                  columns{design.body()};
    design.prompts().answerIgnition(
        ignition(Scope::THIS_CONFIGURATION, IgnitionEvent::EJECTION_CHARGE, 1.5));
    const std::vector<ModId> before = design.modIds();
    EXPECT_TRUE(design.editing().selectIgnition(rows, columns));
    // Bug 4: OpenRocket's dialog edits the mount's default configuration for a cell without one of
    // its own.
    ASSERT_TRUE(design.motors(design.body()).containsId(design.id(2)));
    const MotorConfiguration& own = design.motors(design.body()).get(design.id(2));
    EXPECT_EQ(own.getIgnitionEvent(), IgnitionEvent::EJECTION_CHARGE);
    EXPECT_EQ(own.getIgnitionDelay(), 1.5);
    EXPECT_TRUE(own.hasIgnitionOverride());
    EXPECT_EQ(own.getFcid(), design.id(2));
    EXPECT_TRUE(own.isEmpty());
    const MotorConfiguration& defaultConfiguration =
        design.mount(design.body()).getDefaultMotorConfig();
    EXPECT_EQ(defaultConfiguration.getIgnitionEvent(), IgnitionEvent::AUTOMATIC);
    EXPECT_FALSE(defaultConfiguration.hasIgnitionOverride());
    // One event for the configuration it changed.
    const std::vector<ModId> after = design.modIds();
    EXPECT_EQ(after[0], before[0]);
    EXPECT_EQ(after[1], before[1]);
    EXPECT_NE(after[2], before[2]);
}

TEST_F(FlightConfigurationEditingTest, IgnitionForThisConfigurationReachesTheOtherCells)
{
    Design                                   design;
    const std::vector<FlightConfigurationId> rows{design.id(1), design.id(2)};
    const std::vector<Uuid>                  columns{design.body(), design.boosterTube()};
    design.prompts().answerIgnition(
        ignition(Scope::THIS_CONFIGURATION, IgnitionEvent::BURNOUT, 0.5));
    EXPECT_TRUE(design.editing().selectIgnition(rows, columns));
    for (const Uuid& mount : columns)
    {
        for (const FlightConfigurationId& id : rows)
        {
            ASSERT_TRUE(design.motors(mount).containsId(id));
            EXPECT_EQ(design.motors(mount).get(id).getIgnitionEvent(), IgnitionEvent::BURNOUT);
            EXPECT_EQ(design.motors(mount).get(id).getIgnitionDelay(), 0.5);
        }
        // Bug 4, the other cells: OpenRocket's loop writes the values into the default of a mount
        // whose cell has no configuration of its own.
        EXPECT_EQ(design.mount(mount).getDefaultMotorConfig().getIgnitionEvent(),
                  IgnitionEvent::AUTOMATIC);
    }
    // The motors stay where they were.
    EXPECT_NE(design.motors(design.body()).get(design.id(1)).getMotor(), nullptr);
    EXPECT_EQ(design.motors(design.boosterTube()).get(design.id(1)).getMotor(), nullptr);
}

TEST_F(FlightConfigurationEditingTest, IgnitionForAllConfigurationsSetsTheDefaultAndEveryOne)
{
    Design                                   design;
    const std::vector<FlightConfigurationId> rows{design.id(0)};
    const std::vector<Uuid>                  columns{design.body()};
    design.motors(design.body()).get(design.id(1)).setIgnitionEvent(IgnitionEvent::NEVER);
    design.prompts().answerIgnition(ignition(Scope::ALL_CONFIGURATIONS, IgnitionEvent::LAUNCH, 2));
    const std::vector<ModId> before = design.modIds();
    EXPECT_TRUE(design.editing().selectIgnition(rows, columns));
    const MotorConfiguration& defaultConfiguration =
        design.mount(design.body()).getDefaultMotorConfig();
    EXPECT_EQ(defaultConfiguration.getIgnitionEvent(), IgnitionEvent::LAUNCH);
    EXPECT_EQ(defaultConfiguration.getIgnitionDelay(), 2.0);
    // The overridden one too, as in OpenRocket.
    EXPECT_EQ(design.motors(design.body()).get(design.id(1)).getIgnitionEvent(),
              IgnitionEvent::LAUNCH);
    // The other mounts keep theirs.
    EXPECT_EQ(design.motors(design.inner()).get(design.id(0)).getIgnitionEvent(),
              IgnitionEvent::AUTOMATIC);
    // The event names every configuration: their copies of the motors are refreshed.
    const std::vector<ModId> after = design.modIds();
    for (std::size_t i = 0; i < after.size(); ++i)
    {
        EXPECT_NE(after[i], before[i]) << i;
    }
    EXPECT_EQ(copiedIgnition(design.rocket(), design.id(1), design.mount(design.body())),
              IgnitionEvent::LAUNCH);
}

TEST_F(FlightConfigurationEditingTest, IgnitionLeftAsItWasChangesNothing)
{
    Design                                   design;
    const std::vector<FlightConfigurationId> rows{design.id(2)};
    const std::vector<Uuid>                  columns{design.body()};
    design.prompts().answerIgnition(
        ignition(Scope::THIS_CONFIGURATION, IgnitionEvent::AUTOMATIC, 0));
    const EventLog events(design.rocket());
    EXPECT_FALSE(design.editing().selectIgnition(rows, columns));
    EXPECT_FALSE(design.motors(design.body()).containsId(design.id(2)));
    EXPECT_TRUE(events.types().empty());
}

TEST_F(FlightConfigurationEditingTest, ResetIgnitionLeavesCellsWithoutAConfigurationAlone)
{
    Design design;
    design.mount(design.body()).getDefaultMotorConfig().setIgnitionEvent(IgnitionEvent::BURNOUT);
    design.motors(design.body()).get(design.id(0)).setIgnitionDelay(3);
    const std::vector<FlightConfigurationId> rows{design.id(0), design.id(2)};
    const std::vector<Uuid>                  columns{design.body()};
    EXPECT_TRUE(design.editing().resetIgnition(rows, columns));
    const MotorConfiguration& own = design.motors(design.body()).get(design.id(0));
    EXPECT_EQ(own.getIgnitionEvent(), IgnitionEvent::AUTOMATIC);
    EXPECT_EQ(own.getIgnitionDelay(), 0.0);
    // useDefaultIgnition() sets the values through the setters, as Java's.
    EXPECT_TRUE(own.hasIgnitionOverride());
    // Bug 4: OpenRocket resets the mount's default for the cell without a configuration.
    EXPECT_FALSE(design.motors(design.body()).containsId(design.id(2)));
    EXPECT_EQ(design.mount(design.body()).getDefaultMotorConfig().getIgnitionEvent(),
              IgnitionEvent::BURNOUT);
    // Nothing more to reset.
    EXPECT_FALSE(design.editing().resetIgnition(rows, columns));
}

// ------------------------------------------------------------------- recovery and stages

TEST_F(FlightConfigurationEditingTest, DeploymentForThisConfigurationReachesTheOtherCells)
{
    Design                                   design;
    const std::vector<FlightConfigurationId> rows{design.id(0), design.id(1), design.id(2)};
    const std::vector<Uuid>                  devices{design.chute()};
    design.prompts().answerDeployment(
        deployment(Scope::THIS_CONFIGURATION, DeployEvent::ALTITUDE, 150, 1.5));
    const std::vector<ModId> before = design.modIds();
    const EventLog           events(design.rocket());
    EXPECT_TRUE(design.editing().selectDeployment(rows, devices));
    for (const FlightConfigurationId& id : rows)
    {
        ASSERT_TRUE(design.deployments().containsId(id));
        EXPECT_EQ(design.deployments().get(id).getDeployEvent(), DeployEvent::ALTITUDE);
        EXPECT_EQ(design.deployments().get(id).getDeployAltitude(), 150.0);
        EXPECT_EQ(design.deployments().get(id).getDeployDelay(), 1.5);
    }
    EXPECT_EQ(design.deployments().getDefault().getDeployEvent(), DeployEvent::EJECTION);
    EXPECT_EQ(events.types(), std::vector<int>{ComponentChangeEvent::kAerodynamicChange});
    EXPECT_NE(design.modIds(), before);
    // Reset: the overrides go.
    EXPECT_TRUE(design.editing().resetDeployment(rows, devices));
    EXPECT_EQ(design.deployments().size(), 0U);
}

TEST_F(FlightConfigurationEditingTest, DeploymentForAllConfigurationsKeepsTheOverrides)
{
    Design                  design;
    DeploymentConfiguration never;
    never.setDeployEvent(DeployEvent::NEVER);
    design.deployments().set(design.id(1), never);
    design.prompts().answerDeployment(
        deployment(Scope::ALL_CONFIGURATIONS, DeployEvent::APOGEE, 200, 0));
    const std::vector<ModId> before = design.modIds();
    EXPECT_TRUE(design.editing().selectDeployment(std::vector<FlightConfigurationId>{design.id(0)},
                                                  std::vector<Uuid>{design.chute()}));
    EXPECT_EQ(design.deployments().getDefault().getDeployEvent(), DeployEvent::APOGEE);
    EXPECT_EQ(design.deployments().get(design.id(1)).getDeployEvent(), DeployEvent::NEVER);
    const std::vector<ModId> after = design.modIds();
    for (std::size_t i = 0; i < after.size(); ++i)
    {
        EXPECT_NE(after[i], before[i]) << i;
    }
}

TEST_F(FlightConfigurationEditingTest, DeploymentCancelChangesNothing)
{
    Design design;
    design.prompts().answerDeployment(std::nullopt);
    EXPECT_FALSE(design.editing().selectDeployment(
        std::vector<FlightConfigurationId>{design.id(0), design.id(1)},
        std::vector<Uuid>{design.chute()}));
    EXPECT_EQ(design.deployments().size(), 0U);
}

TEST_F(FlightConfigurationEditingTest, SeparationCancelKeepsAnOverrideAsItWas)
{
    Design                       design;
    StageSeparationConfiguration burnout;
    burnout.setSeparationEvent(SeparationEvent::BURNOUT);
    design.separations().set(design.id(0), burnout);
    design.prompts().answerSeparation(std::nullopt);
    EXPECT_FALSE(design.editing().selectSeparation(std::vector<FlightConfigurationId>{design.id(0)},
                                                   std::vector<Uuid>{design.booster()}));
    // Bug 3: OpenRocket's dialog edits the override in place, and Cancel keeps the edits.
    EXPECT_EQ(design.separations().get(design.id(0)).getSeparationEvent(),
              SeparationEvent::BURNOUT);
    EXPECT_EQ(design.separations().size(), 1U);
}

TEST_F(FlightConfigurationEditingTest, SeparationForAllConfigurationsRemovesEveryOverride)
{
    Design                       design;
    StageSeparationConfiguration launch;
    launch.setSeparationEvent(SeparationEvent::LAUNCH);
    design.separations().set(design.id(1), launch);
    design.separations().set(design.id(2), launch);
    design.prompts().answerSeparation(
        separation(Scope::ALL_CONFIGURATIONS, SeparationEvent::BURNOUT, 200, 1));
    const EventLog events(design.rocket());
    EXPECT_TRUE(design.editing().selectSeparation(std::vector<FlightConfigurationId>{design.id(0)},
                                                  std::vector<Uuid>{design.booster()}));
    EXPECT_EQ(design.separations().size(), 0U);
    EXPECT_EQ(design.separations().getDefault().getSeparationEvent(), SeparationEvent::BURNOUT);
    EXPECT_EQ(design.separations().getDefault().getSeparationDelay(), 1.0);
    EXPECT_EQ(events.types(), std::vector<int>{ComponentChangeEvent::kAeromassChange});
}

TEST_F(FlightConfigurationEditingTest, SeparationReachesTheOtherCellsWithoutItsAltitude)
{
    Design design;
    design.prompts().answerSeparation(
        separation(Scope::THIS_CONFIGURATION, SeparationEvent::ALTITUDE_ASCENDING, 300, 0.5));
    EXPECT_TRUE(design.editing().selectSeparation(
        std::vector<FlightConfigurationId>{design.id(0), design.id(1)},
        std::vector<Uuid>{design.booster()}));
    const StageSeparationConfiguration& first  = design.separations().get(design.id(0));
    const StageSeparationConfiguration& second = design.separations().get(design.id(1));
    EXPECT_EQ(first.getSeparationAltitude(), 300.0);
    EXPECT_EQ(second.getSeparationEvent(), SeparationEvent::ALTITUDE_ASCENDING);
    EXPECT_EQ(second.getSeparationDelay(), 0.5);
    // As OpenRocket's loop: the event and the delay only.
    EXPECT_EQ(second.getSeparationAltitude(),
              design.separations().getDefault().getSeparationAltitude());
    EXPECT_TRUE(design.editing().resetSeparation(
        std::vector<FlightConfigurationId>{design.id(0), design.id(1)},
        std::vector<Uuid>{design.booster()}));
    EXPECT_EQ(design.separations().size(), 0U);
}

TEST_F(FlightConfigurationEditingTest, AnAltitudeOnlyChangeIsAChange)
{
    Design                       design;
    StageSeparationConfiguration ascending;
    ascending.setSeparationEvent(SeparationEvent::ALTITUDE_ASCENDING);
    ascending.setSeparationAltitude(300);
    design.separations().set(design.id(0), ascending);
    design.document().clearUndo();
    design.prompts().answerSeparation(
        separation(Scope::THIS_CONFIGURATION, SeparationEvent::ALTITUDE_ASCENDING, 400, 0));
    const EventLog events(design.rocket());
    // OpenRocket's equals() leaves the altitude out, so its operation fires no event for this
    // change (and leaves the undo history without it).
    EXPECT_TRUE(design.editing().selectSeparation(std::vector<FlightConfigurationId>{design.id(0)},
                                                  std::vector<Uuid>{design.booster()}));
    EXPECT_EQ(design.separations().get(design.id(0)).getSeparationAltitude(), 400.0);
    EXPECT_EQ(events.types(), std::vector<int>{ComponentChangeEvent::kAeromassChange});
    EXPECT_EQ(design.document().getUndoDescription(), "Select separation");
}

// ------------------------------------------------------------------- the configurations

TEST_F(FlightConfigurationEditingTest, NewConfigurationAddsASimulationAndAsksTheFollowUp)
{
    Design                      design;
    const EventLog              events(design.rocket());
    FollowUp                    followUp(events);
    const FlightConfigurationId id =
        required(design.editing().newConfiguration(std::ref(followUp)));
    EXPECT_EQ(followUp.id(), id);
    EXPECT_EQ(design.rocket().getConfigurationCount(), 4);
    EXPECT_EQ(design.rocket().getSelectedConfiguration().getId(), id);
    EXPECT_FALSE(design.rocket().getFlightConfiguration(id).getStoredName().has_value());
    ASSERT_EQ(design.document().getSimulationCount(), 1U);
    EXPECT_EQ(design.document().getSimulation(0)->getName(), "Simulation 1");
    EXPECT_EQ(design.document().getSimulation(0)->getFlightConfigurationId(), id);
    // Stored and selected (the core's), then the operation's MOTOR_CHANGE; after the follow-up
    // Java's "trigger select".
    EXPECT_EQ(followUp.eventsBefore(), (std::vector<int>{ComponentChangeEvent::kNonFunctionalChange,
                                                         ComponentChangeEvent::kNonFunctionalChange,
                                                         ComponentChangeEvent::kMotorChange}));
    EXPECT_EQ(events.types().size(), 4U);
    EXPECT_EQ(events.types().back(), ComponentChangeEvent::kNonFunctionalChange);
}

TEST_F(FlightConfigurationEditingTest, DuplicateCopiesEveryComponentsValues)
{
    Design                  design;
    DeploymentConfiguration apogee;
    apogee.setDeployEvent(DeployEvent::APOGEE);
    design.deployments().set(design.id(0), apogee);
    // A mount that stopped acting keeps its configurations, and they are copied too.
    design.mount(design.inner()).setMotorMount(false);
    const std::vector<FlightConfigurationId> copies = design.editing().duplicateConfigurations(
        std::vector<FlightConfigurationId>{design.id(0), design.id(1)});
    ASSERT_EQ(copies.size(), 2U);
    EXPECT_EQ(design.rocket().getConfigurationCount(), 5);
    EXPECT_EQ(design.rocket().getSelectedConfiguration().getId(), copies[0]);
    EXPECT_EQ(design.motors(design.body()).get(copies[0]).getMotor(),
              design.motors(design.body()).get(design.id(0)).getMotor());
    EXPECT_EQ(design.motors(design.body()).get(copies[0]).getEjectionDelay(), 5.0);
    EXPECT_TRUE(design.motors(design.inner()).containsId(copies[0]));
    EXPECT_EQ(design.deployments().get(copies[0]).getDeployEvent(), DeployEvent::APOGEE);
    // An id without a value of its own gets the default's as an override.
    EXPECT_TRUE(design.deployments().containsId(copies[1]));
    EXPECT_EQ(design.document().getSimulationCount(), 2U);
    EXPECT_EQ(design.document().getSimulation(1)->getFlightConfigurationId(), copies[1]);
    // The name of the copy (the inner tube acts as a mount no more).
    EXPECT_EQ(design.rocket().getFlightConfiguration(copies[0]).getName(
                  design.services().services().preferences()),
              "[C6-5; C6-0]");
}

TEST_F(FlightConfigurationEditingTest, RenameGivesTheOtherConfigurationsTheSameName)
{
    Design             design;
    const Preferences& preferences = design.services().services().preferences();
    const std::vector<FlightConfigurationId> rows{design.id(0), design.id(1), design.id(2)};
    design.prompts().answerRenameConfiguration(
        {.action = UserPrompts::RenameAction::RENAME, .name = QStringLiteral("Flight {motors}")});
    const EventLog events(design.rocket());
    EXPECT_TRUE(design.editing().renameConfigurations(rows));
    for (const FlightConfigurationId& id : rows)
    {
        EXPECT_EQ(design.rocket().getFlightConfiguration(id).getNameRaw(preferences),
                  "Flight {motors}");
    }
    EXPECT_EQ(design.rocket().getFlightConfiguration(design.id(1)).getName(preferences),
              "Flight C6-5; None");
    EXPECT_EQ(events.types(), std::vector<int>{ComponentChangeEvent::kNonFunctionalChange});
    // The question shows the first one's raw name.
    ASSERT_FALSE(design.prompts().prompts().empty());
    EXPECT_EQ(design.prompts().prompts().back().texts, QStringList{QStringLiteral("[{motors}]")});
    // Reset to default: the default name again (setName(null)), the others follow.
    design.prompts().answerRenameConfiguration(
        {.action = UserPrompts::RenameAction::RESET, .name = {}});
    EXPECT_TRUE(design.editing().renameConfigurations(rows));
    EXPECT_FALSE(design.rocket().getFlightConfiguration(design.id(0)).getStoredName().has_value());
    EXPECT_EQ(design.rocket().getFlightConfiguration(design.id(2)).getNameRaw(preferences),
              "[{motors}]");
    // Cancel: the undo position, no change, no event.
    design.prompts().answerRenameConfiguration(
        {.action = UserPrompts::RenameAction::CANCEL, .name = {}});
    EXPECT_FALSE(design.editing().renameConfigurations(rows));
    EXPECT_EQ(events.types().size(), 2U);
}

TEST_F(FlightConfigurationEditingTest, SaveAsDefaultKeepsTheNamesOfEveryOpenDesign)
{
    Design                              design;
    const Preferences&                  preferences = design.services().services().preferences();
    std::unique_ptr<OpenRocketDocument> other       = OpenRocketDocumentFactory::createNewRocket();
    const FlightConfigurationId otherId = other->getRocket().createFlightConfiguration().getId();
    design.editing().setOpenDocuments(documentsOf({other.get()}));
    EXPECT_EQ(design.editing().openDocuments(),
              (std::vector<OpenRocketDocument*>{other.get(), &design.document()}));
    design.prompts().answerRenameConfiguration(
        {.action = UserPrompts::RenameAction::CANCEL, .name = {}},
        {QStringLiteral("Test {motors}")});
    EXPECT_FALSE(
        design.editing().renameConfigurations(std::vector<FlightConfigurationId>{design.id(0)}));
    EXPECT_EQ(preferences.getDefaultFlightConfigName(), "Test {motors}");
    // Java's message, over the dialog.
    const Gui::Test::ScriptedUserPrompts::Prompt& message = design.prompts().prompts().back();
    EXPECT_EQ(message.kind, Gui::Test::ScriptedUserPrompts::Kind::MESSAGE);
    EXPECT_EQ(message.title, QStringLiteral("Save as default"));
    EXPECT_EQ(
        message.texts,
        (QStringList{QStringLiteral("INFORMATION"),
                     QStringLiteral("This change will only affect new flight configurations.")}));
    // The existing configurations of both designs keep their name; a new one takes the new default.
    EXPECT_EQ(design.rocket().getFlightConfiguration(design.id(1)).getNameRaw(preferences),
              "[{motors}]");
    EXPECT_EQ(other->getRocket().getFlightConfiguration(otherId).getNameRaw(preferences),
              "[{motors}]");
    EXPECT_TRUE(design.rocket().getFlightConfiguration(design.id(1)).isNameOverridden(preferences));
    const FlightConfigurationId added = required(design.editing().newConfiguration());
    EXPECT_EQ(design.rocket().getFlightConfiguration(added).getName(preferences), "Test No motors");
}

TEST_F(FlightConfigurationEditingTest, RemoveTakesTheSimulationsAlong)
{
    Design                      design;
    const FlightConfigurationId added = required(design.editing().newConfiguration());
    ASSERT_EQ(design.document().getSimulationCount(), 1U);
    const EventLog events(design.rocket());
    EXPECT_TRUE(design.editing().removeConfigurations(
        std::vector<FlightConfigurationId>{added, design.id(2)}));
    EXPECT_EQ(design.rocket().getConfigurationCount(), 2);
    EXPECT_EQ(design.document().getSimulationCount(), 0U);
    EXPECT_TRUE(design.rocket().getSelectedConfiguration().getId().isDefaultId());
    // The core's events of each removal, then the operation's.
    EXPECT_EQ(events.types().back(), ComponentChangeEvent::kNonFunctionalChange);
    design.document().undo();
    EXPECT_EQ(design.rocket().getConfigurationCount(), 4);
    EXPECT_EQ(design.document().getSimulationCount(), 1U);
}

TEST_F(FlightConfigurationEditingTest, TheMotorMountCheckBoxIsUndoneAlone)
{
    Design design;
    EXPECT_TRUE(design.editing().setMotorMount(design.inner(), false));
    EXPECT_FALSE(design.mount(design.inner()).isMotorMount());
    // Bug 5: OpenRocket's toggle has no undo step of its own.
    design.document().undo();
    EXPECT_TRUE(design.mount(design.inner()).isMotorMount());
    EXPECT_FALSE(design.document().isUndoAvailable());
    EXPECT_EQ(design.document().getRedoDescription(), "Modify motor mount");
}

// ------------------------------------------------------------------- lifetimes

/// Prompts that undo the design's last step while a value question is open (as a modal dialog's
/// event loop could deliver a TREE or UNDO change), replacing every component of the rocket.
class UndoingPrompts final : public Gui::Test::ForwardingUserPrompts
{
public:
    UndoingPrompts(UserPrompts& delegate, OpenRocketDocument& document)
      : ForwardingUserPrompts(delegate), m_document(&document)
    {
    }

    [[nodiscard]] std::optional<IgnitionChoice> askIgnition(QWidget*               parent,
                                                            const IgnitionRequest& request) override
    {
        std::optional<IgnitionChoice> answer = ForwardingUserPrompts::askIgnition(parent, request);
        m_document->undo();
        return answer;
    }

    [[nodiscard]] std::optional<MotorChoice> chooseMotor(
        QWidget* parent, const GuiServices& services, const MotorMount& mount,
        const FlightConfigurationId& configurationId) override
    {
        // After the chooser has read the mount: the undo destroys it.
        std::optional<MotorChoice> answer =
            ForwardingUserPrompts::chooseMotor(parent, services, mount, configurationId);
        m_document->undo();
        return answer;
    }

private:
    OpenRocketDocument* m_document;
};

TEST_F(FlightConfigurationEditingTest, ComponentsReplacedDuringTheQuestionAreFoundAgain)
{
    Design design;
    // A step to undo (the inner tube's mount check), then the operations over the undo.
    ASSERT_TRUE(design.editing().setMotorMount(design.inner(), false));
    UndoingPrompts    prompts(design.prompts(), design.document());
    const GuiServices services(design.services().preferences(),
                               design.services().services().materials(), design.services().motors(),
                               design.services().presets(),
                               design.services().services().loadingContext(), prompts);
    FlightConfigurationEditing editing(design.document(), services);
    design.prompts().answerIgnition(ignition(Scope::THIS_CONFIGURATION, IgnitionEvent::NEVER, 0));
    EXPECT_TRUE(editing.selectIgnition(std::vector<FlightConfigurationId>{design.id(2)},
                                       std::vector<Uuid>{design.inner()}));
    // The undone rocket's inner tube (a new object of the same id) took the ignition.
    EXPECT_TRUE(design.mount(design.inner()).isMotorMount());
    EXPECT_EQ(design.motors(design.inner()).get(design.id(2)).getIgnitionEvent(),
              IgnitionEvent::NEVER);
    ASSERT_TRUE(design.editing().setMotorMount(design.inner(), false));
    design.prompts().answerMotor(
        UserPrompts::MotorChoice{.motor = design.estes("A8"), .delay = 3, .nozzleExitDiameter = 0});
    EXPECT_TRUE(editing.selectMotor(std::vector<FlightConfigurationId>{design.id(1)},
                                    std::vector<Uuid>{design.inner()}));
    EXPECT_EQ(design.motors(design.inner()).get(design.id(1)).getMotor(), design.estes("A8"));
}

// ================================================================= the review's fixes (run 2)

TEST_F(FlightConfigurationEditingTest, AnUnchangedIgnitionOfACellShowingTheDefaultKeepsTheDefault)
{
    Design             design;
    const Preferences& preferences = design.services().services().preferences();
    // The cell (body, first configuration) has a configuration of its own whose ignition is not
    // overridden (as Select motor makes it); the mount's default is Burnout + 1 s, which the
    // cell shows ("Default (Burnout + 1 s)").
    const MotorConfiguration& own = design.motors(design.body()).get(design.id(0));
    ASSERT_FALSE(own.hasIgnitionOverride());
    ASSERT_EQ(own.getIgnitionEvent(), IgnitionEvent::AUTOMATIC);
    design.mount(design.body()).getDefaultMotorConfig().setIgnitionEvent(IgnitionEvent::BURNOUT);
    design.mount(design.body()).getDefaultMotorConfig().setIgnitionDelay(1);
    // The question starts on "all configurations" (bug 2) with the values the cell shows.
    const UserPrompts::IgnitionRequest request = FlightConfigurationEditing::ignitionRequest(
        design.rocket(), design.mount(design.body()), design.id(0), preferences);
    EXPECT_EQ(request.initial.scope, Scope::ALL_CONFIGURATIONS);
    EXPECT_EQ(request.initial.event, IgnitionEvent::BURNOUT);
    EXPECT_EQ(request.initial.delay, 1.0);
    // OK (or Escape) without a change: the default stays Burnout + 1 s (the M3 run 2 verifier's
    // probe: OpenRocket's dialog starts on "this configuration" there, and its unchanged OK sets
    // nothing); "all configurations" gives every configuration of the mount the values, as
    // OpenRocket's.
    design.prompts().answerIgnition(request.initial);
    EXPECT_TRUE(design.editing().selectIgnition(std::vector<FlightConfigurationId>{design.id(0)},
                                                std::vector<Uuid>{design.body()}));
    const MotorConfiguration& defaultConfiguration =
        design.mount(design.body()).getDefaultMotorConfig();
    EXPECT_EQ(defaultConfiguration.getIgnitionEvent(), IgnitionEvent::BURNOUT);
    EXPECT_EQ(defaultConfiguration.getIgnitionDelay(), 1.0);
    EXPECT_EQ(design.motors(design.body()).get(design.id(0)).getIgnitionEvent(),
              IgnitionEvent::BURNOUT);
    EXPECT_EQ(design.motors(design.body()).get(design.id(0)).getIgnitionDelay(), 1.0);
}

TEST_F(FlightConfigurationEditingTest, ThisConfigurationGivesACellShowingTheDefaultTheValuesShown)
{
    Design design;
    design.mount(design.body()).getDefaultMotorConfig().setIgnitionEvent(IgnitionEvent::BURNOUT);
    design.mount(design.body()).getDefaultMotorConfig().setIgnitionDelay(1);
    // "This configuration" with the values shown left as they were: the cell's own configuration
    // takes them (each differs from its stored AUTOMATIC without delay), overridden; the default
    // stays.
    design.prompts().answerIgnition(ignition(Scope::THIS_CONFIGURATION, IgnitionEvent::BURNOUT, 1));
    EXPECT_TRUE(design.editing().selectIgnition(std::vector<FlightConfigurationId>{design.id(0)},
                                                std::vector<Uuid>{design.body()}));
    const MotorConfiguration& own = design.motors(design.body()).get(design.id(0));
    EXPECT_EQ(own.getIgnitionEvent(), IgnitionEvent::BURNOUT);
    EXPECT_EQ(own.getIgnitionDelay(), 1.0);
    EXPECT_TRUE(own.hasIgnitionOverride());
    EXPECT_NE(own.getMotor(), nullptr);
    const MotorConfiguration& defaultConfiguration =
        design.mount(design.body()).getDefaultMotorConfig();
    EXPECT_EQ(defaultConfiguration.getIgnitionEvent(), IgnitionEvent::BURNOUT);
    EXPECT_EQ(defaultConfiguration.getIgnitionDelay(), 1.0);
    // The second configuration's own one is not touched.
    EXPECT_FALSE(design.motors(design.body()).get(design.id(1)).hasIgnitionOverride());
}

TEST_F(FlightConfigurationEditingTest, AMultiCellDeploymentCancelChangesNothing)
{
    Design design;
    // The first cell has an override of its own (Altitude 200 m); the others have none.
    DeploymentConfiguration altitude;
    altitude.setDeployEvent(DeployEvent::ALTITUDE);
    altitude.setDeployAltitude(200);
    design.deployments().set(design.id(0), altitude);
    design.prompts().answerDeployment(std::nullopt);
    const EventLog events(design.rocket());
    EXPECT_FALSE(design.editing().selectDeployment(
        std::vector<FlightConfigurationId>{design.id(0), design.id(1), design.id(2)},
        std::vector<Uuid>{design.chute()}));
    // Bug 3, taken to the deployment (to be confirmed by the user): OpenRocket's panel runs its
    // loop after a Cancel too, so that the second and third configurations get overrides of
    // Altitude 200 m (the M3 run 2 verifier's probe, "A simple model rocket.ork").
    EXPECT_EQ(design.deployments().size(), 1U);
    EXPECT_FALSE(design.deployments().containsId(design.id(1)));
    EXPECT_FALSE(design.deployments().containsId(design.id(2)));
    EXPECT_EQ(design.deployments().get(design.id(0)).getDeployAltitude(), 200.0);
    EXPECT_TRUE(events.types().empty());
}

/// Removes a configuration of the rocket during the next value question, as an undo of its New
/// Configuration in the question's event loop would.
class RemovingPrompts final : public Gui::Test::ForwardingUserPrompts
{
public:
    RemovingPrompts(UserPrompts& delegate, Rocket& rocket, const FlightConfigurationId& id)
      : ForwardingUserPrompts(delegate), m_rocket(&rocket), m_id(id)
    {
    }

    [[nodiscard]] std::optional<IgnitionChoice> askIgnition(QWidget*               parent,
                                                            const IgnitionRequest& request) override
    {
        std::optional<IgnitionChoice> answer = ForwardingUserPrompts::askIgnition(parent, request);
        m_rocket->removeFlightConfiguration(m_id);
        return answer;
    }

    [[nodiscard]] std::optional<DeploymentChoice> askDeployment(
        QWidget* parent, const DeploymentRequest& request) override
    {
        std::optional<DeploymentChoice> answer =
            ForwardingUserPrompts::askDeployment(parent, request);
        m_rocket->removeFlightConfiguration(m_id);
        return answer;
    }

private:
    Rocket*               m_rocket;
    FlightConfigurationId m_id;
};

TEST_F(FlightConfigurationEditingTest, AConfigurationRemovedDuringTheQuestionIsLeftOut)
{
    Design            design;
    RemovingPrompts   second(design.prompts(), design.rocket(), design.id(1));
    const GuiServices services(design.services().preferences(),
                               design.services().services().materials(), design.services().motors(),
                               design.services().presets(),
                               design.services().services().loadingContext(), second);
    FlightConfigurationEditing editing(design.document(), services);
    design.prompts().answerDeployment(
        deployment(Scope::THIS_CONFIGURATION, DeployEvent::ALTITUDE, 150, 0));
    EXPECT_TRUE(editing.selectDeployment(
        std::vector<FlightConfigurationId>{design.id(0), design.id(1), design.id(2)},
        std::vector<Uuid>{design.chute()}));
    // No override for the configuration that went (the savers would write it as a
    // <deploymentconfiguration> of a configuration the design does not have).
    EXPECT_TRUE(design.deployments().containsId(design.id(0)));
    EXPECT_FALSE(design.deployments().containsId(design.id(1)));
    EXPECT_TRUE(design.deployments().containsId(design.id(2)));
    EXPECT_EQ(design.deployments().size(), 2U);

    // The first cell's configuration goes: nothing is done.
    RemovingPrompts   first(design.prompts(), design.rocket(), design.id(0));
    const GuiServices firstServices(design.services().preferences(),
                                    design.services().services().materials(),
                                    design.services().motors(), design.services().presets(),
                                    design.services().services().loadingContext(), first);
    FlightConfigurationEditing firstEditing(design.document(), firstServices);
    design.prompts().answerIgnition(ignition(Scope::THIS_CONFIGURATION, IgnitionEvent::NEVER, 0));
    EXPECT_FALSE(
        firstEditing.selectIgnition(std::vector<FlightConfigurationId>{design.id(0), design.id(2)},
                                    std::vector<Uuid>{design.body()}));
    EXPECT_FALSE(design.motors(design.body()).containsId(design.id(2)));
    EXPECT_FALSE(design.rocket().containsFlightConfigurationId(design.id(0)));
}

/// Destroys the operations (with the tab that owns them) during the next question or message.
class DestroyingPrompts final : public Gui::Test::ForwardingUserPrompts
{
public:
    explicit DestroyingPrompts(UserPrompts& delegate) noexcept : ForwardingUserPrompts(delegate) { }

    /// @p editing goes during the next question.
    void destroyDuringTheNextQuestion(std::unique_ptr<FlightConfigurationEditing>& editing)
    {
        m_editing = &editing;
    }

    [[nodiscard]] std::optional<IgnitionChoice> askIgnition(QWidget*               parent,
                                                            const IgnitionRequest& request) override
    {
        std::optional<IgnitionChoice> answer = ForwardingUserPrompts::askIgnition(parent, request);
        destroy();
        return answer;
    }

    [[nodiscard]] std::optional<DeploymentChoice> askDeployment(
        QWidget* parent, const DeploymentRequest& request) override
    {
        std::optional<DeploymentChoice> answer =
            ForwardingUserPrompts::askDeployment(parent, request);
        destroy();
        return answer;
    }

    [[nodiscard]] std::optional<SeparationChoice> askSeparation(
        QWidget* parent, const SeparationRequest& request) override
    {
        std::optional<SeparationChoice> answer =
            ForwardingUserPrompts::askSeparation(parent, request);
        destroy();
        return answer;
    }

    [[nodiscard]] RenameConfigurationAnswer askRenameConfiguration(
        QWidget* parent, const RenameConfigurationRequest& request) override
    {
        const RenameConfigurationAnswer answer =
            ForwardingUserPrompts::askRenameConfiguration(parent, request);
        destroy();
        return answer;
    }

    [[nodiscard]] std::optional<MotorChoice> chooseMotor(
        QWidget* parent, const GuiServices& services, const MotorMount& mount,
        const FlightConfigurationId& configurationId) override
    {
        std::optional<MotorChoice> answer =
            ForwardingUserPrompts::chooseMotor(parent, services, mount, configurationId);
        destroy();
        return answer;
    }

    void showMessage(QWidget* parent, const Message& message) override
    {
        ForwardingUserPrompts::showMessage(parent, message);
        destroy();
    }

private:
    void destroy()
    {
        if (m_editing != nullptr)
        {
            m_editing->reset();
            m_editing = nullptr;
        }
    }

    std::unique_ptr<FlightConfigurationEditing>* m_editing = nullptr;
};

TEST_F(FlightConfigurationEditingTest, TheOperationsCanGoDuringTheirQuestions)
{
    Design            design;
    DestroyingPrompts prompts(design.prompts());
    const GuiServices services(design.services().preferences(),
                               design.services().services().materials(), design.services().motors(),
                               design.services().presets(),
                               design.services().services().loadingContext(), prompts);
    const std::vector<FlightConfigurationId>    rows{design.id(1), design.id(2)};
    std::unique_ptr<FlightConfigurationEditing> editing;

    // Each operation returns at once, its answer not applied (and touches nothing that went:
    // the asan preset's run).
    editing = std::make_unique<FlightConfigurationEditing>(design.document(), services);
    prompts.destroyDuringTheNextQuestion(editing);
    design.prompts().answerIgnition(ignition(Scope::THIS_CONFIGURATION, IgnitionEvent::NEVER, 0));
    EXPECT_FALSE(editing->selectIgnition(rows, std::vector<Uuid>{design.body()}));
    EXPECT_EQ(editing, nullptr);
    EXPECT_FALSE(design.motors(design.body()).containsId(design.id(2)));

    editing = std::make_unique<FlightConfigurationEditing>(design.document(), services);
    prompts.destroyDuringTheNextQuestion(editing);
    design.prompts().answerDeployment(
        deployment(Scope::THIS_CONFIGURATION, DeployEvent::APOGEE, 200, 0));
    EXPECT_FALSE(editing->selectDeployment(rows, std::vector<Uuid>{design.chute()}));
    EXPECT_EQ(editing, nullptr);
    EXPECT_EQ(design.deployments().size(), 0U);

    editing = std::make_unique<FlightConfigurationEditing>(design.document(), services);
    prompts.destroyDuringTheNextQuestion(editing);
    design.prompts().answerSeparation(
        separation(Scope::THIS_CONFIGURATION, SeparationEvent::BURNOUT, 200, 0));
    EXPECT_FALSE(editing->selectSeparation(rows, std::vector<Uuid>{design.booster()}));
    EXPECT_EQ(editing, nullptr);
    EXPECT_EQ(design.separations().size(), 0U);

    editing = std::make_unique<FlightConfigurationEditing>(design.document(), services);
    prompts.destroyDuringTheNextQuestion(editing);
    design.prompts().answerMotor(
        UserPrompts::MotorChoice{.motor = design.estes("A8"), .delay = 3, .nozzleExitDiameter = 0});
    EXPECT_FALSE(editing->selectMotor(rows, std::vector<Uuid>{design.inner()}));
    EXPECT_EQ(editing, nullptr);
    EXPECT_FALSE(design.motors(design.inner()).containsId(design.id(1)));

    editing = std::make_unique<FlightConfigurationEditing>(design.document(), services);
    prompts.destroyDuringTheNextQuestion(editing);
    design.prompts().answerRenameConfiguration(
        {.action = UserPrompts::RenameAction::RENAME, .name = QStringLiteral("Gone")});
    EXPECT_FALSE(editing->renameConfigurations(rows));
    EXPECT_EQ(editing, nullptr);
    EXPECT_NE(design.rocket()
                  .getFlightConfiguration(design.id(1))
                  .getNameRaw(design.services().services().preferences()),
              "Gone");

    // "Save as default": the operations go during its message, the default name is stored all the
    // same.
    editing = std::make_unique<FlightConfigurationEditing>(design.document(), services);
    prompts.destroyDuringTheNextQuestion(editing);
    design.prompts().answerRenameConfiguration(
        {.action = UserPrompts::RenameAction::CANCEL, .name = {}}, {QStringLiteral("Saved")});
    EXPECT_FALSE(editing->renameConfigurations(rows));
    EXPECT_EQ(editing, nullptr);
    EXPECT_EQ(design.services().services().preferences().getDefaultFlightConfigName(), "Saved");
}

}  // namespace

}  // namespace QtRocket::Gui
