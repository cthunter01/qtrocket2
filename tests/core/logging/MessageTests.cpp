#include "QtRocket/logging/Message.h"

#include <memory>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/logging/ErrorMessage.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"
#include "logging/TestSources.h"

namespace
{

using QtRocket::BugError;
using QtRocket::ErrorMessage;
using QtRocket::Message;
using QtRocket::MessagePriority;
using QtRocket::MessageSource;
using QtRocket::MessageSources;
using QtRocket::SimulationAbort;
using QtRocket::Uuid;
using QtRocket::Warning;
using QtRocket::Test::componentId;
using QtRocket::Test::source;

// Message is abstract; the concrete messages stand in for it. SimulationAbort refines nothing,
// so it shows the base behaviour.

/// A message built with a given id, through the protected Message(Uuid) constructor.
class Stamped final : public Message
{
public:
    explicit Stamped(Uuid id) : Message(id) { }

    [[nodiscard]] std::string messageDescription() const override { return "stamped"; }
    [[nodiscard]] bool        replaceBy(const Message& /*other*/) const override { return false; }
    [[nodiscard]] std::unique_ptr<Message> clone() const override
    {
        return std::make_unique<Stamped>(*this);
    }
    [[nodiscard]] std::string_view typeName() const noexcept override { return "Stamped"; }
};

TEST(Message, ToStringAppendsTheSourceNames)
{
    Warning::Other warning{"Gap in rocket airframe"};
    EXPECT_EQ(warning.toString(), "Gap in rocket airframe");
    warning.setSources(MessageSources{source("bt-1", "Body tube")});
    EXPECT_EQ(warning.toString(), "Gap in rocket airframe:  \"Body tube\"");
    warning.setSources(MessageSources{source("bt-1", "Body tube"), source("nc-1", "Nose cone")});
    EXPECT_EQ(warning.toString(), "Gap in rocket airframe:  \"Body tube\", \"Nose cone\"");
    EXPECT_EQ(warning.messageDescription(), "Gap in rocket airframe");  // never the sources
    warning.setSources({});
    EXPECT_EQ(warning.toString(), "Gap in rocket airframe");
}

TEST(Message, SourcesAreComponentsComparedById)
{
    const MessageSource finSet1 = source("fs-1", "Fin set");
    const MessageSource finSet2 = source("fs-2", "Fin set");
    const MessageSource renamed = source("fs-1", "Fins");
    EXPECT_FALSE(finSet1 == finSet2);  // two components with the default name: two sources
    EXPECT_TRUE(finSet1 != finSet2);
    EXPECT_TRUE(finSet1 == renamed);  // the name plays no part (Java: RocketComponent.equals())
    EXPECT_EQ(finSet1.name, "Fin set");
    EXPECT_EQ(renamed.id, componentId("fs-1"));
    EXPECT_NE(renamed.id, componentId("fs-2"));
    // A source is a component's id and name.
    const Uuid          id = Uuid::random();
    const MessageSource direct{id, "Body tube"};
    EXPECT_EQ(direct.id, id);
    EXPECT_EQ(direct.name, "Body tube");
    EXPECT_FALSE(direct == finSet1);
}

TEST(Message, TestComponentIdsFollowTheirTags)
{
    // The test helper: one Uuid per tag, up to the eight characters that fit in the packed half.
    EXPECT_EQ(componentId("fs-1"), componentId("fs-1"));
    EXPECT_NE(componentId("fs-1"), componentId("fs-2"));
    EXPECT_NE(componentId("abcdefgh"), componentId("bcdefgh"));
    EXPECT_FALSE(componentId("a").isNil());
    EXPECT_THROW(componentId("nine-char"), BugError);  // would collide with its own suffix
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
    a.setSources(MessageSources{source("fs-1", "Fin set")});
    EXPECT_FALSE(a == b);
    b.setSources(MessageSources{source("fs-2", "Fin set")});
    EXPECT_FALSE(a == b);  // the same name, another component
    b.setSources(MessageSources{source("fs-1", "Fins")});
    EXPECT_TRUE(a == b);  // the same component, whatever it is called now
    a.setSources(MessageSources{source("fs-1", "Fin set"), source("bt-1", "Body tube")});
    b.setSources(MessageSources{source("bt-1", "Body tube"), source("fs-1", "Fin set")});
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
    EXPECT_THROW(other.replaceContents(Warning::Other{"y"}), BugError);
    SimulationAbort noCp{SimulationAbort::Cause::NO_CP};
    EXPECT_THROW(noCp.replaceContents(noCp), BugError);
    EXPECT_EQ(other.description(), "x");  // untouched
}

TEST(Message, IdsAreRandomUuids)
{
    const Warning::Other a{"a"};
    const Warning::Other b{"a"};
    EXPECT_NE(a.id(), b.id());
    EXPECT_FALSE(a.id().isNil());
    EXPECT_EQ(a.id().version(), 4);  // UUID.randomUUID(): a random version 4 ...
    EXPECT_EQ(a.id().variant(), 2);  // ... RFC 4122 UUID
    EXPECT_EQ(a.id().toString().size(), 36U);
    EXPECT_TRUE(a == b);  // ids play no part in equality (EventAfterLanding excepted)
}

TEST(Message, IdCanBeSetOrGivenToTheConstructor)
{
    const Uuid     id{0x0f8fad5bd9cb469fU, 0xa16570867728950eU};
    Warning::Other c{"c"};
    c.setId(id);
    EXPECT_EQ(c.id(), id);
    // What the .ork saver writes for it (Java: getID().toString()).
    EXPECT_EQ(c.id().toString(), "0f8fad5b-d9cb-469f-a165-70867728950e");
    // Message(Uuid) takes the id as it is, the nil id included.
    const Stamped given{id};
    EXPECT_EQ(given.id(), id);
    EXPECT_EQ(given.clone()->id(), id);
    EXPECT_TRUE(Stamped{Uuid::nil()}.id().isNil());
}

TEST(Message, CloneCopiesIdSourcesAndPriority)
{
    Warning::Other original{"text"};
    const Uuid     id = Uuid::random();
    original.setId(id);
    original.setSources(MessageSources{source("fs-1", "Fin set")});
    original.setPriority(MessagePriority::HIGH);
    const std::unique_ptr<Message> copy = original.clone();
    ASSERT_NE(copy, nullptr);
    EXPECT_NE(copy.get(), &original);
    EXPECT_EQ(copy->id(), id);
    EXPECT_EQ(copy->sources(), (MessageSources{source("fs-1", "Fin set")}));
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
    warning.setSources(MessageSources{source("bt-1", "Body tube")});
    ASSERT_EQ(warning.sources().size(), 1U);
    EXPECT_EQ(warning.sources().front().id, componentId("bt-1"));
    EXPECT_EQ(warning.sources().front().name, "Body tube");
}

}  // namespace
