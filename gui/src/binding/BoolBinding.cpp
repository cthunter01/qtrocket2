#include "QtRocket/gui/binding/BoolBinding.h"

#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include "QtRocket/gui/binding/Binding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

BoolBinding::BoolBinding(std::shared_ptr<ComponentSelection> selection, BoolSource source)
  : Binding(std::move(selection)), m_source(std::move(source))
{
    if (getSelection() == nullptr)
    {
        bug("A binding of a property needs a component selection");
    }
    if (!m_source->isWritable())
    {
        bug("A check box needs a setter (Java: BooleanModel requires set<Name>)");
    }
    readState();
    m_followerState      = followerState();
    m_followerConnection = addChangeListener([this] { updateFollowers(); });
}

BoolBinding::BoolBinding(bool value)
  : Binding(nullptr),
    m_value(value),
    m_followerState(value),
    m_followerConnection(addChangeListener([this] { updateFollowers(); }))
{
}

BoolBinding::~BoolBinding() = default;

bool BoolBinding::readState()
{
    if (!m_source.has_value())
    {
        return false;  // a binding of its own value has no selection to read
    }
    const BoolSource&         source    = *m_source;
    const ComponentSelection& selection = *getSelection();
    const bool                value     = source.getValue(selection);
    const bool                enabled   = source.isEnabled(selection);
    const bool                changed   = value != m_value || enabled != m_enabled;
    m_value                             = value;
    m_enabled                           = enabled;
    return changed;
}

void BoolBinding::setValue(bool value)
{
    if (!m_source.has_value())
    {
        // A binding of its own value (Java: value = b; stateChanged(null)).
        if (value != m_value)
        {
            m_value = value;
            fireChanged();
        }
        return;
    }
    const BoolSource& source = *m_source;
    if (!isValid())
    {
        return;
    }
    // The selection's event (or its own refresh) updates the cache and tells the listeners, one
    // of which may drop the last owner of this binding.
    const std::shared_ptr<Binding> keepAlive = weak_from_this().lock();
    source.set(*getSelection(), value);
}

bool BoolBinding::setChecked(bool checked)
{
    if (!isWidgetWriteAllowed() || checked == m_value)
    {
        return false;
    }
    setValue(checked);
    return true;
}

BoolBinding::FollowerSignal::Connection BoolBinding::addEnableFollower(
    std::function<void(bool enabled)> setEnabled, bool enableState)
{
    QTROCKET_ASSERT(setEnabled != nullptr);
    FollowerSignal::Slot follower = [setEnabled = std::move(setEnabled),
                                     enableState](std::optional<bool> shown) {
        setEnabled(shown.has_value() && *shown == enableState);
    };
    follower(followerState());
    return m_followers.connect(std::move(follower));
}

std::optional<bool> BoolBinding::followerState() const noexcept
{
    if (!isValid())
    {
        return std::nullopt;
    }
    return m_value;
}

void BoolBinding::updateFollowers()
{
    // Java: updateEnableStatus() when the value changed. A change of the enabled state alone
    // leaves the followers alone (code may enable them for reasons of its own).
    const std::optional<bool> state = followerState();
    if (state == m_followerState)
    {
        return;
    }
    m_followerState = state;
    m_followers.emit(state);
}

}  // namespace QtRocket::Gui
