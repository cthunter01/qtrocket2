#include "QtRocket/logging/MessageSet.h"

#include <iterator>
#include <stdexcept>
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

namespace
{

using QtRocket::ErrorMessage;
using QtRocket::ErrorSet;
using QtRocket::MessagePriority;
using QtRocket::MessageSources;
using QtRocket::Warning;
using QtRocket::WarningSet;

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
    b.setSources(MessageSources{{"fs-1", "Fin set"}});
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
    EXPECT_TRUE(warnings.add(Warning::kAirframeGap,
                             MessageSources{{"bt-1", "Body tube"}, {"nc-1", "Nose cone"}}));
    EXPECT_TRUE(Warning::kAirframeGap.sources().empty());
    ASSERT_EQ(warnings.size(), 1U);
    EXPECT_EQ(warnings.begin()->sources(),
              (MessageSources{{"bt-1", "Body tube"}, {"nc-1", "Nose cone"}}));
    EXPECT_EQ(warnings.begin()->toString(),
              "Gap in rocket airframe:  \"Body tube\", \"Nose cone\"");
    // The same warning with other sources, or none, is another kind.
    EXPECT_TRUE(warnings.add(Warning::kAirframeGap, MessageSources{{"fs-1", "Fin set"}}));
    EXPECT_TRUE(warnings.add(Warning::kAirframeGap));
    EXPECT_FALSE(warnings.add(Warning::kAirframeGap, MessageSources{{"fs-1", "Fin set"}}));
    EXPECT_EQ(warnings.size(), 3U);
}

TEST(MessageSet, SourcesAreComparedByComponentIdNotName)
{
    WarningSet warnings;
    // Two fin sets with the default name: two warnings, as in Java (RocketComponent.equals()
    // compares ids).
    EXPECT_TRUE(warnings.add(Warning::kThickFin, MessageSources{{"fs-1", "Fin set"}}));
    EXPECT_TRUE(warnings.add(Warning::kThickFin, MessageSources{{"fs-2", "Fin set"}}));
    EXPECT_EQ(warnings.size(), 2U);
    // The same component under another name is the same warning.
    EXPECT_FALSE(warnings.add(Warning::kThickFin, MessageSources{{"fs-1", "Fins"}}));
    EXPECT_EQ(warnings.size(), 2U);
    Warning::Other probe = Warning::kThickFin;
    probe.setSources(MessageSources{{"fs-1", "Fins"}});
    EXPECT_TRUE(warnings.contains(probe));
    EXPECT_NE(warnings.find(probe), nullptr);
    probe.setSources(MessageSources{{"fs-3", "Fin set"}});
    EXPECT_FALSE(warnings.contains(probe));
    EXPECT_FALSE(warnings.remove(probe));
    probe.setSources(MessageSources{{"fs-2", "Whatever"}});
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
    EXPECT_THROW(errors.erase(errors.begin()), std::logic_error);
    EXPECT_EQ(errors.size(), 1U);
}

TEST(MessageSet, FilterOutRemovesEveryMessageOfThatType)
{
    WarningSet warnings;
    warnings.add(Warning::kOpenAirframeForward, MessageSources{{"a-1", "A"}});
    warnings.add(Warning::kOpenAirframeForward, MessageSources{{"b-1", "B"}});
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
    EXPECT_EQ(warnings.findById("no-such-id"), nullptr);
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
    const std::vector<const Warning*> high = warnings.messagesWithPriority(MessagePriority::HIGH);
    ASSERT_EQ(high.size(), 2U);
    EXPECT_EQ(high[0]->messageDescription(), Warning::kNoRecoveryDevice.messageDescription());
    EXPECT_EQ(high[1]->messageDescription(), Warning::kEarlySeparation.messageDescription());
    EXPECT_EQ(warnings.messagesWithPriority(MessagePriority::LOW).size(), 1U);
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
    EXPECT_THROW(errors.add("more"), std::logic_error);
    EXPECT_THROW(errors.add(ErrorMessage::Other{"more"}), std::logic_error);
    EXPECT_THROW(errors.add(ErrorMessage::Other{"more"}, MessageSources{{"x-1", "x"}}),
                 std::logic_error);
    EXPECT_THROW(errors.add(ErrorMessage::Other{"more"}, "discriminator"), std::logic_error);
    EXPECT_THROW(errors.addAll(more), std::logic_error);
    EXPECT_EQ(errors.size(), 1U);
    EXPECT_TRUE(errors.contains(ErrorMessage::Other{"kept"}));
}

TEST(MessageSet, ImmuteBlocksRemoving)
{
    ErrorSet errors;
    errors.add("kept");
    errors.immute();
    EXPECT_THROW(errors.remove(ErrorMessage::Other{"kept"}), std::logic_error);
    EXPECT_THROW(errors.erase(errors.begin()), std::logic_error);
    EXPECT_THROW(errors.filterOut(ErrorMessage::Other{"kept"}), std::logic_error);
    EXPECT_THROW(errors.clear(), std::logic_error);
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
    EXPECT_THROW(kept.addAll(kept), std::logic_error);
    EXPECT_EQ(kept.size(), 1U);
    WarningSet aoaOnly;
    aoaOnly.add(Warning::LargeAOA{0.1});
    aoaOnly.immute();
    EXPECT_NO_THROW(aoaOnly.filterOut(Warning::kSupersonic));  // no Other in the set
    EXPECT_THROW(aoaOnly.filterOut(Warning::LargeAOA{0.9}), std::logic_error);
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
    EXPECT_THROW(target.add(Warning::kThickFin), std::logic_error);
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
    EXPECT_FALSE(a.addAll(a));
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
    errors.add(ErrorMessage::Other{"two"}, MessageSources{{"nc-1", "Nose cone"}});
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

}  // namespace
