#pragma once

#include <memory>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/util/Config.h"

namespace QtRocket
{

/// The base of the handlers of entries with a key, a type and a value, such as
/// `<entry key="foo" type="string">bar</entry>` (OpenRocket's
/// file/openrocket/importt/EntryHandler): the entries of a simulation extension's configuration
/// (ConfigHandler) and of the document's preferences. It keeps what an entry of type "list"
/// needs: the handler that reads the list's own entries, and this handler's own list, into
/// which the entries without a key go when this handler is itself the handler of a list.
///
/// An entry of type "list" holds entries without a key, one per element of the list:
///
///     <entry key="values" type="list">
///       <entry type="number">1</entry>
///       <entry type="string">two</entry>
///     </entry>
///
/// The handler of the outer entry opens a list handler for it (openList()), which collects the
/// inner values (addToList()); when the outer entry closes, EntryHelper asks getNestedList()
/// for them.
///
/// Deviations from OpenRocket:
/// - Java's fields listHandler and list are protected; here they are reached through
///   openList(), closeList(), addToList() and getList().
/// - The list handler is owned (std::unique_ptr) and the next list entry replaces it, where
///   Java leaves the old one to the garbage collector.
/// - A value is a Config::Value (Java: an Object), so a list holds nothing but the types a
///   Config holds.
class EntryHandler : public AbstractElementHandler
{
public:
    /// The values of the list entry this handler opened last (getNestedList()): the list of
    /// that entry's handler, or null when this handler has not opened a list entry.
    [[nodiscard]] const Config::List* getNestedList() const noexcept;

    /// This handler's own list (Java: the field list): the values of the entries without a key
    /// it has closed.
    [[nodiscard]] const Config::List& getList() const noexcept { return m_list; }

protected:
    EntryHandler() = default;

    /// Makes @p handler the handler of the list entry being opened (Java: listHandler = ...)
    /// and returns it, for openElement() to return. The handler of the list entry before it is
    /// destroyed.
    /// @throws BugError when @p handler is null
    EntryHandler* openList(std::unique_ptr<EntryHandler> handler);

    /// Forgets the handler of the list entry opened last, so that getNestedList() is null
    /// again: for a list entry this handler does not read.
    void closeList() noexcept;

    /// Adds @p value to this handler's own list (Java: list.add(value)).
    void addToList(Config::Value value);

private:
    std::unique_ptr<EntryHandler> m_listHandler;
    Config::List                  m_list;
};

}  // namespace QtRocket
