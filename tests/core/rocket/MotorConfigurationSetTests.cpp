#include "QtRocket/rocket/MotorConfigurationSet.h"

#include <concepts>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/MotorConfigurationId.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ClusterConfiguration.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FlightConfigurableParameterSet.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Uuid.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::BodyTube;
using QtRocket::ComponentChangeEvent;
using QtRocket::FlightConfigurableParameterSet;
using QtRocket::FlightConfigurationId;
using QtRocket::IgnitionEvent;
using QtRocket::InMemoryPreferences;
using QtRocket::MotorConfiguration;
using QtRocket::MotorConfigurationId;
using QtRocket::MotorConfigurationSet;
using QtRocket::MotorMount;
using QtRocket::Test::addMotor;
using QtRocket::Test::motorC6;
using QtRocket::Test::motorD21;

// A mount's set is rebuilt for a copied mount; it is never copied or moved on its own, and its
// default cannot be replaced.
template <class Set>
concept CanSetDefault =
    requires(Set& set, MotorConfiguration value) { set.setDefault(std::move(value)); };
static_assert(!std::is_copy_constructible_v<MotorConfigurationSet>);
static_assert(!std::is_move_constructible_v<MotorConfigurationSet>);
static_assert(!CanSetDefault<MotorConfigurationSet>);
static_assert(CanSetDefault<FlightConfigurableParameterSet<MotorConfiguration>>);

TEST(MotorConfigurationSet, DefaultIsAnEmptyConfigurationOfTheMount)
{
    const std::unique_ptr<BodyTube> mount = std::make_unique<BodyTube>(0.2, 0.01);
    const MotorConfigurationSet&    set   = mount->getMotorConfigurationSet();
    EXPECT_EQ(set.size(), 0U);
    EXPECT_TRUE(set.getDefault().isEmpty());
    EXPECT_EQ(&set.getDefault().getMount(), static_cast<const MotorMount*>(mount.get()));
    EXPECT_TRUE(set.getDefault().getFcid().isDefaultId());
    EXPECT_EQ(
        set.getDefault().getMid(),
        (MotorConfigurationId{mount->getId(), FlightConfigurationId::defaultValueId().key()}));
    EXPECT_EQ(MotorConfigurationSet::kDefaultMotorEventType,
              ComponentChangeEvent::kMotorChange | ComponentChangeEvent::kEventChange);
    EXPECT_FALSE(mount->hasMotor());
}

TEST(MotorConfigurationSet, TheDefaultCannotChangeThroughTheBaseClass)
{
    const std::unique_ptr<BodyTube> mount = std::make_unique<BodyTube>(0.2, 0.01);
    FlightConfigurableParameterSet<MotorConfiguration>& base   = mount->getMotorConfigurationSet();
    const MotorConfiguration*                           before = &base.getDefault();

    // Java's override throws whichever reference it is called through: even a configuration
    // with a motor, or one equal to the default, is refused.
    MotorConfiguration withMotor{*mount, FlightConfigurationId::defaultValueId()};
    withMotor.setMotor(motorD21());
    EXPECT_THROW(base.setDefault(withMotor), QtRocket::BugError);
    EXPECT_THROW(base.setDefault(base.getDefault().clone()), QtRocket::BugError);
    EXPECT_EQ(&base.getDefault(), before);
    EXPECT_TRUE(base.getDefault().isEmpty());
    EXPECT_EQ(MotorConfiguration::kFixedDefaultMessage,
              "Cannot change default value of motor configuration");
}

/// Checks that @p copied is @p original made anew for @p target.
void expectCopiedFor(const MotorConfiguration& copied, const MotorConfiguration& original,
                     const BodyTube& target)
{
    EXPECT_EQ(&copied.getMount(), static_cast<const MotorMount*>(&target));
    EXPECT_EQ(copied.getFcid(), original.getFcid());
    EXPECT_EQ(copied.getMid(), (MotorConfigurationId{target.getId(), original.getFcid().key()}));
    EXPECT_EQ(copied.getMotor(), original.getMotor());
    EXPECT_EQ(copied.getEjectionDelay(), original.getEjectionDelay());
    EXPECT_EQ(copied.getIgnitionEvent(), original.getIgnitionEvent());
}

TEST(MotorConfigurationSet, CopyForANewMountRebuildsEveryOverride)
{
    const std::unique_ptr<BodyTube> source = std::make_unique<BodyTube>(0.2, 0.01);
    const FlightConfigurationId     a;
    const FlightConfigurationId     b;
    addMotor(*source, a, motorC6(), 5).setIgnitionEvent(IgnitionEvent::LAUNCH);
    addMotor(*source, b, motorD21(), 3);
    source->getDefaultMotorConfig().setIgnitionEvent(IgnitionEvent::NEVER);

    const std::unique_ptr<BodyTube> target = std::make_unique<BodyTube>(0.2, 0.01);
    const MotorConfigurationSet     copy{source->getMotorConfigurationSet(), *target};

    EXPECT_EQ(copy.getIds(), (std::vector<FlightConfigurationId>{a, b}));
    expectCopiedFor(copy.get(a), source->getMotorConfig(a), *target);
    expectCopiedFor(copy.get(b), source->getMotorConfig(b), *target);
    // The default is the source's made anew for the target: its ignition is carried over (M3's
    // bug 6, fixed; Java's copy has a fresh AUTOMATIC default, see TheCopyKeepsTheDefaultIgnition).
    EXPECT_EQ(copy.getDefault().getIgnitionEvent(), IgnitionEvent::NEVER);
    EXPECT_EQ(&copy.getDefault().getMount(), static_cast<const MotorMount*>(target.get()));
}

/// "<event> + <delay> override <yes|no>, motor <name|none>, ejection <delay>, nozzle <diameter>"
/// of @p config, the fields the probe of part C5 prints of a configuration.
[[nodiscard]] std::string ignitionOf(const MotorConfiguration& config)
{
    return std::format("{} + {} override {}, motor {}, ejection {}, nozzle {}",
                       QtRocket::name(config.getIgnitionEvent()), config.getIgnitionDelay(),
                       config.hasIgnitionOverride() ? "yes" : "no",
                       config.getMotor() != nullptr ? config.getMotor()->getDesignation() : "none",
                       config.getEjectionDelay(), config.getNozzleExitDiameter());
}

/// The default ignition of the scenario of the probe of part C5 (M3 run 1, probe/C5Probe.java,
/// defaultIgnition()): BURNOUT + 2.5 s.
constexpr std::string_view kDefaultIgnition =
    "BURNOUT + 2.5 override yes, motor none, ejection 0, nozzle 0";

/// The configuration of the scenario that has a motor: a C6-5 that ignites at LAUNCH.
constexpr std::string_view kLaunchIgnition =
    "LAUNCH + 0 override yes, motor C6, ejection 5, nozzle 0";

/// Gives @p mount the scenario's ignitions: the default BURNOUT + 2.5 s, and a C6-5 that ignites
/// at LAUNCH in @p withMotor.
void setScenarioIgnitions(MotorMount& mount, const FlightConfigurationId& withMotor)
{
    addMotor(mount, withMotor, motorC6(), 5).setIgnitionEvent(IgnitionEvent::LAUNCH);
    mount.getDefaultMotorConfig().setIgnitionEvent(IgnitionEvent::BURNOUT);
    mount.getDefaultMotorConfig().setIgnitionDelay(2.5);
}

/// Checks that @p mount has the scenario's ignitions (setScenarioIgnitions() with @p withMotor),
/// and that its default belongs to it with the mid @p defaultMid (a copy keeps the mids of the
/// source, also one with new ids, which derives them before it draws the ids: as for every
/// configuration, see CopiedMountsOwnTheirConfigurations).
void expectScenarioIgnitions(const MotorMount& mount, const FlightConfigurationId& withMotor,
                             const MotorConfigurationId& defaultMid)
{
    const FlightConfigurationId without;
    EXPECT_EQ(ignitionOf(mount.getDefaultMotorConfig()), kDefaultIgnition);
    EXPECT_EQ(ignitionOf(mount.getMotorConfig(without)), kDefaultIgnition)
        << "a configuration without a motor of its own reads the default";
    EXPECT_EQ(ignitionOf(mount.getMotorConfig(withMotor)), kLaunchIgnition);
    EXPECT_EQ(&mount.getDefaultMotorConfig().getMount(), &mount);
    EXPECT_EQ(mount.getDefaultMotorConfig().getMid(), defaultMid);
}

// QtRocket fixes M3's bug 6 (U-M3-5): a copy of a mount, with the original ids (an undo state, a
// simulation's rocket) or with new ones (copy and paste, duplicate), keeps the default ignition,
// and a configuration without a motor of its own still reads it. OpenRocket's copy() gives
// "AUTOMATIC delay 0.0 override false" for the default (the probe) and keeps the override
// ("LAUNCH delay 0.0 override true motor C6 ejection 5.0"), as QtRocket does.
TEST(MotorConfigurationSet, TheCopyKeepsTheDefaultIgnition)
{
    const FlightConfigurationId     withMotor;
    const std::unique_ptr<BodyTube> mount = std::make_unique<BodyTube>(0.2, 0.01);
    setScenarioIgnitions(*mount, withMotor);
    const MotorConfigurationId mid{mount->getId(), FlightConfigurationId::defaultValueId().key()};
    expectScenarioIgnitions(*mount, withMotor, mid);

    const std::unique_ptr<QtRocket::RocketComponent> sameIds = mount->copyWithOriginalId();
    expectScenarioIgnitions(dynamic_cast<const BodyTube&>(*sameIds), withMotor, mid);
    const std::unique_ptr<QtRocket::RocketComponent> newIds = mount->copyWithNewIds();
    expectScenarioIgnitions(dynamic_cast<const BodyTube&>(*newIds), withMotor, mid);
    EXPECT_NE(newIds->getId(), mount->getId());
}

// The default's override flag is carried as it is: a default nobody set stays without an
// override, and one reset with useDefaultIgnition() (which goes through the setters, as Java's)
// keeps its override.
TEST(MotorConfigurationSet, TheCopyKeepsTheDefaultsOverrideFlag)
{
    const std::unique_ptr<BodyTube> untouched = std::make_unique<BodyTube>(0.2, 0.01);
    const std::unique_ptr<BodyTube> target    = std::make_unique<BodyTube>(0.2, 0.01);
    const MotorConfigurationSet     fresh{untouched->getMotorConfigurationSet(), *target};
    EXPECT_EQ(ignitionOf(fresh.getDefault()),
              "AUTOMATIC + 0 override no, motor none, ejection 0, nozzle 0");

    const FlightConfigurationId     fcid;
    const std::unique_ptr<BodyTube> reset = std::make_unique<BodyTube>(0.2, 0.01);
    setScenarioIgnitions(*reset, fcid);
    reset->getDefaultMotorConfig().useDefaultIgnition();
    const MotorConfigurationSet copy{reset->getMotorConfigurationSet(), *target};
    EXPECT_EQ(ignitionOf(copy.getDefault()),
              "AUTOMATIC + 0 override yes, motor none, ejection 0, nozzle 0");
}

/// The motor mount of @p document's rocket whose id is @p id.
[[nodiscard]] const MotorMount& mountIn(const QtRocket::OpenRocketDocument& document,
                                        const QtRocket::Uuid&               id)
{
    return dynamic_cast<const MotorMount&>(*document.getRocket().findComponent(id));
}

/// The length of the body tube of @p document's rocket whose id is @p id, or -1 without one.
[[nodiscard]] double bodyLengthIn(const QtRocket::OpenRocketDocument& document,
                                  const QtRocket::Uuid&               id)
{
    const auto* body = dynamic_cast<const BodyTube*>(document.getRocket().findComponent(id));
    return body != nullptr ? body->getLength() : -1;
}

// Undo and redo load copies of the rocket (Rocket::loadFrom()), which keep the default ignition
// now. OpenRocket's undo() and redo() give "AUTOMATIC delay 0.0 override false" for the default of
// the probe's mount after an unrelated change (the probe: the body tube's length).
TEST(MotorConfigurationSet, UndoAndRedoKeepTheDefaultIgnition)
{
    auto                        rocket = std::make_unique<QtRocket::Rocket>();
    QtRocket::AxialStage&       stage  = rocket->addChild(std::make_unique<QtRocket::AxialStage>());
    BodyTube&                   body   = stage.addChild(std::make_unique<BodyTube>(0.3, 0.012));
    QtRocket::InnerTube&        tube   = body.addChild(std::make_unique<QtRocket::InnerTube>());
    const QtRocket::Uuid        bodyId = body.getId();
    const QtRocket::Uuid        tubeId = tube.getId();
    const FlightConfigurationId withMotor;
    rocket->createFlightConfiguration(withMotor);
    setScenarioIgnitions(tube, withMotor);

    const std::unique_ptr<QtRocket::OpenRocketDocument> document =
        QtRocket::OpenRocketDocumentFactory::createDocumentFromRocket(std::move(rocket));
    document->addUndoPosition("Change length");
    body.setLength(0.5);

    const MotorConfigurationId mid{tubeId, FlightConfigurationId::defaultValueId().key()};
    // The undo and the redo load other rockets (the lengths show which), so the ignitions below
    // come from the copies, not from the rocket the scenario set them on.
    ASSERT_TRUE(document->isUndoAvailable());
    document->undo();
    EXPECT_EQ(bodyLengthIn(*document, bodyId), 0.3);
    expectScenarioIgnitions(mountIn(*document, tubeId), withMotor, mid);
    ASSERT_TRUE(document->isRedoAvailable());
    document->redo();
    EXPECT_EQ(bodyLengthIn(*document, bodyId), 0.5);
    expectScenarioIgnitions(mountIn(*document, tubeId), withMotor, mid);
}

// The tubes of a split cluster (InnerTube::makeIndividualClusterComponent(), a copy with new
// ids) keep the cluster's default ignition too (M3's bug 6, fixed), and own their configurations.
// OpenRocket's split goes through copy(): the probe of M3 run 1's fixer of group C
// (SplitProbe.java: a 3-ring cluster with the scenario's ignitions) gives every split tube
// "AUTOMATIC delay 0.0 override false" for the default and for a configuration without a motor,
// and "LAUNCH delay 0.0 override true motor C6 ejection 5.0" for the one with the motor, as
// QtRocket does.
TEST(MotorConfigurationSet, TheTubesOfASplitClusterKeepTheDefaultIgnition)
{
    QtRocket::Rocket            rocket;
    QtRocket::AxialStage&       stage = rocket.addChild(std::make_unique<QtRocket::AxialStage>());
    BodyTube&                   body  = stage.addChild(std::make_unique<BodyTube>(0.3, 0.025));
    QtRocket::InnerTube&        tube  = body.addChild(std::make_unique<QtRocket::InnerTube>());
    const FlightConfigurationId withMotor;
    tube.setOuterRadius(0.009);
    const QtRocket::ClusterConfiguration* ring =
        QtRocket::ClusterConfiguration::fromXmlName("3-ring");
    ASSERT_NE(ring, nullptr);
    tube.setClusterConfiguration(*ring);
    rocket.createFlightConfiguration(withMotor);
    setScenarioIgnitions(tube, withMotor);
    const MotorConfigurationId mid{tube.getId(), FlightConfigurationId::defaultValueId().key()};

    const std::vector<QtRocket::Coordinate> locations = tube.getComponentLocations();
    ASSERT_EQ(locations.size(), 3U);
    for (const QtRocket::Coordinate& location : locations)
    {
        const std::unique_ptr<QtRocket::InnerTube> split =
            QtRocket::InnerTube::makeIndividualClusterComponent(location, "MMT", tube);
        expectScenarioIgnitions(*split, withMotor, mid);
    }
}

TEST(MotorConfigurationSet, CopiedMountsOwnTheirConfigurations)
{
    const std::unique_ptr<BodyTube> mount = std::make_unique<BodyTube>(0.2, 0.01);
    const FlightConfigurationId     fcid;
    addMotor(*mount, fcid, motorC6(), 5);
    mount->setMotorOverhang(0.02);

    const std::unique_ptr<QtRocket::RocketComponent> copy = mount->copyWithOriginalId();
    const auto& copiedMount                               = dynamic_cast<const BodyTube&>(*copy);
    EXPECT_EQ(&copiedMount.getMotorConfig(fcid).getMount(),
              static_cast<const MotorMount*>(&copiedMount));
    EXPECT_EQ(copiedMount.getMotorConfig(fcid).getMid(), mount->getMotorConfig(fcid).getMid())
        << "the original ids: the same mids";
    EXPECT_TRUE(copiedMount.isMotorMount());
    EXPECT_EQ(copiedMount.getMotorOverhang(), 0.02);

    const std::unique_ptr<QtRocket::RocketComponent> fresh = mount->copyWithNewIds();
    const auto& freshMount                                 = dynamic_cast<const BodyTube&>(*fresh);
    EXPECT_EQ(&freshMount.getMotorConfig(fcid).getMount(),
              static_cast<const MotorMount*>(&freshMount));
    // As in Java, the mids were derived before the new ids were drawn.
    EXPECT_EQ(freshMount.getMotorConfig(fcid).getMid(), mount->getMotorConfig(fcid).getMid());
}

TEST(MotorConfigurationSet, SetRemoveAndResetOnTheMount)
{
    const std::unique_ptr<BodyTube> mount = std::make_unique<BodyTube>(0.2, 0.01);
    const FlightConfigurationId     fcid;
    EXPECT_FALSE(mount->isMotorMount());
    addMotor(*mount, fcid, motorC6());
    EXPECT_TRUE(mount->isMotorMount()) << "setMotorConfig() makes the component a mount";
    EXPECT_TRUE(mount->hasMotor());
    EXPECT_FALSE(mount->getMotorConfig(fcid).isEmpty());

    mount->setMotorConfig(std::nullopt, fcid);
    EXPECT_FALSE(mount->hasMotor());
    EXPECT_TRUE(mount->getMotorConfig(fcid).isEmpty()) << "back to the default";

    addMotor(*mount, fcid, motorC6());
    mount->reset(fcid);
    EXPECT_FALSE(mount->hasMotor());

    // A configuration of another mount is refused.
    const std::unique_ptr<BodyTube> other = std::make_unique<BodyTube>(0.2, 0.01);
    EXPECT_THROW(mount->setMotorConfig(MotorConfiguration{*other, fcid}, fcid), QtRocket::BugError);
}

TEST(MotorConfigurationSet, CopyFlightConfigurationCopiesTheMotor)
{
    const std::unique_ptr<BodyTube> mount = std::make_unique<BodyTube>(0.2, 0.01);
    const FlightConfigurationId     a;
    const FlightConfigurationId     b;
    addMotor(*mount, a, motorD21(), 3);
    mount->copyFlightConfiguration(a, b);
    EXPECT_EQ(mount->getMotorConfig(b).getMotor(), mount->getMotorConfig(a).getMotor());
    EXPECT_EQ(mount->getMotorConfig(b).getFcid(), b);
    EXPECT_EQ(mount->getMotorConfig(b).getEjectionDelay(), 3.0);
    EXPECT_NE(mount->getMotorConfig(b).getMid(), mount->getMotorConfig(a).getMid());
}

TEST(MotorConfigurationSet, ToDebug)
{
    const std::unique_ptr<BodyTube> mount = std::make_unique<BodyTube>(0.2, 0.01);
    mount->setName("Tube");
    const FlightConfigurationId fcid;
    addMotor(*mount, fcid, motorD21(), 3);
    const InMemoryPreferences preferences;

    const MotorConfiguration& defaults = mount->getDefaultMotorConfig();
    const MotorConfiguration& config   = mount->getMotorConfig(fcid);
    // Java's "@%10s=[fcid//%8s][mid//%8s][    %8s ign@: %12s]\n".
    const auto line = [](const std::string& key, const MotorConfiguration& c,
                         const std::string& motorName) {
        return std::format("@{:>10}=[fcid//{:>8}][mid//{:>8}][    {:>8} ign@: {:>12}]\n", key,
                           c.getFcid().toShortKey(), c.getMid().toShortKey(), motorName,
                           c.toIgnitionDescription());
    };
    const std::string expected = " ====== Dumping MotorConfigurationSet: 1 motors in " +
                                 mount->getDebugName() + " ======\n" + "  [DEF]" +
                                 line("DefaultKey", defaults, "None") + "       " +
                                 line(fcid.toShortKey(), config, "D21-3");
    EXPECT_EQ(mount->getMotorConfigurationSet().toDebug(preferences), expected);
    EXPECT_EQ(mount->toMotorDebug(preferences), expected);
}

TEST(MotorConfigurationSet, IsAFlightConfigurableParameterSet)
{
    static_assert(std::derived_from<MotorConfigurationSet,
                                    FlightConfigurableParameterSet<MotorConfiguration>>);
    const std::unique_ptr<BodyTube> mount = std::make_unique<BodyTube>(0.2, 0.01);
    const FlightConfigurationId     fcid;
    addMotor(*mount, fcid, motorD21());
    // isDefault(E) compares mids, as Java's equals() does.
    EXPECT_FALSE(mount->getMotorConfigurationSet().isDefault(mount->getMotorConfig(fcid)));
    EXPECT_TRUE(mount->getMotorConfigurationSet().isDefault(mount->getDefaultMotorConfig()));
    EXPECT_EQ(mount->getMotorConfigurationSet().getId(mount->getMotorConfig(fcid)), fcid);
}

}  // namespace
