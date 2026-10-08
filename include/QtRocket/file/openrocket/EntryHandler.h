#pragma once

#include <memory>
#include <optional>

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
/// inner values (addToList()); when the outer entry closes, EntryHelper takes them
/// (takeNestedList()).
///
/// A nested list belongs to the entry that opened it and is read once, by the close of that
/// entry, which is the first close this handler sees after openList(): takeNestedList() moves
/// the values out and forgets the list handler, and EntryHelper::getValueFromEntry() calls it
/// for every entry it is asked about, whatever the entry's type. So a later entry never finds
/// the list of an earlier one, although the attributes it is closed with may claim that it is
/// a list (DelegatorHandler hands an entry the attributes of an ignored child element). A
/// subclass that reads its entries through EntryHelper need not do anything for this.
///
/// Deviations from OpenRocket:
/// - Java's fields listHandler and list are protected; here they are reached through
///   openList(), closeList(), takeNestedList(), addToList() and getList().
/// - The list handler is owned (std::unique_ptr) and the next list entry replaces it, where
///   Java leaves the old one to the garbage collector.
/// - Java keeps the list handler of the last list entry for good and hands its list out by
///   reference to every later entry that closes as a list. With the lists really loaded (see
///   ConfigHandler) that would let a file store one list many times over, each copy holding
///   the copies before it, so here the list is handed out once (takeNestedList()).
/// - A value is a Config::Value (Java: an Object), so a list holds nothing but the types a
///   Config holds.
class EntryHandler : public AbstractElementHandler
{
public:
    /// The values of the list entry this handler opened and has not closed yet
    /// (getNestedList()): the list of that entry's handler, or null when there is none. It only
    /// looks; the close of an entry takes the values with takeNestedList().
    [[nodiscard]] const Config::List* getNestedList() const noexcept;

    /// The values of the list entry this handler opened, moved out of that entry's handler,
    /// which is forgotten (getNestedList() is null afterwards); nullopt when no list entry is
    /// open. For the close of an entry: see the class comment.
    [[nodiscard]] std::optional<Config::List> takeNestedList() noexcept;

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

    /// Forgets the handler of the list entry opened last and its values, so that
    /// getNestedList() is null again: for a list entry this handler does not read.
    void closeList() noexcept;

    /// Adds @p value to this handler's own list (Java: list.add(value)).
    void addToList(Config::Value value);

private:
    std::unique_ptr<EntryHandler> m_listHandler;
    Config::List                  m_list;
};

}  // namespace QtRocket
