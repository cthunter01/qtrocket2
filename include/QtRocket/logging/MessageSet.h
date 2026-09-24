#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <format>
#include <iterator>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>

#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

/// A set of messages in insertion order that holds at most one message of each kind
/// (OpenRocket: logging/MessageSet). "Kind" is Message::equals(): same dynamic type, sources and
/// priority, plus whatever the subclass adds. When a message of the same kind is already there,
/// Message::replaceBy() decides whether the newcomer's contents replace it (Java: replaceBy() then
/// replaceContents()), and add() returns false either way.
///
/// Ownership and identity: the set owns copies of the messages it is given (see Message). Java
/// stores the caller's object instead, so there the same instance can sit in a WarningSet and,
/// say, a SIM_WARN FlightEvent (SimulationStatus.addWarning() reads it back with get()). Here the
/// stored instance is reached only through the pointers and references the set hands out
/// (find(), findById(), begin(), messagesWithPriority()); they stay valid until that message is
/// removed or the set is destroyed or assigned to, survive a move of the set, and a copy of the
/// set holds other instances (with the same ids).
///
/// Immutability (Java: Mutable, IllegalStateException): after immute() every call that would
/// change the set's contents throws BugError, since writing to a frozen set is a programming
/// error; the error names the file and line of the immute() call (Java: the stack trace of it,
/// "Object has been made immutable at ..."). As in Java the check happens only when something
/// would change: remove() of an absent
/// message, filterOut() without a match, clear() of an empty set and addAll() of an empty set
/// return quietly. Assignment is not a change of the contents but a replacement of the whole
/// set, like reassigning the reference in Java: it overwrites an immuted set and carries over
/// the source's mutability (so a move assignment can stay noexcept). Hold a set const, or hand
/// it out by const reference, to forbid that. A copy or a move of a set keeps its immutability.
///
/// Monitoring (Java: Monitorable; the set satisfies the Monitorable concept of
/// util/Monitorable.h): modId() is a ModId that is redrawn whenever the contents may have
/// changed, so that a reader (the simulation's flight data, a view in the GUI) can cache what it
/// made of the set and refresh it only when the id differs. Java draws a new id in add() only,
/// whether or not the set grew (addAll() of the set itself included: it add()s every element),
/// and not when Iterator.remove() takes a message out; here every mutation draws one (add() as
/// in Java; remove(), erase(), filterOut() and clear() when they remove something), as
/// Monitorable's contract asks. A copy carries the source's id and an assignment draws a fresh
/// one, so an object's id never goes backwards.
///
/// Not ported: the java.util.AbstractSet helpers removeAll(), retainAll(), containsAll(),
/// removeIf(), toArray() and stream(); iterate with begin()/end() and the standard algorithms
/// instead. Iterator.remove() is erase(). Java's findById() logs an error when nothing is found;
/// here it just returns nullptr.
///
/// T is Warning or ErrorMessage: a Message subclass with a static fromString(std::string) that
/// builds its text-only message (used by add(std::string_view)).
template <class T>
    requires std::derived_from<T, Message>
class MessageSet
{
public:
    /// A forward iterator over `const T&`, in insertion order.
    class ConstIterator
    {
    public:
        // NOLINTBEGIN(readability-identifier-naming): the names std::iterator_traits requires
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const T*;
        using reference         = const T&;
        // NOLINTEND(readability-identifier-naming)

        ConstIterator() = default;

        [[nodiscard]] reference operator*() const noexcept { return **m_it; }
        [[nodiscard]] pointer   operator->() const noexcept { return m_it->get(); }
        ConstIterator&          operator++() noexcept
        {
            ++m_it;
            return *this;
        }
        ConstIterator operator++(int) noexcept
        {
            ConstIterator old = *this;
            ++m_it;
            return old;
        }
        [[nodiscard]] bool operator==(const ConstIterator& other) const noexcept = default;

    private:
        friend class MessageSet;
        using Underlying = std::vector<std::unique_ptr<T>>::const_iterator;
        explicit ConstIterator(Underlying it) noexcept : m_it(it) { }
        Underlying m_it{};
    };

    MessageSet() = default;
    MessageSet(const MessageSet& other);
    MessageSet(MessageSet&&) noexcept = default;
    /// Replaces the whole set, immutability included (see the class comment).
    MessageSet& operator=(const MessageSet& other);
    /// Replaces the whole set, immutability included (see the class comment).
    MessageSet& operator=(MessageSet&& other) noexcept;
    ~MessageSet() = default;

    /// Adds a copy of @p message. Returns true when the set grew, false when a message of the
    /// same kind was already there (whose contents @p message may have replaced; Java: add(E)).
    bool add(const T& message);
    /// Adds a copy of @p message with @p sources as its sources (Java: add(E, RocketComponent...)).
    bool add(const T& message, MessageSources sources);
    /// Adds the text-only message `<message.toString()>:  "<discriminator>"` (Java: add(E,
    /// String)). As in Java it goes through add(std::string_view), so the result has the default
    /// priority, not the priority of @p message.
    bool add(const T& message, std::string_view discriminator);
    /// Adds the text-only message T::fromString(text) (Java: add(String)).
    bool add(std::string_view text);
    /// Adds a copy of every message of @p other, in order. Returns true when the set grew.
    bool addAll(const MessageSet& other);

    /// Removes the first message equal to @p message, which may be an element of this set.
    /// Returns true when one was removed.
    bool remove(const Message& message);
    /// Removes the message at @p pos (an iterator of this set, not end()) and returns the
    /// iterator to the one after it (Java: iterator().remove()).
    ConstIterator erase(ConstIterator pos)
    {
        checkMutable();
        m_modId = ModId{};
        return ConstIterator{m_messages.erase(pos.m_it)};
    }
    /// Removes every message with the dynamic type of @p filter, whatever its contents; @p filter
    /// may be an element of this set (Java: filterOut()).
    void filterOut(const T& filter);
    void clear();

    [[nodiscard]] bool contains(const Message& message) const;
    /// The message in the set equal to @p message, or nullptr (Java: get(Message)). A caller that
    /// just add()ed a message reads the stored copy back this way.
    [[nodiscard]] const T* find(const Message& message) const;
    [[nodiscard]] T*       find(const Message& message);
    /// The message with that id, or nullptr (Java: findById()).
    [[nodiscard]] const T* findById(const Uuid& id) const noexcept;
    [[nodiscard]] T*       findById(const Uuid& id) noexcept;

    [[nodiscard]] std::size_t size() const noexcept { return m_messages.size(); }
    [[nodiscard]] bool        empty() const noexcept { return m_messages.empty(); }
    /// Java: getNrOfMessagesWithPriority().
    [[nodiscard]] std::size_t countWithPriority(MessagePriority priority) const noexcept;
    /// The messages with that priority, in insertion order (Java: getMessagesWithPriority()).
    [[nodiscard]] std::vector<const T*> messagesWithPriority(MessagePriority priority) const;
    /// The same, modifiable in place (Java hands out the stored objects). As with find(), a
    /// change that alters a message's kind (its sources or priority) is not a change of the set:
    /// no id is drawn and no duplicate is merged.
    [[nodiscard]] std::vector<T*> messagesWithPriority(MessagePriority priority);

    [[nodiscard]] ConstIterator begin() const noexcept
    {
        return ConstIterator{m_messages.cbegin()};
    }
    [[nodiscard]] ConstIterator end() const noexcept { return ConstIterator{m_messages.cend()}; }

    /// Makes the set immutable for good; repeated calls do nothing (Java: immute()). @p where,
    /// the call site by default, is what a refused change reports afterwards.
    void immute(std::source_location where = std::source_location::current()) noexcept
    {
        if (m_mutable)
        {
            m_mutable   = false;
            m_immutedAt = where;
        }
    }
    [[nodiscard]] bool isMutable() const noexcept { return m_mutable; }

    /// A modification id that is redrawn whenever the contents may have changed, ModId::zero()
    /// until the first add() (Java: getModID(); see the class comment).
    [[nodiscard]] ModId modId() const noexcept { return m_modId; }

    /// "Messages[<first>,<second>,...]" with each message's toString() (Java: toString()).
    [[nodiscard]] std::string toString() const;

    /// Set equality: the same messages in any order (Java: AbstractSet.equals()).
    [[nodiscard]] bool operator==(const MessageSet& other) const;

private:
    void                      checkMutable() const;
    bool                      addOwned(std::unique_ptr<T> message);
    static std::unique_ptr<T> cloneMessage(const T& message);

    std::vector<std::unique_ptr<T>> m_messages;
    bool                            m_mutable{true};
    std::source_location            m_immutedAt;
    ModId                           m_modId{ModId::zero()};
};

template <class T>
    requires std::derived_from<T, Message>
MessageSet<T>::MessageSet(const MessageSet& other)
  : m_mutable(other.m_mutable), m_immutedAt(other.m_immutedAt), m_modId(other.m_modId)
{
    m_messages.reserve(other.m_messages.size());
    for (const std::unique_ptr<T>& message : other.m_messages)
    {
        m_messages.push_back(cloneMessage(*message));
    }
}

template <class T>
    requires std::derived_from<T, Message>
MessageSet<T>& MessageSet<T>::operator=(const MessageSet& other)
{
    if (this != &other)
    {
        MessageSet copy(other);
        *this = std::move(copy);
    }
    return *this;
}

template <class T>
    requires std::derived_from<T, Message>
MessageSet<T>& MessageSet<T>::operator=(MessageSet&& other) noexcept
{
    // Through a local, so that a set moved onto itself keeps its messages.
    MessageSet moved(std::move(other));
    m_messages.swap(moved.m_messages);
    m_mutable   = moved.m_mutable;
    m_immutedAt = moved.m_immutedAt;
    m_modId     = ModId{};
    return *this;
}

template <class T>
    requires std::derived_from<T, Message>
bool MessageSet<T>::add(const T& message)
{
    checkMutable();
    return addOwned(cloneMessage(message));
}

template <class T>
    requires std::derived_from<T, Message>
bool MessageSet<T>::add(const T& message, MessageSources sources)
{
    checkMutable();
    std::unique_ptr<T> copy = cloneMessage(message);
    copy->setSources(std::move(sources));
    return addOwned(std::move(copy));
}

template <class T>
    requires std::derived_from<T, Message>
bool MessageSet<T>::add(const T& message, std::string_view discriminator)
{
    std::string text = message.toString();
    text += ":  \"";
    text += discriminator;
    text += '"';
    return add(std::string_view{text});
}

template <class T>
    requires std::derived_from<T, Message>
bool MessageSet<T>::add(std::string_view text)
{
    checkMutable();
    return add(T::fromString(std::string{text}));
}

template <class T>
    requires std::derived_from<T, Message>
bool MessageSet<T>::addAll(const MessageSet& other)
{
    if (other.m_messages.empty())
    {
        return false;  // Java: AbstractCollection.addAll() checks in add(), so never here
    }
    checkMutable();
    if (this == &other)
    {
        m_modId = ModId{};  // Java add()s every element to itself: nothing grows, each draws an id
        return false;
    }
    bool grew = false;
    for (const std::unique_ptr<T>& message : other.m_messages)
    {
        grew = add(*message) || grew;
    }
    return grew;
}

template <class T>
    requires std::derived_from<T, Message>
bool MessageSet<T>::remove(const Message& message)
{
    const auto it = std::ranges::find_if(
        m_messages, [&message](const std::unique_ptr<T>& m) { return message.equals(*m); });
    if (it == m_messages.end())
    {
        return false;
    }
    checkMutable();  // Java: AbstractCollection.remove() checks in Iterator.remove()
    m_modId = ModId{};
    m_messages.erase(it);
    return true;
}

template <class T>
    requires std::derived_from<T, Message>
void MessageSet<T>::filterOut(const T& filter)
{
    // The type is taken once, up front: `filter` may be an element of this set, which the erase
    // destroys while the predicate is still running.
    const std::type_info& type = typeid(filter);
    const auto ofThatType = [&type](const std::unique_ptr<T>& m) { return typeid(*m) == type; };
    if (!std::ranges::any_of(m_messages, ofThatType))
    {
        return;  // Java: filterOut() checks in Iterator.remove(), only on a match
    }
    checkMutable();
    m_modId = ModId{};
    std::erase_if(m_messages, ofThatType);
}

template <class T>
    requires std::derived_from<T, Message>
void MessageSet<T>::clear()
{
    if (m_messages.empty())
    {
        return;  // Java: AbstractCollection.clear() checks in Iterator.remove()
    }
    checkMutable();
    m_modId = ModId{};
    m_messages.clear();
}

template <class T>
    requires std::derived_from<T, Message>
bool MessageSet<T>::contains(const Message& message) const
{
    return find(message) != nullptr;
}

template <class T>
    requires std::derived_from<T, Message>
const T* MessageSet<T>::find(const Message& message) const
{
    const auto it = std::ranges::find_if(
        m_messages, [&message](const std::unique_ptr<T>& m) { return message.equals(*m); });
    return it == m_messages.end() ? nullptr : it->get();
}

template <class T>
    requires std::derived_from<T, Message>
T* MessageSet<T>::find(const Message& message)
{
    const auto it = std::ranges::find_if(
        m_messages, [&message](const std::unique_ptr<T>& m) { return message.equals(*m); });
    return it == m_messages.end() ? nullptr : it->get();
}

template <class T>
    requires std::derived_from<T, Message>
const T* MessageSet<T>::findById(const Uuid& id) const noexcept
{
    const auto it = std::ranges::find_if(
        m_messages, [&id](const std::unique_ptr<T>& m) { return m->id() == id; });
    return it == m_messages.end() ? nullptr : it->get();
}

template <class T>
    requires std::derived_from<T, Message>
T* MessageSet<T>::findById(const Uuid& id) noexcept
{
    const auto it = std::ranges::find_if(
        m_messages, [&id](const std::unique_ptr<T>& m) { return m->id() == id; });
    return it == m_messages.end() ? nullptr : it->get();
}

template <class T>
    requires std::derived_from<T, Message>
std::size_t MessageSet<T>::countWithPriority(MessagePriority priority) const noexcept
{
    return static_cast<std::size_t>(std::ranges::count_if(
        m_messages, [priority](const std::unique_ptr<T>& m) { return m->priority() == priority; }));
}

template <class T>
    requires std::derived_from<T, Message>
std::vector<const T*> MessageSet<T>::messagesWithPriority(MessagePriority priority) const
{
    std::vector<const T*> result;
    for (const std::unique_ptr<T>& message : m_messages)
    {
        if (message->priority() == priority)
        {
            result.push_back(message.get());
        }
    }
    return result;
}

template <class T>
    requires std::derived_from<T, Message>
std::vector<T*> MessageSet<T>::messagesWithPriority(MessagePriority priority)
{
    std::vector<T*> result;
    for (const std::unique_ptr<T>& message : m_messages)
    {
        if (message->priority() == priority)
        {
            result.push_back(message.get());
        }
    }
    return result;
}

template <class T>
    requires std::derived_from<T, Message>
std::string MessageSet<T>::toString() const
{
    std::string text  = "Messages[";
    bool        first = true;
    for (const std::unique_ptr<T>& message : m_messages)
    {
        if (!first)
        {
            text += ',';
        }
        first = false;
        text += message->toString();
    }
    text += ']';
    return text;
}

template <class T>
    requires std::derived_from<T, Message>
bool MessageSet<T>::operator==(const MessageSet& other) const
{
    return size() == other.size() &&
           std::ranges::all_of(other.m_messages,
                               [this](const std::unique_ptr<T>& m) { return contains(*m); });
}

template <class T>
    requires std::derived_from<T, Message>
void MessageSet<T>::checkMutable() const
{
    if (!m_mutable)
    {
        // Java: IllegalStateException("Object has been made immutable at " + immuteTrace); the
        // file name is cut down to its base name as BugError does with its own location.
        std::string_view file = m_immutedAt.file_name();
        file                  = file.substr(file.find_last_of("/\\") + 1);
        bug(std::format("MessageSet has been made immutable at {}:{}", file, m_immutedAt.line()));
    }
}

template <class T>
    requires std::derived_from<T, Message>
bool MessageSet<T>::addOwned(std::unique_ptr<T> message)
{
    m_modId     = ModId{};  // Java: add() draws one before it looks, so a duplicate counts too
    T* existing = find(*message);
    if (existing == nullptr)
    {
        m_messages.push_back(std::move(message));
        return true;
    }
    if (existing->replaceBy(*message))
    {
        existing->replaceContents(*message);
    }
    return false;
}

template <class T>
    requires std::derived_from<T, Message>
std::unique_ptr<T> MessageSet<T>::cloneMessage(const T& message)
{
    std::unique_ptr<Message> copy  = message.clone();
    T*                       typed = dynamic_cast<T*>(copy.get());
    if (typed == nullptr)
    {
        bug("Message::clone() returned a message of another type");
    }
    // The pointer now lives in `typed`; release() only gives up ownership.
    [[maybe_unused]] const Message* const released = copy.release();
    return std::unique_ptr<T>(typed);
}

}  // namespace QtRocket
