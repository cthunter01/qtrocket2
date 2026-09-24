#include "QtRocket/logging/Message.h"

#include <memory>
#include <stdexcept>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/logging/ErrorMessage.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"

namespace
{

using QtRocket::ErrorMessage;
using QtRocket::Message;
using QtRocket::MessagePriority;
using QtRocket::MessageSource;
using QtRocket::MessageSources;
using QtRocket::SimulationAbort;
using QtRocket::Warning;

// Message is abstract; the concrete messages stand in for it. SimulationAbort refines nothing,
// so it shows the base behaviour.

TEST(Message, ToStringAppendsTheSourceNames)
{
    Warning::Other warning{"Gap in rocket airframe"};
    EXPECT_EQ(warning.toString(), "Gap in rocket airframe");
    warning.setSources(MessageSources{{"bt-1", "Body tube"}});
    EXPECT_EQ(warning.toString(), "Gap in rocket airframe:  \"Body tube\"");
    warning.setSources(MessageSources{{"bt-1", "Body tube"}, {"nc-1", "Nose cone"}});
    EXPECT_EQ(warning.toString(), "Gap in rocket airframe:  \"Body tube\", \"Nose cone\"");
    EXPECT_EQ(warning.messageDescription(), "Gap in rocket airframe");  // never the sources
    warning.setSources({});
    EXPECT_EQ(warning.toString(), "Gap in rocket airframe");
}

TEST(Message, SourcesAreComponentsComparedById)
{
    const MessageSource finSet1{"fs-1", "Fin set"};
    const MessageSource finSet2{"fs-2", "Fin set"};
    const MessageSource renamed{"fs-1", "Fins"};
    EXPECT_FALSE(finSet1 == finSet2);  // two components with the default name: two sources
    EXPECT_TRUE(finSet1 != finSet2);
    EXPECT_TRUE(finSet1 == renamed);  // the name plays no part (Java: RocketComponent.equals())
    EXPECT_EQ(finSet1.name, "Fin set");
    EXPECT_EQ(renamed.id, "fs-1");
}

TEST(Message, EqualityIsTypeSourcesAndPriority)
{
    SimulationAbort a{SimulationAbort::Cause::NO_CP};
    SimulationAbort b{SimulationAbort::Cause::NO_LIFTOFF};
    EXPECT_TRUE(a == b);  // ids and causes play no part
    b.setPriority(MessagePriority::HIGH);
    EXPECT_FALSE(a == b);
    a.setPriority(MessagePriority::HIGH);
    EXPECT_TRUE(a == b);
    a.setSources(MessageSources{{"fs-1", "Fin set"}});
    EXPECT_FALSE(a == b);
    b.setSources(MessageSources{{"fs-2", "Fin set"}});
    EXPECT_FALSE(a == b);  // the same name, another component
    b.setSources(MessageSources{{"fs-1", "Fins"}});
    EXPECT_TRUE(a == b);  // the same component, whatever it is called now
    a.setSources(MessageSources{{"fs-1", "Fin set"}, {"bt-1", "Body tube"}});
    b.setSources(MessageSources{{"bt-1", "Body tube"}, {"fs-1", "Fin set"}});
    EXPECT_FALSE(a == b);  // the order matters, as for Arrays.equals()
    EXPECT_FALSE(a == Warning::Other{"x"});
    EXPECT_FALSE(a == ErrorMessage::Other{"x"});
}

TEST(Message, SameTypeIsTheExactDynamicType)
{
    const Warning::Other      a{"a"};
    const Warning::Other      b{"b"};
    const ErrorMessage::Other e{"a"};
    const Warning::LargeAOA   aoa{0.1};
    EXPECT_TRUE(a.sameType(b));
    EXPECT_TRUE(a.sameType(a));
    EXPECT_FALSE(a.sameType(e));
    EXPECT_FALSE(e.sameType(a));
    EXPECT_FALSE(a.sameType(aoa));
    const Message& asBase = aoa;
    EXPECT_TRUE(asBase.sameType(Warning::LargeAOA{0.5}));
    EXPECT_FALSE(asBase.sameType(a));
}

TEST(Message, ReplaceContentsThrowsByDefault)
{
    Warning::Other other{"x"};
    EXPECT_THROW(other.replaceContents(Warning::Other{"y"}), std::logic_error);
    SimulationAbort noCp{SimulationAbort::Cause::NO_CP};
    EXPECT_THROW(noCp.replaceContents(noCp), std::logic_error);
    EXPECT_EQ(other.description(), "x");  // untouched
}

TEST(Message, IdsAreRandomUuidsAndCanBeSet)
{
    const Warning::Other a{"a"};
    const Warning::Other b{"a"};
    EXPECT_NE(a.id(), b.id());
    ASSERT_EQ(a.id().size(), 36U);
    EXPECT_EQ(a.id()[8], '-');
    EXPECT_EQ(a.id()[13], '-');
    EXPECT_EQ(a.id()[18], '-');
    EXPECT_EQ(a.id()[23], '-');
    EXPECT_EQ(a.id()[14], '4');                                                    // version 4
    EXPECT_NE(std::string_view{"89ab"}.find(a.id()[19]), std::string_view::npos);  // variant
    EXPECT_TRUE(a == b);  // ids play no part in equality (EventAfterLanding excepted)
    Warning::Other c{"c"};
    c.setId("0f8fad5b-d9cb-469f-a165-70867728950e");
    EXPECT_EQ(c.id(), "0f8fad5b-d9cb-469f-a165-70867728950e");
}

TEST(Message, CloneCopiesIdSourcesAndPriority)
{
    Warning::Other original{"text"};
    original.setId("id-1");
    original.setSources(MessageSources{{"fs-1", "Fin set"}});
    original.setPriority(MessagePriority::HIGH);
    const std::unique_ptr<Message> copy = original.clone();
    ASSERT_NE(copy, nullptr);
    EXPECT_NE(copy.get(), &original);
    EXPECT_EQ(copy->id(), "id-1");
    EXPECT_EQ(copy->sources(), (MessageSources{{"fs-1", "Fin set"}}));
    EXPECT_EQ(copy->priority(), MessagePriority::HIGH);
    EXPECT_EQ(copy->toString(), "text:  \"Fin set\"");
    EXPECT_TRUE(*copy == original);
    // The copy has state of its own.
    original.setPriority(MessagePriority::LOW);
    EXPECT_EQ(copy->priority(), MessagePriority::HIGH);
    EXPECT_FALSE(*copy == original);
}

TEST(Message, PriorityAndSourcesCanBeSet)
{
    Warning::Other warning{"x"};
    EXPECT_EQ(warning.priority(), MessagePriority::NORMAL);
    warning.setPriority(MessagePriority::LOW);
    EXPECT_EQ(warning.priority(), MessagePriority::LOW);
    EXPECT_TRUE(warning.sources().empty());
    warning.setSources(MessageSources{{"bt-1", "Body tube"}});
    ASSERT_EQ(warning.sources().size(), 1U);
    EXPECT_EQ(warning.sources().front().id, "bt-1");
    EXPECT_EQ(warning.sources().front().name, "Body tube");
}

}  // namespace
