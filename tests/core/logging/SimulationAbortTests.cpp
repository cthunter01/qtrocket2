#include "QtRocket/logging/SimulationAbort.h"

#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/MessagePriority.h"

namespace
{

using QtRocket::causeFromName;
using QtRocket::causeName;
using QtRocket::causeText;
using QtRocket::Message;
using QtRocket::MessagePriority;
using QtRocket::MessageSources;
using QtRocket::SimulationAbort;
using Cause = SimulationAbort::Cause;

/// Checks a cause's constant name, its OpenRocket text and the name round trip.
void expectCause(Cause cause, std::string_view name, std::string_view text)
{
    EXPECT_EQ(causeName(cause), name);
    EXPECT_EQ(causeText(cause), text);
    EXPECT_EQ(SimulationAbort{cause}.messageDescription(), text);
    EXPECT_EQ(SimulationAbort{cause}.cause(), cause);
    EXPECT_EQ(causeFromName(name), std::optional<Cause>{cause});
}

TEST(SimulationAbort, EveryCauseHasItsNameAndText)
{
    expectCause(Cause::NO_ACTIVE_STAGES, "NO_ACTIVE_STAGES", "No active stages");
    expectCause(Cause::NO_MOTORS_DEFINED, "NO_MOTORS_DEFINED",
                "No motors defined in the simulation");
    expectCause(Cause::NO_CONFIGURED_IGNITION, "NO_CONFIGURED_IGNITION",
                "No motors configured to ignite at liftoff");
    expectCause(Cause::NO_MOTORS_FIRED, "NO_MOTORS_FIRED", "No motors ignited");
    expectCause(Cause::NO_LIFTOFF, "NO_LIFTOFF",
                "<html>Motor burnout without liftoff. <br>Use more (powerful) motors, or decrease "
                "the rocket mass.</html>");
    expectCause(Cause::NO_CP, "NO_CP", "Can't calculate Center of Pressure");
    expectCause(Cause::ACTIVE_LENGTH_ZERO, "ACTIVE_LENGTH_ZERO", "Active airframe has length 0");
    expectCause(Cause::ACTIVE_MASS_ZERO, "ACTIVE_MASS_ZERO", "Total mass of active stages is 0");
    expectCause(Cause::TUMBLE_UNDER_THRUST, "TUMBLE_UNDER_THRUST",
                "Stage began to tumble under thrust.");
    expectCause(Cause::DEPLOY_UNDER_THRUST, "DEPLOY_UNDER_THRUST",
                "Recovery system deployed while still under thrust");
    EXPECT_EQ(SimulationAbort::kAllCauses.size(), 10U);
}

TEST(SimulationAbort, CausesHaveDistinctNamesAndTexts)
{
    std::set<std::string_view> names;
    std::set<std::string_view> texts;
    for (const Cause cause : SimulationAbort::kAllCauses)
    {
        names.insert(causeName(cause));
        texts.insert(causeText(cause));
    }
    EXPECT_EQ(names.size(), SimulationAbort::kAllCauses.size());
    EXPECT_EQ(texts.size(), SimulationAbort::kAllCauses.size());
    EXPECT_FALSE(names.contains(""));
    EXPECT_FALSE(texts.contains(""));
}

TEST(SimulationAbort, UnknownNamesGiveNoCause)
{
    EXPECT_EQ(causeFromName("noactivestages"), std::nullopt);  // the .ork spelling: not ours
    EXPECT_EQ(causeFromName("no_cp"), std::nullopt);
    EXPECT_EQ(causeFromName(""), std::nullopt);
    EXPECT_EQ(causeFromName("NO_CP "), std::nullopt);
}

TEST(SimulationAbort, IsAMessageWithJavaSemantics)
{
    const SimulationAbort noLiftoff{Cause::NO_LIFTOFF};
    EXPECT_EQ(noLiftoff.priority(), MessagePriority::NORMAL);
    EXPECT_EQ(noLiftoff.typeName(), "SimulationAbort");
    EXPECT_TRUE(noLiftoff.sources().empty());
    EXPECT_FALSE(noLiftoff.replaceBy(SimulationAbort{Cause::NO_CP}));
    EXPECT_THROW(SimulationAbort{Cause::NO_CP}.replaceContents(noLiftoff), std::logic_error);
    // Java does not override equals(): two aborts are equal whatever their causes.
    EXPECT_TRUE(noLiftoff == SimulationAbort{Cause::NO_CP});
    SimulationAbort withSource{Cause::ACTIVE_MASS_ZERO};
    withSource.setSources(MessageSources{{"st-1", "Sustainer"}});
    EXPECT_FALSE(noLiftoff == withSource);
    EXPECT_EQ(withSource.toString(), "Total mass of active stages is 0:  \"Sustainer\"");
}

TEST(SimulationAbort, CloneKeepsCauseIdAndSources)
{
    SimulationAbort original{Cause::TUMBLE_UNDER_THRUST};
    original.setSources(MessageSources{{"bo-1", "Booster"}});
    original.setId("abort-1");
    const std::unique_ptr<Message> copy = original.clone();
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->id(), "abort-1");
    EXPECT_EQ(copy->toString(), "Stage began to tumble under thrust.:  \"Booster\"");
    const auto* typed = dynamic_cast<const SimulationAbort*>(copy.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->cause(), Cause::TUMBLE_UNDER_THRUST);
    EXPECT_TRUE(*copy == original);
}

}  // namespace
