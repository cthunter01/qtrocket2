#include "QtRocket/gui/binding/Binding.h"

#include <memory>
#include <utility>

#include "QtRocket/gui/binding/BindingContext.h"
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

Binding::Binding(std::shared_ptr<BindingContext> context)
  : m_context(std::move(context)),
    m_selection(std::dynamic_pointer_cast<ComponentSelection>(m_context))
{
    if (m_context != nullptr)
    {
        m_contextConnection =
            m_context->addListener([this](BindingContext::Phase phase) { contextChanged(phase); });
    }
}

Binding::~Binding() = default;

bool Binding::isValid() const noexcept
{
    return m_context == nullptr || m_context->isValid();
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

void Binding::contextChanged(BindingContext::Phase phase)
{
    switch (phase)
    {
        case BindingContext::Phase::READ:
            if (readState())
            {
                m_changePending = true;
            }
            break;
        case BindingContext::Phase::NOTIFY:
            if (m_changePending)
            {
                m_changePending = false;
                fireChanged();
            }
            break;
        case BindingContext::Phase::INVALIDATED:
            m_changePending = false;
            fireChanged();
            break;
    }
}

}  // namespace QtRocket::Gui
