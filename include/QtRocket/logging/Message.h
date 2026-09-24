#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

/// A rocket component that caused a message (Java: RocketComponent). Extension point: rocket/
/// does not exist yet, so a source carries the two things a message needs from a component: its
/// id, which is its identity, and its name, which the message text prints. The rocket group
/// builds one from a component (or replaces this with the component itself); equality must stay
/// by id.
///
/// Two sources are equal when their ids are equal, whatever their names (Java:
/// RocketComponent.equals() compares ids), so two same-named components ("Body tube" twice) are
/// two sources. Deviation: the name is a snapshot; renaming the component afterwards does not
/// change the text of an existing message (Java prints the current name).
struct MessageSource
{
    MessageSource(Uuid componentId, std::string componentName)
      : id(componentId), name(std::move(componentName))
    {
    }

    /// The component's id (Java: RocketComponent.getID()).
    Uuid id;
    /// The component's name, e.g. "Body tube".
    std::string name;

    [[nodiscard]] friend bool operator==(const MessageSource& lhs,
                                         const MessageSource& rhs) noexcept
    {
        return lhs.id == rhs.id;
    }
};

/// The rocket components that caused a message, in the order they were given. Deviation: Java
/// tells `null` (no sources) from an empty array and the two compare unequal; here both are the
/// empty vector.
using MessageSources = std::vector<MessageSource>;

/// Base class of the user-visible warnings and errors (OpenRocket: logging/Message). This is the
/// warning/error model shown to the user, not diagnostic logging.
///
/// Ownership: a message is a polymorphic value. A MessageSet keeps its own copy of every message
/// it is given (a std::unique_ptr<T> made with clone()), so the static warning constants and the
/// caller's objects are never shared with, or mutated by, a set. Java shares the objects instead;
/// the observable behaviour is the same because a set only ever mutates a message through
/// replaceContents(), and Java too clones before it attaches sources.
///
/// Equality (Java: equals()): two messages are equal when they have the same dynamic type, the
/// same sources (by component id, see MessageSource) and the same priority. Subclasses refine
/// this: Warning::Other and ErrorMessage::Other also compare their text, Warning::EventAfterLanding
/// compares ids and Warning::MissingMotor every field. A MessageSet uses this to hold at most one
/// message of each kind.
///
/// Not ported: hashCode() (Java: getClass().hashCode(), with Warning::Other hashing its text and
/// Warning::MissingMotor its fields), so there is no std::hash<Message>. Nothing in the core
/// hashes messages: a MessageSet is a vector searched with equals(). A group that wants messages
/// as keys of an unordered container adds a virtual hash consistent with equals() here.
class Message
{
public:
    virtual ~Message() = default;

    /// The message text without the sources; short and descriptive (Java: getMessageDescription()).
    [[nodiscard]] virtual std::string messageDescription() const = 0;

    /// The message text followed by the source names, e.g. `Gap in rocket airframe:  "Body tube",
    /// "Nose cone"`; the text alone when there are no sources (Java: toString(), through
    /// addSourcesToMessageText()).
    [[nodiscard]] std::string toString() const;

    /// True when @p other describes a worse condition than this message, so that a MessageSet
    /// should let it replace this one (Java: replaceBy()).
    [[nodiscard]] virtual bool replaceBy(const Message& other) const = 0;

    /// Copies the contents of @p other into this message. Only the subclasses whose replaceBy()
    /// can return true implement it; the default throws BugError (Java:
    /// UnsupportedOperationException).
    virtual void replaceContents(const Message& other);

    /// A copy with the dynamic type and the full state (id included) of this message (Java:
    /// clone()).
    [[nodiscard]] virtual std::unique_ptr<Message> clone() const = 0;

    /// The simple class name that OpenRocket writes as the `type` of a warning in .ork files, e.g.
    /// "LargeAOA" or "Other" (Java: getClass().getSimpleName()).
    [[nodiscard]] virtual std::string_view typeName() const noexcept = 0;

    /// Java: equals(); see the class comment. operator== forwards to it.
    [[nodiscard]] virtual bool equals(const Message& other) const;

    /// True when @p other has exactly the dynamic type of this message (Java: getClass() ==).
    [[nodiscard]] bool sameType(const Message& other) const noexcept;

    /// The message's id (Java: getID()): a random UUID unless one was set. The .ork saver writes
    /// it and the loader sets it back, so that the flight events of a saved simulation find
    /// their warnings again (MessageSet::findById()).
    [[nodiscard]] const Uuid& id() const noexcept { return m_id; }
    void                      setId(Uuid id) noexcept { m_id = id; }

    [[nodiscard]] const MessageSources& sources() const noexcept { return m_sources; }
    void setSources(MessageSources sources) { m_sources = std::move(sources); }

    [[nodiscard]] MessagePriority priority() const noexcept { return m_priority; }
    void setPriority(MessagePriority priority) noexcept { m_priority = priority; }

protected:
    /// A message with a fresh random id (Java: UUID.randomUUID()).
    Message();
    /// A message with that id (Java: Message(UUID)).
    explicit Message(Uuid id) noexcept;
    Message(const Message&)                = default;
    Message(Message&&) noexcept            = default;
    Message& operator=(const Message&)     = default;
    Message& operator=(Message&&) noexcept = default;

    /// @p text followed by `:  "<name>", "<name>"` for the sources; @p text alone when there are
    /// none (Java: addSourcesToMessageText()).
    [[nodiscard]] static std::string addSourcesToMessageText(std::string           text,
                                                             const MessageSources& sources);

    /// True when the two source lists have the same components in the same order, by id (Java:
    /// sourcesEqual(), Arrays.equals()).
    [[nodiscard]] static bool sourcesEqual(const MessageSources& lhs,
                                           const MessageSources& rhs) noexcept;

private:
    Uuid            m_id;
    MessageSources  m_sources;
    MessagePriority m_priority{MessagePriority::NORMAL};
};

[[nodiscard]] inline bool operator==(const Message& lhs, const Message& rhs)
{
    return lhs.equals(rhs);
}

}  // namespace QtRocket
