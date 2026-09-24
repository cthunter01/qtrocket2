#pragma once

// Qt's `emit` keyword is an empty macro unless QT_NO_EMIT (or QT_NO_KEYWORDS) is defined, which
// would turn the declaration of Signal::emit() into `void (Args... args) const`.
#ifdef emit
#error "Signal.h: Qt's emit macro is in force; compile this target with QT_NO_EMIT (see Signal)"
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "QtRocket/util/BugError.h"

namespace QtRocket
{

/// A typed signal: the list of slots (callbacks) to run when something happens. It replaces
/// OpenRocket's ListenerList together with the listener interfaces it held
/// (ComponentChangeListener, StateChangeListener, ...), one std::function per connection.
///
/// Emission follows ListenerList's iterator, which walks a copy of the list: slots may connect and
/// disconnect during emit(), a slot connected during an emission first runs on the next one, and a
/// slot disconnected during an emission does not run if it has not run yet (so a slot may safely
/// disconnect itself or others). Unlike ListenerList, which refused the same listener object twice,
/// every connect() is a separate connection, since a std::function has no identity.
///
/// A Connection or ScopedConnection may outlive its signal: disconnecting then does nothing. A slot
/// must not destroy the signal it is being called from.
///
/// Not thread-safe: every method must be called from the thread that owns the signal.
///
/// Targets that include Qt must define QT_NO_EMIT (the GUI target's CMakeLists sets it; Qt code
/// then writes Q_EMIT), since Qt's `emit` macro would break the declaration of emit(): the guard
/// at the top of this header refuses to compile otherwise. operator() is a convenience for
/// functor-style use, not a workaround for the macro.
///
/// Not ported from ListenerList, all of it debugging bookkeeping with no caller in OpenRocket's
/// core: toString(), the ListenerData record (listener, add/access timestamps, add location), the
/// warn/info/trace logging in addListener()/removeListener() (QtRocket has no logger yet either),
/// getInstantiationLocation(), and invalidateMe()/isInvalidated() (the Invalidatable interface;
/// core only calls invalidateMe() on an Invalidator, and never instantiates a ListenerList).
template <class... Args>
class Signal
{
public:
    using Slot = std::function<void(Args...)>;

    /// A handle to one connection. A default-constructed handle is connected to nothing.
    class Connection
    {
    public:
        Connection() noexcept = default;

        /// True while the slot is still in its signal's list.
        [[nodiscard]] bool connected() const noexcept
        {
            const std::shared_ptr<Signal*> owner = m_owner.lock();
            return owner != nullptr && *owner != nullptr && (*owner)->contains(m_id);
        }

        /// Removes the slot from its signal and empties this handle; true when it was connected.
        bool disconnect() noexcept
        {
            const std::shared_ptr<Signal*> owner = m_owner.lock();
            const bool                     wasConnected =
                owner != nullptr && *owner != nullptr && (*owner)->erase(m_id);
            m_owner.reset();
            m_id = 0;
            return wasConnected;
        }

        /// Two handles are equal when they name the same connection of the same signal.
        [[nodiscard]] bool operator==(const Connection& other) const noexcept
        {
            return m_id == other.m_id && !m_owner.owner_before(other.m_owner) &&
                   !other.m_owner.owner_before(m_owner);
        }

    private:
        friend Signal;

        Connection(std::weak_ptr<Signal*> owner, std::uint64_t id) noexcept
          : m_owner(std::move(owner)), m_id(id)
        {
        }

        std::weak_ptr<Signal*> m_owner;
        std::uint64_t          m_id{0};
    };

    /// Owns a connection: disconnects it when destroyed, unless release() has been called.
    /// Move-only, so a listener object holds one per connection and cannot be copied by mistake.
    /// [[nodiscard]] because a temporary disconnects at the end of its full expression, so the
    /// statement `ScopedConnection{signal.connect(slot)};` would silently undo the connect.
    class [[nodiscard]] ScopedConnection
    {
    public:
        ScopedConnection() noexcept = default;
        explicit ScopedConnection(Connection connection) noexcept
          : m_connection(std::move(connection))
        {
        }
        ~ScopedConnection() { m_connection.disconnect(); }

        ScopedConnection(const ScopedConnection&)            = delete;
        ScopedConnection& operator=(const ScopedConnection&) = delete;

        ScopedConnection(ScopedConnection&& other) noexcept : m_connection(other.release()) { }
        ScopedConnection& operator=(ScopedConnection&& other) noexcept
        {
            if (this != &other)
            {
                m_connection.disconnect();
                m_connection = other.release();
            }
            return *this;
        }

        /// Takes over @p connection, disconnecting the one held so far.
        ScopedConnection& operator=(Connection connection) noexcept
        {
            m_connection.disconnect();
            m_connection = std::move(connection);
            return *this;
        }

        [[nodiscard]] const Connection& connection() const noexcept { return m_connection; }
        [[nodiscard]] bool connected() const noexcept { return m_connection.connected(); }

        /// Disconnects now; true when the connection was still connected.
        bool disconnect() noexcept { return m_connection.disconnect(); }

        /// Gives up ownership: the connection stays connected and this object no longer
        /// disconnects it.
        [[nodiscard]] Connection release() noexcept
        {
            return std::exchange(m_connection, Connection{});
        }

    private:
        Connection m_connection;
    };

    Signal() noexcept = default;
    ~Signal()
    {
        if (m_self != nullptr)
        {
            *m_self = nullptr;  // handles that outlive this signal see it as gone
        }
    }

    Signal(const Signal&)            = delete;
    Signal& operator=(const Signal&) = delete;

    /// Moving takes the connections along: their handles then refer to the new signal, and the
    /// moved-from signal is empty.
    Signal(Signal&& other) noexcept
      : m_entries(std::move(other.m_entries)),
        m_nextId(other.m_nextId),
        m_self(std::move(other.m_self))
    {
        if (m_self != nullptr)
        {
            *m_self = this;
        }
    }
    Signal& operator=(Signal&& other) noexcept
    {
        if (this != &other)
        {
            if (m_self != nullptr)
            {
                *m_self = nullptr;  // this signal's own connections are dropped
            }
            m_entries = std::move(other.m_entries);
            m_nextId  = other.m_nextId;
            m_self    = std::move(other.m_self);
            if (m_self != nullptr)
            {
                *m_self = this;
            }
        }
        return *this;
    }

    /// Adds @p slot, which must not be empty (a programming error, as ListenerList's null check).
    Connection connect(Slot slot)
    {
        QTROCKET_ASSERT(slot != nullptr);
        if (m_self == nullptr)
        {
            m_self = std::make_shared<Signal*>(this);
        }
        const std::uint64_t id = m_nextId++;
        m_entries.push_back(Entry{.id = id, .slot = std::make_shared<const Slot>(std::move(slot))});
        return Connection{m_self, id};
    }

    /// Removes the connection; true when it was connected to this signal. A handle from another
    /// signal is left alone.
    bool disconnect(const Connection& connection) noexcept
    {
        const std::shared_ptr<Signal*> owner = connection.m_owner.lock();
        return owner != nullptr && *owner == this && erase(connection.m_id);
    }

    /// Removes every connection.
    void disconnectAll() noexcept
    {
        // Destroy the slots only once the list is empty (see erase()).
        const std::vector<Entry> removed = std::move(m_entries);
        m_entries.clear();
    }

    /// Calls every connected slot in connection order. See the class comment for what happens
    /// when a slot connects or disconnects during the call. Emitting changes nothing about the
    /// signal itself, so it is const, as in Qt and Boost.Signals2.
    void emit(Args... args) const
    {
        // The snapshot keeps the iteration (and each slot, through its shared_ptr) safe from
        // connect() and disconnect() calls made by the slots.
        const std::vector<Entry> snapshot = m_entries;
        for (const Entry& entry : snapshot)
        {
            if (!contains(entry.id))
            {
                continue;  // disconnected by a slot that ran before it
            }
            (*entry.slot)(args...);
        }
    }

    /// The same as emit(); usable where Qt's `emit` keyword macro is in force.
    void operator()(Args... args) const { emit(args...); }

    [[nodiscard]] std::size_t size() const noexcept { return m_entries.size(); }
    [[nodiscard]] bool        empty() const noexcept { return m_entries.empty(); }

private:
    struct Entry
    {
        std::uint64_t               id;
        std::shared_ptr<const Slot> slot;
    };

    [[nodiscard]] bool contains(std::uint64_t id) const noexcept
    {
        return std::ranges::find(m_entries, id, &Entry::id) != m_entries.end();
    }

    bool erase(std::uint64_t id) noexcept
    {
        const auto it = std::ranges::find(m_entries, id, &Entry::id);
        if (it == m_entries.end())
        {
            return false;
        }
        // Destroy the slot only once the list is consistent again: its captured state may own a
        // ScopedConnection to this very signal, whose destructor re-enters erase().
        const Entry removed = std::move(*it);
        m_entries.erase(it);
        return true;
    }

    std::vector<Entry> m_entries;
    std::uint64_t      m_nextId{1};
    /// The liveness token the handles watch: it points at this signal while it exists, is
    /// created on the first connect() and nulled by the destructor.
    std::shared_ptr<Signal*> m_self;
};

}  // namespace QtRocket
