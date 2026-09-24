#include "QtRocket/logging/ErrorMessage.h"

#include <memory>
#include <stdexcept>

#include <gtest/gtest.h>

#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/Warning.h"

namespace
{

using QtRocket::ErrorMessage;
using QtRocket::Message;
using QtRocket::MessagePriority;
using QtRocket::MessageSources;
using QtRocket::Warning;

TEST(ErrorMessage, OtherIsItsText)
{
    const ErrorMessage::Other error{"boom"};
    EXPECT_EQ(error.description(), "boom");
    EXPECT_EQ(error.messageDescription(), "boom");
    EXPECT_EQ(error.toString(), "boom");
    EXPECT_EQ(error.typeName(), "Other");
    EXPECT_EQ(error.priority(), MessagePriority::NORMAL);
    EXPECT_TRUE(error.sources().empty());
    EXPECT_EQ(ErrorMessage::fromString("x").description(), "x");
    EXPECT_EQ(ErrorMessage::fromString("").toString(), "");
}

TEST(ErrorMessage, OtherComparesTheTextOnly)
{
    const ErrorMessage::Other a{"boom"};
    ErrorMessage::Other       b{"boom"};
    b.setPriority(MessagePriority::HIGH);
    b.setSources(MessageSources{{"fs-1", "Fin set"}});
    EXPECT_TRUE(a == b);  // Java's Error.Other.equals() looks at the description only
    EXPECT_TRUE(b == a);
    EXPECT_FALSE(a == ErrorMessage::Other{"bang"});
    EXPECT_TRUE(a != ErrorMessage::Other{"bang"});
    // A warning and an error with the same text are different kinds of message.
    EXPECT_FALSE(a == Warning::Other{"boom"});
    EXPECT_FALSE(Warning::Other{"boom"} == a);
}

TEST(ErrorMessage, OtherIsNeverReplaced)
{
    const ErrorMessage::Other a{"boom"};
    EXPECT_FALSE(a.replaceBy(ErrorMessage::Other{"boom"}));
    EXPECT_FALSE(a.replaceBy(ErrorMessage::Other{"bang"}));
    EXPECT_THROW(ErrorMessage::Other{"x"}.replaceContents(a), std::logic_error);
}

TEST(ErrorMessage, CloneKeepsTypeTextIdAndSources)
{
    ErrorMessage::Other original{"boom"};
    original.setId("error-1");
    original.setSources(MessageSources{{"nc-1", "Nose cone"}});
    original.setPriority(MessagePriority::HIGH);
    const std::unique_ptr<Message> copy = original.clone();
    ASSERT_NE(copy, nullptr);
    EXPECT_NE(copy.get(), &original);
    EXPECT_EQ(copy->typeName(), "Other");
    EXPECT_EQ(copy->id(), "error-1");
    EXPECT_EQ(copy->priority(), MessagePriority::HIGH);
    EXPECT_EQ(copy->toString(), "boom:  \"Nose cone\"");
    const auto* typed = dynamic_cast<const ErrorMessage::Other*>(copy.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->description(), "boom");
    EXPECT_TRUE(*copy == original);
}

}  // namespace
