#pragma once

#include <string_view>

#include "QtRocket/file/openrocket/EntryHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// Reads the <entry> elements of a simulation extension's configuration into a Config
/// (OpenRocket's file/openrocket/importt/ConfigHandler); it is the handler of an <extension>
/// element's <config>, and of an entry of type "list".
///
/// Every <entry> that closes with a value (see EntryHelper::getValueFromEntry()) goes into the
/// Config under its key attribute, a later entry with the same key replacing the earlier value
/// in its place, or, without a key, into this handler's own list (EntryHandler::getList()),
/// which is what the handler of a list entry is read for. An entry without a value (a type
/// that is not known or missing, a number that is none) is dropped without a warning. An entry
/// of type "list" gets a ConfigHandler of its own for the entries in it; any other child is
/// plain text, and a child that is no <entry> gives AbstractElementHandler's warnings when it
/// has text or attributes.
///
/// Deviation from OpenRocket: an entry of type "list" is loaded. Java never loads one:
/// ConfigHandler declares fields listHandler and list of its own, which hide EntryHandler's,
/// so EntryHandler.getNestedList() reads a list handler that is never set and the entry has no
/// value. OpenRocket's saver writes list entries, so its loader loses what its saver wrote;
/// here the entry becomes a Config list of the values of the entries in it (a list in a list
/// included), and an empty list entry an empty list. Inside a list entry, an entry with a key
/// goes into the Config of the list's own handler, which nobody reads, as the code is written
/// in Java. A list entry nested more than kMaxListDepth deep in list entries is not read: it
/// gives the warning "List entries nested too deeply, ignoring." and has no value, so that no
/// file can make the handlers and the values nest without bound.
class ConfigHandler final : public EntryHandler
{
public:
    /// The deepest a list entry may stand in list entries: a list in a list in a ... in a list
    /// of the configuration, 32 lists in all. (OpenRocket's own extensions store no list.)
    static constexpr int kMaxListDepth = 32;

    /// The handler of a <config> element.
    ConfigHandler() = default;

    /// The handler of the entries of a list entry that stands in @p listDepth list entries
    /// itself (0: an entry of the configuration).
    explicit ConfigHandler(int listDepth) noexcept;

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

    /// The entries read so far (getConfig()).
    [[nodiscard]] const Config& getConfig() const noexcept { return m_config; }

private:
    Config m_config;
    /// The number of list entries this handler's entries stand in.
    int m_listDepth{0};
};

}  // namespace QtRocket
