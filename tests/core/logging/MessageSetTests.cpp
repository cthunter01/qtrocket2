#include "QtRocket/logging/MessageSet.h"

#include <cstddef>
#include <format>
#include <functional>
#include <iterator>
#include <optional>
#include <source_location>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/ErrorMessage.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Uuid.h"
#include "logging/TestSources.h"

namespace
{

using QtRocket::BugError;
using QtRocket::ErrorMessage;
using QtRocket::ErrorSet;
using QtRocket::MessagePriority;
using QtRocket::MessageSources;
using QtRocket::ModId;
using QtRocket::Uuid;
using QtRocket::Warning;
using QtRocket::WarningSet;
using QtRocket::Test::source;

/// The what() of the BugError that @p body throws; fails the test when it throws nothing.
std::string bugMessage(const std::function<void()>& body)
{
    try
    {
        body();
    }
    catch (const BugError& error)
    {
        return error.what();
    }
    ADD_FAILURE() << "no BugError was thrown";
    return {};
}

TEST(MessageSet, IsEmptyUntilAdded)
{
    ErrorSet errors;
    EXPECT_TRUE(errors.empty());
    EXPECT_EQ(errors.size(), 0U);
    EXPECT_TRUE(errors.add("first"));
    EXPECT_FALSE(errors.empty());
    EXPECT_EQ(errors.size(), 1U);
}

TEST(MessageSet, KeepsInsertionOrderAndDeduplicates)
{
    ErrorSet errors;
    EXPECT_TRUE(errors.add("first"));
    EXPECT_TRUE(errors.add("second"));
    EXPECT_FALSE(errors.add("first"));  // the same text is the same kind of error
    EXPECT_TRUE(errors.add("third"));
    std::vector<std::string> texts;
    for (const ErrorMessage& error : errors)
    {
        texts.push_back(error.toString());
    }
    EXPECT_EQ(texts, (std::vector<std::string>{"first", "second", "third"}));
}

TEST(MessageSet, ErrorSetDeduplicatesByTextOnly)
{
    const ErrorMessage::Other a{"boom"};
    ErrorMessage::Other       b{"boom"};
    b.setPriority(MessagePriority::HIGH);
    b.setSources(MessageSources{source("fs-1", "Fin set")});
    ErrorSet errors;
    EXPECT_TRUE(errors.add(a));
    EXPECT_FALSE(errors.add(b));  // Error.Other.equals() looks at the text only
    EXPECT_EQ(errors.size(), 1U);
    EXPECT_EQ(errors.begin()->priority(), MessagePriority::NORMAL);  // the first one stays
    EXPECT_TRUE(errors.contains(b));
}

TEST(MessageSet, AddStoresACopy)
{
    WarningSet              warnings;
    const Warning::LargeAOA aoa{0.2};
    EXPECT_TRUE(warnings.add(aoa));
    const Warning* stored = warnings.find(aoa);
    ASSERT_NE(stored, nullptr);
    EXPECT_NE(stored, &aoa);
    EXPECT_EQ(stored->id(), aoa.id());
    // Replacing the stored angle leaves the caller's object alone.
    EXPECT_FALSE(warnings.add(Warning::LargeAOA{0.5}));
    EXPECT_DOUBLE_EQ(aoa.aoa(), 0.2);
    const auto* replaced = dynamic_cast<const Warning::LargeAOA*>(warnings.find(aoa));
    ASSERT_NE(replaced, nullptr);
    EXPECT_DOUBLE_EQ(replaced->aoa(), 0.5);
    EXPECT_EQ(replaced, stored);  // replaced in place, not re-added
}

TEST(MessageSet, AddWithSourcesLeavesTheConstantAlone)
{
    WarningSet warnings;
    EXPECT_TRUE(warnings.add(Warning::kAirframeGap, MessageSources{source("bt-1", "Body tube"),
                                                                   source("nc-1", "Nose cone")}));
    EXPECT_TRUE(Warning::kAirframeGap.sources().empty());
    ASSERT_EQ(warnings.size(), 1U);
    EXPECT_EQ(warnings.begin()->sources(),
              (MessageSources{source("bt-1", "Body tube"), source("nc-1", "Nose cone")}));
    EXPECT_EQ(warnings.begin()->toString(),
              "Gap in rocket airframe:  \"Body tube\", \"Nose cone\"");
    // The same warning with other sources, or none, is another kind.
    EXPECT_TRUE(warnings.add(Warning::kAirframeGap, MessageSources{source("fs-1", "Fin set")}));
    EXPECT_TRUE(warnings.add(Warning::kAirframeGap));
    EXPECT_FALSE(warnings.add(Warning::kAirframeGap, MessageSources{source("fs-1", "Fin set")}));
    EXPECT_EQ(warnings.size(), 3U);
}

TEST(MessageSet, SourcesAreComparedByComponentIdNotName)
{
    WarningSet warnings;
    // Two fin sets with the default name: two warnings, as in Java (RocketComponent.equals()
    // compares ids).
    EXPECT_TRUE(warnings.add(Warning::kThickFin, MessageSources{source("fs-1", "Fin set")}));
    EXPECT_TRUE(warnings.add(Warning::kThickFin, MessageSources{source("fs-2", "Fin set")}));
    EXPECT_EQ(warnings.size(), 2U);
    // The same component under another name is the same warning.
    EXPECT_FALSE(warnings.add(Warning::kThickFin, MessageSources{source("fs-1", "Fins")}));
    EXPECT_EQ(warnings.size(), 2U);
    Warning::Other probe = Warning::kThickFin;
    probe.setSources(MessageSources{source("fs-1", "Fins")});
    EXPECT_TRUE(warnings.contains(probe));
    EXPECT_NE(warnings.find(probe), nullptr);
    probe.setSources(MessageSources{source("fs-3", "Fin set")});
    EXPECT_FALSE(warnings.contains(probe));
    EXPECT_FALSE(warnings.remove(probe));
    probe.setSources(MessageSources{source("fs-2", "Whatever")});
    EXPECT_TRUE(warnings.remove(probe));
    EXPECT_EQ(warnings.toString(),
              "Messages[Thick fins may not simulate accurately:  \"Fin set\"]");
}

TEST(MessageSet, AddWithDiscriminatorBuildsATextMessage)
{
    WarningSet warnings;
    EXPECT_TRUE(warnings.add(Warning::kEmptyBranch, "Sustainer"));
    const Warning::Other expected =
        Warning::fromString("Simulation branch contains no data:  \"Sustainer\"");
    const Warning* stored = warnings.find(expected);
    ASSERT_NE(stored, nullptr);
    // Java goes through add(String), so the priority is the default, not kEmptyBranch's HIGH.
    EXPECT_EQ(stored->priority(), MessagePriority::NORMAL);
    EXPECT_EQ(stored->typeName(), "Other");
    EXPECT_FALSE(warnings.add(Warning::kEmptyBranch, "Sustainer"));
    EXPECT_TRUE(warnings.add(Warning::kEmptyBranch, "Booster"));
    EXPECT_EQ(warnings.size(), 2U);
}

TEST(MessageSet, RemoveContainsFindAndClear)
{
    ErrorSet errors;
    errors.add("a");
    errors.add("b");
    EXPECT_TRUE(errors.contains(ErrorMessage::Other{"a"}));
    EXPECT_FALSE(errors.contains(ErrorMessage::Other{"c"}));
    EXPECT_TRUE(errors.remove(ErrorMessage::Other{"a"}));
    EXPECT_FALSE(errors.remove(ErrorMessage::Other{"a"}));
    EXPECT_EQ(errors.size(), 1U);
    EXPECT_EQ(errors.find(ErrorMessage::Other{"a"}), nullptr);
    EXPECT_NE(errors.find(ErrorMessage::Other{"b"}), nullptr);
    errors.clear();
    EXPECT_TRUE(errors.empty());
    EXPECT_TRUE(errors.begin() == errors.end());
}

TEST(MessageSet, RemoveWithAnElementOfTheSet)
{
    WarningSet warnings;
    warnings.add(Warning::kSupersonic);
    warnings.add(Warning::kThickFin);
    warnings.add(Warning::kAirframeGap);
    EXPECT_TRUE(warnings.remove(*warnings.begin()));
    ASSERT_EQ(warnings.size(), 2U);
    EXPECT_EQ(warnings.begin()->messageDescription(), Warning::kThickFin.messageDescription());
    const Warning& last = *std::next(warnings.begin());
    EXPECT_TRUE(warnings.remove(last));
    EXPECT_EQ(warnings.size(), 1U);
    EXPECT_FALSE(warnings.contains(Warning::kAirframeGap));
    EXPECT_TRUE(warnings.contains(Warning::kThickFin));
}

TEST(MessageSet, EraseThroughTheIterator)
{
    ErrorSet errors;
    errors.add("a");
    errors.add("b");
    errors.add("c");
    auto it = errors.begin();
    ++it;
    it = errors.erase(it);  // removes "b"
    ASSERT_TRUE(it != errors.end());
    EXPECT_EQ(it->toString(), "c");
    it = errors.erase(it);
    EXPECT_TRUE(it == errors.end());
    EXPECT_EQ(errors.toString(), "Messages[a]");
}

/// Java's `while (it.hasNext()) { it.next(); it.remove(); }`.
void eraseAll(ErrorSet& errors)
{
    while (!errors.empty())
    {
        errors.erase(errors.begin());
    }
}

TEST(MessageSet, EraseLoopEmptiesTheSet)
{
    ErrorSet errors;
    errors.add("a");
    errors.add("b");
    errors.add("c");
    eraseAll(errors);
    EXPECT_TRUE(errors.begin() == errors.end());
    errors.add("kept");
    errors.immute();
    EXPECT_THROW(errors.erase(errors.begin()), BugError);
    EXPECT_EQ(errors.size(), 1U);
}

TEST(MessageSet, FilterOutRemovesEveryMessageOfThatType)
{
    WarningSet warnings;
    warnings.add(Warning::kOpenAirframeForward, MessageSources{source("a-1", "A")});
    warnings.add(Warning::kOpenAirframeForward, MessageSources{source("b-1", "B")});
    warnings.add(Warning::kSupersonic);
    warnings.add(Warning::LargeAOA{0.3});
    ASSERT_EQ(warnings.size(), 4U);
    // Java compares classes, so every text-only warning goes, kSupersonic included.
    warnings.filterOut(Warning::kOpenAirframeForward);
    ASSERT_EQ(warnings.size(), 1U);
    EXPECT_EQ(warnings.begin()->typeName(), "LargeAOA");
    warnings.filterOut(Warning::kSupersonic);  // nothing of that type left: no change
    EXPECT_EQ(warnings.size(), 1U);
}

TEST(MessageSet, FilterOutWithAnElementOfTheSet)
{
    WarningSet warnings;
    warnings.add(Warning::kSupersonic);
    warnings.add(Warning::LargeAOA{0.3});
    warnings.add(Warning::kThickFin);
    ASSERT_EQ(warnings.size(), 3U);
    // The filter is the first element, which is destroyed while the later ones are still being
    // looked at; the type has to be taken before the erase starts.
    warnings.filterOut(*warnings.begin());
    ASSERT_EQ(warnings.size(), 1U);
    EXPECT_EQ(warnings.begin()->typeName(), "LargeAOA");
    warnings.filterOut(*warnings.begin());
    EXPECT_TRUE(warnings.empty());
}

TEST(MessageSet, FindByIdGivesTheStoredMessageForPatching)
{
    WarningSet                       warnings;
    const Warning::EventAfterLanding event{"Apogee"};
    warnings.add(event);
    Warning* found = warnings.findById(event.id());
    ASSERT_NE(found, nullptr);
    auto* typed = dynamic_cast<Warning::EventAfterLanding*>(found);
    ASSERT_NE(typed, nullptr);
    typed->setEventType("Ejection charge");  // what the .ork loader does once it knows the event
    EXPECT_EQ(warnings.begin()->messageDescription(),
              "Flight Event occurred after landing: Ejection charge");
    EXPECT_EQ(warnings.findById(Uuid::random()), nullptr);
    const WarningSet& constSet = warnings;
    EXPECT_EQ(constSet.findById(event.id()), found);
}

TEST(MessageSet, CountsAndListsByPriority)
{
    WarningSet warnings;
    warnings.add(Warning::kThickFin);          // LOW
    warnings.add(Warning::kSupersonic);        // NORMAL
    warnings.add(Warning::kNoRecoveryDevice);  // HIGH
    warnings.add(Warning::kEarlySeparation);   // HIGH
    EXPECT_EQ(warnings.countWithPriority(MessagePriority::LOW), 1U);
    EXPECT_EQ(warnings.countWithPriority(MessagePriority::NORMAL), 1U);
    EXPECT_EQ(warnings.countWithPriority(MessagePriority::HIGH), 2U);
    const std::vector<const Warning*> high =
        std::as_const(warnings).messagesWithPriority(MessagePriority::HIGH);
    ASSERT_EQ(high.size(), 2U);
    EXPECT_EQ(high[0]->messageDescription(), Warning::kNoRecoveryDevice.messageDescription());
    EXPECT_EQ(high[1]->messageDescription(), Warning::kEarlySeparation.messageDescription());
    EXPECT_EQ(warnings.messagesWithPriority(MessagePriority::LOW).size(), 1U);
}

TEST(MessageSet, MessagesWithPriorityCanBeModifiedInPlace)
{
    // Java hands out the stored objects, which the GUI may edit; a const set hands out const ones.
    WarningSet warnings;
    warnings.add(Warning::kNoRecoveryDevice);  // HIGH
    warnings.add(Warning::kThickFin);          // LOW
    const std::vector<Warning*> high = warnings.messagesWithPriority(MessagePriority::HIGH);
    ASSERT_EQ(high.size(), 1U);
    high.front()->setSources(MessageSources{source("bt-1", "Body tube")});
    const WarningSet&                 constSet = warnings;
    const std::vector<const Warning*> seen = constSet.messagesWithPriority(MessagePriority::HIGH);
    ASSERT_EQ(seen.size(), 1U);
    EXPECT_EQ(seen.front(), high.front());
    EXPECT_EQ(seen.front()->toString(),
              Warning::kNoRecoveryDevice.messageDescription() + ":  \"Body tube\"");
    EXPECT_TRUE(constSet.messagesWithPriority(MessagePriority::NORMAL).empty());
}

TEST(MessageSet, ImmuteBlocksAdding)
{
    ErrorSet errors;
    errors.add("kept");
    EXPECT_TRUE(errors.isMutable());
    errors.immute();
    errors.immute();  // repeated calls do nothing
    EXPECT_FALSE(errors.isMutable());
    ErrorSet more;
    more.add("more");
    EXPECT_THROW(errors.add("more"), BugError);
    EXPECT_THROW(errors.add(ErrorMessage::Other{"more"}), BugError);
    EXPECT_THROW(errors.add(ErrorMessage::Other{"more"}, MessageSources{source("x-1", "x")}),
                 BugError);
    EXPECT_THROW(errors.add(ErrorMessage::Other{"more"}, "discriminator"), BugError);
    EXPECT_THROW(errors.addAll(more), BugError);
    EXPECT_EQ(errors.size(), 1U);
    EXPECT_TRUE(errors.contains(ErrorMessage::Other{"kept"}));
}

TEST(MessageSet, ImmuteBlocksRemoving)
{
    ErrorSet errors;
    errors.add("kept");
    errors.immute();
    EXPECT_THROW(errors.remove(ErrorMessage::Other{"kept"}), BugError);
    EXPECT_THROW(errors.erase(errors.begin()), BugError);
    EXPECT_THROW(errors.filterOut(ErrorMessage::Other{"kept"}), BugError);
    EXPECT_THROW(errors.clear(), BugError);
    EXPECT_EQ(errors.size(), 1U);
    EXPECT_TRUE(errors.contains(ErrorMessage::Other{"kept"}));
}

TEST(MessageSet, ImmutedSetReturnsQuietlyWhenNothingWouldChange)
{
    // Java reaches Mutable.check() through Iterator.remove() and add(), so a call that finds
    // nothing to do returns without it.
    ErrorSet empty;
    empty.immute();
    EXPECT_NO_THROW(empty.clear());
    EXPECT_FALSE(empty.remove(ErrorMessage::Other{"absent"}));
    EXPECT_NO_THROW(empty.filterOut(ErrorMessage::Other{"absent"}));
    EXPECT_FALSE(empty.addAll(ErrorSet{}));
    EXPECT_TRUE(empty.empty());
    ErrorSet kept;
    kept.add("kept");
    kept.immute();
    EXPECT_FALSE(kept.remove(ErrorMessage::Other{"absent"}));
    EXPECT_FALSE(kept.addAll(ErrorSet{}));
    // Java's add() checks before it looks, so adding the set to itself is not quiet.
    EXPECT_THROW(kept.addAll(kept), BugError);
    EXPECT_EQ(kept.size(), 1U);
    WarningSet aoaOnly;
    aoaOnly.add(Warning::LargeAOA{0.1});
    aoaOnly.immute();
    EXPECT_NO_THROW(aoaOnly.filterOut(Warning::kSupersonic));  // no Other in the set
    EXPECT_THROW(aoaOnly.filterOut(Warning::LargeAOA{0.9}), BugError);
    EXPECT_EQ(aoaOnly.size(), 1U);
}

TEST(MessageSet, CopyIsDeepAndKeepsImmutability)
{
    WarningSet original;
    original.add(Warning::LargeAOA{0.1});
    original.add(Warning::kSupersonic);
    WarningSet copy = original;
    EXPECT_EQ(copy.size(), 2U);
    EXPECT_TRUE(copy == original);
    EXPECT_NE(&*copy.begin(), &*original.begin());
    // Replacing in the copy does not touch the original.
    copy.add(Warning::LargeAOA{0.9});
    EXPECT_DOUBLE_EQ(dynamic_cast<const Warning::LargeAOA&>(*original.begin()).aoa(), 0.1);
    EXPECT_DOUBLE_EQ(dynamic_cast<const Warning::LargeAOA&>(*copy.begin()).aoa(), 0.9);
    WarningSet assigned;
    assigned.add(Warning::kThickFin);
    assigned = original;
    EXPECT_TRUE(assigned == original);
    EXPECT_FALSE(assigned.contains(Warning::kThickFin));
    original.immute();
    const WarningSet immutableCopy = original;
    EXPECT_FALSE(immutableCopy.isMutable());
    EXPECT_TRUE(assigned.isMutable());
}

TEST(MessageSet, MoveKeepsTheMessagesThemselves)
{
    WarningSet original;
    original.add(Warning::LargeAOA{0.1});
    original.add(Warning::kSupersonic);
    const Warning* first = &*original.begin();
    WarningSet     moved = std::move(original);
    ASSERT_EQ(moved.size(), 2U);
    EXPECT_EQ(&*moved.begin(), first);  // the messages moved, they were not copied
    EXPECT_TRUE(moved.isMutable());
    WarningSet assigned;
    assigned.add(Warning::kThickFin);
    assigned = std::move(moved);
    ASSERT_EQ(assigned.size(), 2U);
    EXPECT_EQ(&*assigned.begin(), first);
    EXPECT_FALSE(assigned.contains(Warning::kThickFin));
    // A move keeps immutability, like a copy.
    assigned.immute();
    const WarningSet immutedMove = std::move(assigned);
    EXPECT_FALSE(immutedMove.isMutable());
    EXPECT_EQ(immutedMove.size(), 2U);
}

TEST(MessageSet, AssignmentReplacesTheWholeSetImmutabilityIncluded)
{
    // Assignment is the C++ counterpart of reassigning the reference in Java, not a mutation of
    // the set: it overwrites an immuted set and carries over the source's mutability.
    WarningSet immuted;
    immuted.add(Warning::kSupersonic);
    immuted.immute();
    WarningSet other;
    other.add(Warning::kThickFin);
    immuted = other;
    EXPECT_TRUE(immuted.isMutable());
    EXPECT_TRUE(immuted == other);
    EXPECT_FALSE(immuted.contains(Warning::kSupersonic));
    EXPECT_TRUE(immuted.add(Warning::kAirframeGap));
    immuted.immute();
    WarningSet movedIn;
    movedIn.add(Warning::kEmptyBranch);
    immuted = std::move(movedIn);
    EXPECT_TRUE(immuted.isMutable());
    EXPECT_EQ(immuted.size(), 1U);
    EXPECT_TRUE(immuted.contains(Warning::kEmptyBranch));
    // ... and assigning an immuted set makes the target immutable.
    WarningSet target;
    target.add(Warning::kAirframeGap);
    other.immute();
    target = other;
    EXPECT_FALSE(target.isMutable());
    EXPECT_TRUE(target == other);
    EXPECT_THROW(target.add(Warning::kThickFin), BugError);
    EXPECT_FALSE(other.isMutable());  // the source is left alone
}

TEST(MessageSet, AddAllAndSetEquality)
{
    WarningSet a;
    a.add(Warning::kThickFin);
    a.add(Warning::LargeAOA{0.2});
    WarningSet b;
    b.add(Warning::LargeAOA{0.4});
    b.add(Warning::kSupersonic);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a.addAll(b));  // grows by kSupersonic; the LargeAOA is replaced by 0.4
    EXPECT_EQ(a.size(), 3U);
    EXPECT_FALSE(a.addAll(b));
    const ModId beforeSelfAddAll = a.modId();
    EXPECT_FALSE(a.addAll(a));  // Java add()s every element to itself, so an id is drawn
    EXPECT_GT(a.modId().toInt(), beforeSelfAddAll.toInt());
    EXPECT_EQ(a.size(), 3U);
    const auto* aoa = dynamic_cast<const Warning::LargeAOA*>(a.find(Warning::LargeAOA{0.0}));
    ASSERT_NE(aoa, nullptr);
    EXPECT_DOUBLE_EQ(aoa->aoa(), 0.4);
    WarningSet c;  // the same messages in another order
    c.add(Warning::kSupersonic);
    c.add(Warning::LargeAOA{0.4});
    c.add(Warning::kThickFin);
    EXPECT_TRUE(a == c);
    EXPECT_TRUE(a != b);
}

TEST(MessageSet, SetEqualityWithEventAfterLandingIsById)
{
    WarningSet a;
    a.add(Warning::EventAfterLanding{"Apogee"});
    WarningSet b;
    b.add(Warning::EventAfterLanding{"Apogee"});
    EXPECT_FALSE(a == b);  // two events, two ids
    const WarningSet copy = a;
    EXPECT_TRUE(copy == a);  // the copy carries the id
    EXPECT_NE(&*copy.begin(), &*a.begin());
    EXPECT_EQ(copy.begin()->id(), a.begin()->id());
    WarningSet c;
    c.add(*a.begin());  // adding the stored warning again keeps its id, so the sets are equal
    EXPECT_TRUE(c == a);
    c.add(Warning::EventAfterLanding{"Apogee"});
    EXPECT_FALSE(c == a);
}

TEST(MessageSet, ToStringJoinsMessagesWithCommas)
{
    ErrorSet errors;
    EXPECT_EQ(errors.toString(), "Messages[]");
    errors.add("one");
    errors.add(ErrorMessage::Other{"two"}, MessageSources{source("nc-1", "Nose cone")});
    EXPECT_EQ(errors.toString(), "Messages[one,two:  \"Nose cone\"]");
}

TEST(MessageSet, IteratesAsAForwardRange)
{
    static_assert(std::forward_iterator<WarningSet::ConstIterator>);
    WarningSet warnings;
    warnings.add(Warning::kThickFin);
    warnings.add(Warning::kSupersonic);
    EXPECT_EQ(std::distance(warnings.begin(), warnings.end()), 2);
    auto it = warnings.begin();
    EXPECT_EQ(it->messageDescription(), Warning::kThickFin.messageDescription());
    const auto old = it++;
    EXPECT_TRUE(old == warnings.begin());
    EXPECT_EQ((*it).messageDescription(), Warning::kSupersonic.messageDescription());
    ++it;
    EXPECT_TRUE(it == warnings.end());
}

TEST(MessageSet, ModIdIsRedrawnByEveryAdd)
{
    // Java: Monitorable.getModID(); the simulation's flight data reads it to refresh its view of
    // the warnings only when something changed.
    WarningSet warnings;
    EXPECT_EQ(warnings.modId(), ModId::zero());
    EXPECT_TRUE(warnings.add(Warning::kThickFin));
    const ModId afterFirst = warnings.modId();
    EXPECT_GT(afterFirst.toInt(), 0);
    // Java draws a new id in add() before it looks for a duplicate, so an add that changes
    // nothing counts too ...
    EXPECT_FALSE(warnings.add(Warning::kThickFin));
    const ModId afterDuplicate = warnings.modId();
    EXPECT_GT(afterDuplicate.toInt(), afterFirst.toInt());
    // ... as does one that replaces the contents of a stored message.
    EXPECT_TRUE(warnings.add(Warning::LargeAOA{0.1}));
    const ModId afterAoa = warnings.modId();
    EXPECT_GT(afterAoa.toInt(), afterDuplicate.toInt());
    EXPECT_FALSE(warnings.add(Warning::LargeAOA{0.5}));
    EXPECT_GT(warnings.modId().toInt(), afterAoa.toInt());
    // The other adds go through the same path.
    ModId before = warnings.modId();
    EXPECT_TRUE(warnings.add("text"));
    EXPECT_GT(warnings.modId().toInt(), before.toInt());
    before = warnings.modId();
    EXPECT_TRUE(warnings.add(Warning::kAirframeGap, MessageSources{source("bt-1", "Body tube")}));
    EXPECT_GT(warnings.modId().toInt(), before.toInt());
    before = warnings.modId();
    EXPECT_TRUE(warnings.add(Warning::kEmptyBranch, "Sustainer"));
    EXPECT_GT(warnings.modId().toInt(), before.toInt());
    before = warnings.modId();
    WarningSet more;
    more.add(Warning::kSupersonic);
    EXPECT_TRUE(warnings.addAll(more));
    EXPECT_GT(warnings.modId().toInt(), before.toInt());
    before = warnings.modId();
    EXPECT_FALSE(warnings.addAll(warnings));  // the same, element by element, onto itself
    EXPECT_GT(warnings.modId().toInt(), before.toInt());
}

TEST(MessageSet, RefusedChangeNamesWhereTheSetWasImmuted)
{
    // Java: Mutable.check() throws "Object has been made immutable at ..." with the stack trace
    // of the immute() call; the port names the file and line of that call, and keeps the first
    // one when immute() is called again.
    ErrorSet errors;
    errors.add("kept");
    const std::source_location first = std::source_location::current();
    errors.immute();
    errors.immute();
    const std::string expected = std::format("MessageSetTests.cpp:{}", first.line() + 1);
    const std::string later    = std::format("MessageSetTests.cpp:{}", first.line() + 2);
    const std::string what     = bugMessage([&errors] { errors.add("more"); });
    EXPECT_NE(what.find("MessageSet has been made immutable at "), std::string::npos) << what;
    EXPECT_NE(what.find(expected), std::string::npos) << what;
    EXPECT_EQ(what.find(later), std::string::npos) << what;
    // A copy, and a set assigned from it, are immuted at the same place as the source.
    const ErrorSet copy = errors;
    ErrorSet       assigned;
    assigned = copy;
    EXPECT_NE(bugMessage([&assigned] { assigned.clear(); }).find(expected), std::string::npos);
    EXPECT_NE(bugMessage([&copy] {
                  ErrorSet moved = copy;
                  moved.add("x");
              }).find(expected),
              std::string::npos);
}

TEST(MessageSet, ModIdIsRedrawnWhenAMessageIsRemoved)
{
    // Java draws no id here (Iterator.remove() bypasses add()); the port keeps Monitorable's
    // contract instead, so that a reader notices a removal too.
    ErrorSet errors;
    errors.add("a");
    errors.add("b");
    errors.add("c");
    errors.add("d");
    ModId before = errors.modId();
    EXPECT_TRUE(errors.remove(ErrorMessage::Other{"a"}));
    EXPECT_GT(errors.modId().toInt(), before.toInt());
    before = errors.modId();
    errors.erase(errors.begin());
    EXPECT_GT(errors.modId().toInt(), before.toInt());
    before = errors.modId();
    errors.filterOut(ErrorMessage::Other{"x"});  // every Other goes, i.e. "c" and "d"
    EXPECT_GT(errors.modId().toInt(), before.toInt());
    EXPECT_TRUE(errors.empty());
    errors.add("e");
    before = errors.modId();
    errors.clear();
    EXPECT_GT(errors.modId().toInt(), before.toInt());
}

TEST(MessageSet, ModIdStaysWhenNothingChanges)
{
    ErrorSet errors;
    errors.add("kept");
    const ModId before = errors.modId();
    EXPECT_FALSE(errors.remove(ErrorMessage::Other{"absent"}));
    EXPECT_FALSE(errors.addAll(ErrorSet{}));
    // Reading is not a change.
    EXPECT_TRUE(errors.contains(ErrorMessage::Other{"kept"}));
    EXPECT_NE(errors.find(ErrorMessage::Other{"kept"}), nullptr);
    EXPECT_EQ(errors.toString(), "Messages[kept]");
    EXPECT_EQ(errors.modId(), before);
    // A refused change of an immuted set is not one either: Java checks before it draws.
    errors.immute();
    EXPECT_THROW(errors.add("more"), BugError);
    EXPECT_THROW(errors.clear(), BugError);
    EXPECT_EQ(errors.modId(), before);
    WarningSet aoaOnly;
    aoaOnly.add(Warning::LargeAOA{0.1});
    const ModId aoaBefore = aoaOnly.modId();
    aoaOnly.filterOut(Warning::kSupersonic);  // no Other in the set
    EXPECT_EQ(aoaOnly.modId(), aoaBefore);
    ErrorSet empty;
    empty.clear();
    EXPECT_EQ(empty.modId(), ModId::zero());
}

TEST(MessageSet, ModIdOfCopiesAndAssignedSets)
{
    WarningSet original;
    original.add(Warning::kThickFin);
    const ModId originalId = original.modId();
    // A copy or a move carries the id along with the contents.
    const WarningSet copy = original;
    EXPECT_EQ(copy.modId(), originalId);
    WarningSet moved = std::move(original);
    EXPECT_EQ(moved.modId(), originalId);
    // Assignment draws a fresh one, so that a set's id never goes back to an earlier value, even
    // when its earlier contents are restored from a copy.
    moved.add(Warning::kSupersonic);
    const ModId changedId = moved.modId();
    EXPECT_GT(changedId.toInt(), originalId.toInt());
    moved = copy;
    EXPECT_TRUE(moved == copy);
    EXPECT_GT(moved.modId().toInt(), changedId.toInt());
    EXPECT_NE(moved.modId(), copy.modId());
    const ModId assignedId = moved.modId();
    WarningSet  other;
    other.add(Warning::kAirframeGap);
    moved = std::move(other);
    EXPECT_GT(moved.modId().toInt(), assignedId.toInt());
    EXPECT_TRUE(moved.contains(Warning::kAirframeGap));
}

// ------------------------------------------------------------------------------- capacity
//
// QtRocket's own: OpenRocket's sets grow as long as they are added to. The .ork loader gives
// the set of a load and the set of each stored flight a capacity.

/// The message a full set of these tests adds once.
[[nodiscard]] Warning::Other overflowWarning()
{
    return Warning::fromString("more were dropped");
}

/// The texts of @p warnings, in order, each followed by a line feed.
[[nodiscard]] std::string textsOf(const WarningSet& warnings)
{
    std::string texts;
    for (const Warning& warning : warnings)
    {
        texts += warning.toString() + "\n";
    }
    return texts;
}

/// Gives @p warnings the warnings "w0" to "w<count - 1>".
void addNumbered(WarningSet& warnings, int count)
{
    for (int i = 0; i < count; ++i)
    {
        warnings.add(std::format("w{}", i));
    }
}

/// A set of the capacity @p capacity that was given the warnings "w0" to "w<count - 1>".
[[nodiscard]] WarningSet setOfCapacity(std::size_t capacity, int count)
{
    WarningSet warnings;
    warnings.setCapacity(capacity, overflowWarning());
    addNumbered(warnings, count);
    return warnings;
}

TEST(MessageSet, HasNoCapacityUntilItIsGivenOne)
{
    WarningSet warnings;
    EXPECT_EQ(warnings.capacity(), std::nullopt);
    addNumbered(warnings, 5000);
    EXPECT_EQ(warnings.size(), 5000U);

    warnings.setCapacity(3, overflowWarning());
    EXPECT_EQ(warnings.capacity(), std::optional<std::size_t>(3));
    // What it holds stays, and giving it a capacity is no change of the contents.
    EXPECT_EQ(warnings.size(), 5000U);
    EXPECT_FALSE(warnings.contains(overflowWarning()));
}

TEST(MessageSet, KeepsAsManyMessagesAsItsCapacityAndSaysOnceThatMoreWereDropped)
{
    // At the capacity: every message is there and nothing else.
    WarningSet warnings = setOfCapacity(3, 3);
    EXPECT_EQ(textsOf(warnings), "w0\nw1\nw2\n");

    // One more: it is dropped, and the overflow message is added in its place.
    EXPECT_FALSE(warnings.add("w3"));
    EXPECT_EQ(textsOf(warnings), "w0\nw1\nw2\nmore were dropped\n");
    EXPECT_FALSE(warnings.contains(Warning::fromString("w3")));

    // And every later one is dropped without a word.
    EXPECT_FALSE(warnings.add("w4"));
    EXPECT_FALSE(warnings.add(Warning::LargeAOA{0.3}));
    EXPECT_FALSE(warnings.add(Warning::kThickFin, {source("fs-1", "Fins")}));
    EXPECT_EQ(textsOf(warnings), "w0\nw1\nw2\nmore were dropped\n");
    EXPECT_EQ(warnings.size(), 4U);
}

TEST(MessageSet, AFullSetStillReplacesTheContentsOfAMessageItHolds)
{
    WarningSet warnings;
    warnings.setCapacity(2, overflowWarning());
    EXPECT_TRUE(warnings.add(Warning::LargeAOA{0.1}));
    EXPECT_TRUE(warnings.add("w1"));
    EXPECT_FALSE(warnings.add("w2"));
    ASSERT_EQ(warnings.size(), 3U);

    // A message of a kind the set holds is not one more: it is merged as ever.
    EXPECT_FALSE(warnings.add(Warning::LargeAOA{0.9}));
    EXPECT_FALSE(warnings.add("w1"));
    EXPECT_FALSE(warnings.add(overflowWarning()));
    ASSERT_EQ(warnings.size(), 3U);
    EXPECT_DOUBLE_EQ(dynamic_cast<const Warning::LargeAOA&>(*warnings.begin()).aoa(), 0.9);
}

TEST(MessageSet, AddingToAFullSetDrawsAModificationIdAsEveryAddDoes)
{
    WarningSet  warnings = setOfCapacity(1, 2);
    const ModId full     = warnings.modId();
    EXPECT_FALSE(warnings.add("another"));
    EXPECT_NE(warnings.modId(), full);
    // Giving a set a capacity draws none.
    const ModId before = warnings.modId();
    warnings.setCapacity(7, overflowWarning());
    EXPECT_EQ(warnings.modId(), before);
}

// The overflow message is none of the messages the capacity counts, also when the caller
// added it (a saved flight has it among its stored warnings): the set takes as many others as
// its capacity says, and does not add the message a second time.
TEST(MessageSet, AnOverflowMessageTheSetHoldsAlreadyIsNotCountedAndNotAddedTwice)
{
    WarningSet warnings;
    warnings.setCapacity(2, overflowWarning());
    EXPECT_TRUE(warnings.add(overflowWarning()));
    EXPECT_TRUE(warnings.add("w1"));
    EXPECT_TRUE(warnings.add("w2"));
    EXPECT_FALSE(warnings.add("w3"));
    EXPECT_FALSE(warnings.add("w4"));
    EXPECT_EQ(textsOf(warnings), "more were dropped\nw1\nw2\n");
}

// A full set takes its overflow message from a caller too, as the message it is given: the
// stored warnings of a flight that was cut off end with that text, and the warning a file
// brings keeps its id, by which the events of the flight find it.
TEST(MessageSet, AFullSetTakesItsOverflowMessageFromACallerAsItIsGiven)
{
    WarningSet           warnings = setOfCapacity(2, 2);
    const Warning::Other given    = overflowWarning();
    EXPECT_TRUE(warnings.add(given));
    EXPECT_EQ(textsOf(warnings), "w0\nw1\nmore were dropped\n");
    const Warning* stored = warnings.find(given);
    ASSERT_NE(stored, nullptr);
    EXPECT_EQ(stored->id(), given.id());
    EXPECT_EQ(warnings.findById(given.id()), stored);

    // The set is as full as before, and does not say twice that it is.
    EXPECT_FALSE(warnings.add("w2"));
    EXPECT_FALSE(warnings.add(overflowWarning()));
    EXPECT_EQ(textsOf(warnings), "w0\nw1\nmore were dropped\n");
    EXPECT_EQ(warnings.find(given), stored);
}

TEST(MessageSet, ASetWithRoomAgainTakesMessagesAgain)
{
    WarningSet warnings = setOfCapacity(2, 3);
    ASSERT_EQ(textsOf(warnings), "w0\nw1\nmore were dropped\n");

    // One fewer: room for one, although the set still holds two messages, one of them the
    // overflow message. The new one stands behind that.
    EXPECT_TRUE(warnings.remove(Warning::fromString("w0")));
    EXPECT_TRUE(warnings.add("w5"));
    EXPECT_FALSE(warnings.add("w6"));
    EXPECT_EQ(textsOf(warnings), "w1\nmore were dropped\nw5\n");

    // Two fewer: room for two.
    EXPECT_TRUE(warnings.remove(Warning::fromString("w1")));
    EXPECT_TRUE(warnings.remove(Warning::fromString("w5")));
    EXPECT_TRUE(warnings.add("w7"));
    EXPECT_TRUE(warnings.add("w8"));
    EXPECT_FALSE(warnings.add("w9"));
    EXPECT_EQ(textsOf(warnings), "more were dropped\nw7\nw8\n");

    // The overflow message removed: the set is full without it, and says so again.
    EXPECT_TRUE(warnings.remove(overflowWarning()));
    EXPECT_EQ(textsOf(warnings), "w7\nw8\n");
    EXPECT_FALSE(warnings.add("w10"));
    EXPECT_EQ(textsOf(warnings), "w7\nw8\nmore were dropped\n");

    // Cleared: the capacity stays, and the set says again that it dropped something.
    warnings.clear();
    EXPECT_EQ(warnings.capacity(), std::optional<std::size_t>(2));
    EXPECT_TRUE(warnings.add("a"));
    EXPECT_TRUE(warnings.add("b"));
    EXPECT_FALSE(warnings.add("c"));
    EXPECT_EQ(textsOf(warnings), "a\nb\nmore were dropped\n");
}

// A capacity can be taken away again: who merges other warnings into a set that a load
// bounded, or into a copy of it, lifts the bound first.
TEST(MessageSet, ACapacityCanBeLifted)
{
    WarningSet warnings = setOfCapacity(2, 3);
    ASSERT_EQ(textsOf(warnings), "w0\nw1\nmore were dropped\n");
    const ModId before = warnings.modId();

    warnings.clearCapacity();

    // No change of the contents: what the set holds stays, the overflow message too.
    EXPECT_EQ(warnings.capacity(), std::nullopt);
    EXPECT_EQ(warnings.modId(), before);
    EXPECT_EQ(textsOf(warnings), "w0\nw1\nmore were dropped\n");
    // And it grows without bound again.
    addNumbered(warnings, 50);
    EXPECT_EQ(warnings.size(), 51U);
    EXPECT_TRUE(warnings.contains(Warning::fromString("w49")));

    // A copy of a bounded set is bounded until it is lifted; the original stays as it is.
    const WarningSet bounded = setOfCapacity(1, 1);
    WarningSet       copy    = bounded;
    EXPECT_FALSE(copy.add("x"));
    copy.clearCapacity();
    EXPECT_TRUE(copy.add("x"));
    EXPECT_EQ(textsOf(copy), "w0\nmore were dropped\nx\n");
    EXPECT_EQ(bounded.capacity(), std::optional<std::size_t>(1));

    // Lifting what was never there, and on an immuted set, is allowed.
    WarningSet plain;
    plain.clearCapacity();
    EXPECT_EQ(plain.capacity(), std::nullopt);
    WarningSet frozen = setOfCapacity(1, 1);
    frozen.immute();
    frozen.clearCapacity();
    EXPECT_EQ(frozen.capacity(), std::nullopt);
}

// A capacity given a second time, with another overflow message: the first message stays in the
// set as a message like any other, and the second is what a full set adds.
TEST(MessageSet, ASecondCapacityHasItsOwnOverflowMessage)
{
    WarningSet warnings = setOfCapacity(1, 2);
    ASSERT_EQ(textsOf(warnings), "w0\nmore were dropped\n");
    warnings.setCapacity(2, Warning::fromString("still more were dropped"));
    // Two messages, neither of them the new overflow message: the set is full.
    EXPECT_FALSE(warnings.add("w2"));
    EXPECT_EQ(textsOf(warnings), "w0\nmore were dropped\nstill more were dropped\n");
}

TEST(MessageSet, ACapacityOfNoneKeepsNothingButTheOverflowMessage)
{
    WarningSet warnings;
    warnings.setCapacity(0, overflowWarning());
    EXPECT_FALSE(warnings.add("w0"));
    EXPECT_FALSE(warnings.add("w1"));
    EXPECT_EQ(textsOf(warnings), "more were dropped\n");
}

TEST(MessageSet, AddAllIsHeldToTheCapacity)
{
    WarningSet many;
    addNumbered(many, 10);
    WarningSet bounded;
    bounded.setCapacity(4, overflowWarning());
    EXPECT_TRUE(bounded.addAll(many));
    EXPECT_EQ(textsOf(bounded), "w0\nw1\nw2\nw3\nmore were dropped\n");
    // Nothing of a second helping is added.
    EXPECT_FALSE(bounded.addAll(many));
    EXPECT_EQ(bounded.size(), 5U);

    // A set without a capacity takes everything of a bounded one, its overflow message too.
    WarningSet unbounded;
    EXPECT_TRUE(unbounded.addAll(bounded));
    EXPECT_EQ(unbounded.size(), 5U);
    EXPECT_EQ(unbounded.capacity(), std::nullopt);
    EXPECT_TRUE(unbounded.add("one more"));
}

TEST(MessageSet, ACopyAndAMoveKeepTheCapacityAndAnAssignmentTakesTheSourcesOne)
{
    const WarningSet original = setOfCapacity(2, 2);
    WarningSet       copy     = original;
    EXPECT_EQ(copy.capacity(), std::optional<std::size_t>(2));
    EXPECT_FALSE(copy.add("w2"));
    EXPECT_EQ(textsOf(copy), "w0\nw1\nmore were dropped\n");
    EXPECT_EQ(textsOf(original), "w0\nw1\n") << "the copy is a set of its own";

    WarningSet moved = std::move(copy);
    EXPECT_EQ(moved.capacity(), std::optional<std::size_t>(2));
    EXPECT_FALSE(moved.add("w3"));
    EXPECT_EQ(moved.size(), 3U);

    // Assignment replaces the whole set, the capacity included, both ways.
    WarningSet unbounded;
    unbounded.add("u0");
    unbounded.add("u1");
    unbounded.add("u2");
    WarningSet target = original;
    target            = unbounded;
    EXPECT_EQ(target.capacity(), std::nullopt);
    EXPECT_TRUE(target.add("u3"));
    target = original;
    EXPECT_EQ(target.capacity(), std::optional<std::size_t>(2));
    EXPECT_FALSE(target.add("w2"));
    EXPECT_EQ(textsOf(target), "w0\nw1\nmore were dropped\n");
    target = setOfCapacity(1, 1);
    EXPECT_EQ(target.capacity(), std::optional<std::size_t>(1));
    EXPECT_FALSE(target.add("w1"));
    EXPECT_EQ(textsOf(target), "w0\nmore were dropped\n");
}

TEST(MessageSet, AnErrorSetHasACapacityToo)
{
    ErrorSet errors;
    errors.setCapacity(1, ErrorMessage::fromString("more errors were dropped"));
    EXPECT_TRUE(errors.add("first"));
    EXPECT_FALSE(errors.add("second"));
    EXPECT_FALSE(errors.add("third"));
    ASSERT_EQ(errors.size(), 2U);
    EXPECT_EQ(std::next(errors.begin())->toString(), "more errors were dropped");
}

// ---------------------------------------------------------------------------------- hashing
//
// Message::hashCode() (Java: hashCode()) and the set's search by it: QtRocket's own way to find
// a message, which has to find exactly what a search with equals() finds.

// Messages that are equal share their hash, whatever else they hold.
TEST(MessageSet, EqualMessagesShareTheirHash)
{
    EXPECT_EQ(Warning::fromString("a").hashCode(), Warning::fromString("a").hashCode());
    EXPECT_EQ(Warning::LargeAOA{0.1}.hashCode(), Warning::LargeAOA{0.9}.hashCode());
    EXPECT_EQ(Warning::MissingMotor().hashCode(), Warning::MissingMotor().hashCode());
    EXPECT_EQ(ErrorMessage::fromString("a").hashCode(), ErrorMessage::fromString("a").hashCode());
    // The sources, the priority and the id are no part of it: they can change in a set.
    Warning::Other changed = Warning::fromString("a");
    const auto     hash    = changed.hashCode();
    changed.setSources({source("fs-1", "Fins")});
    changed.setPriority(MessagePriority::HIGH);
    EXPECT_EQ(changed.hashCode(), hash);
    // A copy has the hash of its original.
    EXPECT_EQ(Warning::fromString("a").clone()->hashCode(), hash);
    // What tells two messages apart tells their hashes apart as far as a hash can: the type,
    // and for a text-only message the text.
    EXPECT_NE(Warning::fromString("a").hashCode(), Warning::fromString("b").hashCode());
    EXPECT_NE(Warning::fromString("a").hashCode(), ErrorMessage::fromString("a").hashCode());
    EXPECT_NE(Warning::LargeAOA{0.1}.hashCode(), Warning::fromString("a").hashCode());
}

// Messages of one hash that are not equal (two missing motors, which hash their type alone;
// one text with other sources or another priority) are told apart by equals(), as ever.
TEST(MessageSet, MessagesOfOneHashAreStillToldApart)
{
    Warning::MissingMotor first;
    first.setDesignation("D12");
    Warning::MissingMotor second;
    second.setDesignation("C6");
    ASSERT_EQ(first.hashCode(), second.hashCode());

    WarningSet warnings;
    EXPECT_TRUE(warnings.add(first));
    EXPECT_TRUE(warnings.add(second));
    EXPECT_FALSE(warnings.add(second));
    EXPECT_TRUE(warnings.add("text"));
    EXPECT_TRUE(warnings.add(Warning::fromString("text"), {source("fs-1", "Fins")}));
    EXPECT_TRUE(warnings.add(Warning::Other("text", MessagePriority::HIGH)));
    EXPECT_EQ(warnings.size(), 5U);
    ASSERT_NE(warnings.find(second), nullptr);
    EXPECT_EQ(dynamic_cast<const Warning::MissingMotor&>(*warnings.find(second)).designation(),
              std::optional<std::string>("C6"));
    EXPECT_TRUE(warnings.remove(first));
    EXPECT_FALSE(warnings.contains(first));
    EXPECT_TRUE(warnings.contains(second));
}

/// The texts of the warnings "w0" to "w<count - 1>" that @p warnings does not find, or finds
/// although @p expected says otherwise, as "w3 w7 "; "" when every answer is as expected.
[[nodiscard]] std::string wrongFinds(const WarningSet& warnings, int count,
                                     bool (*expected)(int number))
{
    std::string wrong;
    for (int i = 0; i < count; ++i)
    {
        const Warning::Other warning = Warning::fromString(std::format("w{}", i));
        const Warning*       found   = warnings.find(warning);
        const bool           isRight = found != nullptr && found->toString() == warning.toString();
        if (isRight != expected(i))
        {
            wrong += std::format("w{} ", i);
        }
    }
    return wrong;
}

[[nodiscard]] bool isOdd(int number)
{
    return number % 2 != 0;
}

[[nodiscard]] bool isAny(int /*number*/)
{
    return true;
}

[[nodiscard]] bool isNone(int /*number*/)
{
    return false;
}

[[nodiscard]] bool isThree(int number)
{
    return number == 3;
}

[[nodiscard]] bool isNotZero(int number)
{
    return number != 0;
}

[[nodiscard]] bool isNotTwelve(int number)
{
    return number != 12;
}

/// Removes the warnings "w0", "w2", ... below "w<count>" from @p warnings and returns how many
/// of them were there.
[[nodiscard]] int removeEven(WarningSet& warnings, int count)
{
    int removed = 0;
    for (int i = 0; i < count; i += 2)
    {
        removed += warnings.remove(Warning::fromString(std::format("w{}", i))) ? 1 : 0;
    }
    return removed;
}

// Every way a message leaves a set takes its hash with it: what is left is found, each under
// its own text, and what left is not.
TEST(MessageSet, FindsWhatItHoldsAfterEveryKindOfRemoval)
{
    WarningSet warnings;
    addNumbered(warnings, 40);
    warnings.add(Warning::LargeAOA{0.3});
    warnings.add(Warning::kThickFin, {source("fs-1", "Fins")});
    ASSERT_EQ(wrongFinds(warnings, 40, &isAny), "");

    // remove(): every even one.
    EXPECT_EQ(removeEven(warnings, 40), 20);
    EXPECT_EQ(wrongFinds(warnings, 40, &isOdd), "");
    EXPECT_EQ(warnings.size(), 22U);

    // erase(): the first, which is "w1".
    EXPECT_EQ(warnings.erase(warnings.begin())->toString(), "w3");
    EXPECT_FALSE(warnings.contains(Warning::fromString("w1")));
    EXPECT_TRUE(warnings.contains(Warning::fromString("w3")));
    EXPECT_TRUE(warnings.contains(Warning::LargeAOA{0.0}));

    // filterOut(): the text-only ones, which stand around the other.
    warnings.filterOut(Warning::fromString("any"));
    EXPECT_EQ(wrongFinds(warnings, 40, &isNone), "");
    EXPECT_EQ(warnings.size(), 1U);
    EXPECT_TRUE(warnings.contains(Warning::LargeAOA{0.0}));
    EXPECT_TRUE(warnings.add("w3"));
    EXPECT_FALSE(warnings.add("w3"));

    // clear(), and what is added afterwards.
    warnings.clear();
    EXPECT_FALSE(warnings.contains(Warning::LargeAOA{0.0}));
    EXPECT_TRUE(warnings.add("w3"));
    EXPECT_EQ(wrongFinds(warnings, 40, &isThree), "");
}

// A copy, a move and an assignment take the hashes with the messages.
TEST(MessageSet, ACopyAMoveAndAnAssignmentFindWhatTheOriginalHeld)
{
    WarningSet original;
    addNumbered(original, 12);
    WarningSet copy = original;
    EXPECT_EQ(wrongFinds(copy, 12, &isAny), "");
    EXPECT_FALSE(copy.add("w7"));
    EXPECT_TRUE(copy.add("w12"));

    WarningSet moved = std::move(copy);
    EXPECT_EQ(wrongFinds(moved, 13, &isAny), "");
    EXPECT_TRUE(moved.remove(Warning::fromString("w0")));

    WarningSet assigned;
    assigned.add("other");
    assigned = moved;
    EXPECT_EQ(wrongFinds(assigned, 13, &isNotZero), "");
    EXPECT_FALSE(assigned.contains(Warning::fromString("other")));
    assigned = std::move(original);
    EXPECT_EQ(wrongFinds(assigned, 13, &isNotTwelve), "");
    // A set that is added to itself, and one assigned to itself, keep finding everything.
    EXPECT_FALSE(assigned.addAll(assigned));
    EXPECT_EQ(assigned.size(), 12U);
}

// A message a set has merged another into (the larger angle of attack) is found as before.
TEST(MessageSet, AMessageWhoseContentsWereReplacedKeepsItsHash)
{
    WarningSet warnings;
    warnings.add("w0");
    warnings.add(Warning::LargeAOA{0.1});
    EXPECT_FALSE(warnings.add(Warning::LargeAOA{0.9}));
    ASSERT_EQ(warnings.size(), 2U);
    const Warning* found = warnings.find(Warning::LargeAOA{0.5});
    ASSERT_NE(found, nullptr);
    EXPECT_DOUBLE_EQ(dynamic_cast<const Warning::LargeAOA&>(*found).aoa(), 0.9);
}

// Two sets are equal when they hold the same messages: the capacity is no part of it.
TEST(MessageSet, TheCapacityIsNoPartOfEquality)
{
    WarningSet plain;
    plain.add("w0");
    plain.add("w1");
    EXPECT_TRUE(setOfCapacity(5, 2) == plain);
}

}  // namespace
