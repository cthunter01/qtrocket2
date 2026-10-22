#pragma once

// Test support of the design view's ribbon widgets (StageSelector, ConfigurationComboBox): the
// change events of a rocket by their types, and an item of a combo box picked as the user picks
// it.

#include <vector>

#include "QtRocket/rocket/ComponentChangeEvent.h"

class QComboBox;

namespace QtRocket
{
class Rocket;
}  // namespace QtRocket

namespace QtRocket::Gui::Test
{

/// The change events a rocket's listeners received since the recorder was made or cleared: their
/// types (ComponentChangeEvent::getType()), and whether each came from the rocket itself.
class ChangeEventTypes
{
public:
    explicit ChangeEventTypes(Rocket& rocket);

    /// The types, in the order of the events.
    [[nodiscard]] const std::vector<int>& types() const noexcept { return m_types; }

    /// Whether every event recorded came from the rocket (an event's source is the rocket).
    [[nodiscard]] bool allFromRocket() const noexcept { return m_allFromRocket; }

    /// Forgets what was recorded.
    void clear() noexcept;

private:
    const Rocket*                           m_rocket;
    std::vector<int>                        m_types;
    bool                                    m_allFromRocket{true};
    ComponentChangeSignal::ScopedConnection m_connection;
};

/// Picks the item at @p index of @p combo as the user does in the popup: makes the item the
/// current one of the popup's view and presses Return there, so that the popup's own handling
/// selects it (QComboBox emits activated()). The popup is not opened: the offscreen platform
/// warns that it can neither raise a popup nor grab the keyboard for it, and the popup's
/// selection does not depend on it being open.
void pickComboItem(QComboBox& combo, int index);

}  // namespace QtRocket::Gui::Test
