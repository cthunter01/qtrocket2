#include "QtRocket/rocket/MotorConfigurationSet.h"

#include <concepts>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/MotorConfigurationId.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FlightConfigurableParameterSet.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
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
    // As in Java, the default is a fresh one: the source default's ignition is not carried over.
    EXPECT_EQ(copy.getDefault().getIgnitionEvent(), IgnitionEvent::AUTOMATIC);
    EXPECT_EQ(&copy.getDefault().getMount(), static_cast<const MotorMount*>(target.get()));
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
