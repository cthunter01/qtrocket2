#include "QtRocket/gui/binding/Binding.h"

#include <memory>
#include <utility>

#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

namespace
{

/// Holds a binding's firing counter up for a scope (also when a listener throws).
class FiringScope
{
public:
    explicit FiringScope(int& counter) noexcept : m_counter(&counter) { ++*m_counter; }
    ~FiringScope() { --*m_counter; }
    FiringScope(const FiringScope&)            = delete;
    FiringScope& operator=(const FiringScope&) = delete;
    FiringScope(FiringScope&&)                 = delete;
    FiringScope& operator=(FiringScope&&)      = delete;

private:
    int* m_counter;
};

}  // namespace

Binding::Binding(std::shared_ptr<ComponentSelection> selection) : m_selection(std::move(selection))
{
    if (m_selection != nullptr)
    {
        m_selectionConnection = m_selection->addListener(
            [this](ComponentSelection::Phase phase) { selectionChanged(phase); });
    }
}

Binding::~Binding() = default;

bool Binding::isValid() const noexcept
{
    return m_selection == nullptr || m_selection->isValid();
}

Signal<>::Connection Binding::addChangeListener(Signal<>::Slot slot)
{
    return m_changed.connect(std::move(slot));
}

void Binding::fireChanged()
{
    // Destroyed last: a listener may drop the last owner of the binding (see the class comment).
    const std::shared_ptr<Binding> keepAlive = weak_from_this().lock();
    const FiringScope              firing(m_firing);
    m_changed.emit();
}

void Binding::selectionChanged(ComponentSelection::Phase phase)
{
    switch (phase)
    {
        case ComponentSelection::Phase::READ:
            if (readState())
            {
                m_changePending = true;
            }
            break;
        case ComponentSelection::Phase::NOTIFY:
            if (m_changePending)
            {
                m_changePending = false;
                fireChanged();
            }
            break;
        case ComponentSelection::Phase::INVALIDATED:
            m_changePending = false;
            fireChanged();
            break;
    }
}

}  // namespace QtRocket::Gui
