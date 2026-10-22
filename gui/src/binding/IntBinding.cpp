#include "QtRocket/gui/binding/IntBinding.h"

#include <memory>
#include <optional>
#include <utility>

#include "QtRocket/gui/binding/Binding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket::Gui
{

IntBinding::IntBinding(std::shared_ptr<ComponentSelection> selection, IntSource source, int min,
                       int max)
  : Binding(std::move(selection)), m_source(std::move(source)), m_min(min), m_max(max)
{
    if (getSelection() == nullptr)
    {
        bug("A binding of a property needs a component selection");
    }
    if (!m_source.isWritable())
    {
        bug("An integer field needs a setter (Java: IntegerModel requires set<Name>)");
    }
    if (m_min > m_max)
    {
        bug("An integer field's minimum is above its maximum");
    }
    readState();
}

IntBinding::~IntBinding() = default;

bool IntBinding::readState()
{
    const ComponentSelection& selection = *getSelection();
    const int                 value     = m_source.getValue(selection);
    const bool                enabled   = m_source.isEnabled(selection);
    const bool                changed   = value != m_value || enabled != m_enabled;
    m_value                             = value;
    m_enabled                           = enabled;
    return changed;
}

void IntBinding::setValue(int value)
{
    if (!isValid())
    {
        return;
    }
    // The selection's event (or its own refresh) updates the cache and tells the listeners, one
    // of which may drop the last owner of this binding.
    const std::shared_ptr<Binding> keepAlive = weak_from_this().lock();
    m_source.set(*getSelection(), MathUtil::clamp(value, m_min, m_max));
}

std::optional<int> IntBinding::nextValue() const noexcept
{
    // Java: if (d >= maxValue) return null; return d + 1.
    if (!isValid() || m_value >= m_max)
    {
        return std::nullopt;
    }
    return m_value + 1;
}

std::optional<int> IntBinding::previousValue() const noexcept
{
    // Java: if (d <= minValue) return null; return d - 1.
    if (!isValid() || m_value <= m_min)
    {
        return std::nullopt;
    }
    return m_value - 1;
}

bool IntBinding::setSpinnerValue(int value)
{
    if (!isWidgetWriteAllowed())
    {
        return false;
    }
    setValue(value);
    return true;
}

bool IntBinding::stepUp()
{
    const std::optional<int> next = nextValue();
    return next.has_value() && setSpinnerValue(*next);
}

bool IntBinding::stepDown()
{
    const std::optional<int> previous = previousValue();
    return previous.has_value() && setSpinnerValue(*previous);
}

}  // namespace QtRocket::Gui
