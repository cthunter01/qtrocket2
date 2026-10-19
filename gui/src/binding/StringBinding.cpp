#include "QtRocket/gui/binding/StringBinding.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/gui/binding/Binding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

StringBinding::StringBinding(std::shared_ptr<ComponentSelection> selection, StringSource source)
  : Binding(std::move(selection)), m_source(std::move(source))
{
    if (getSelection() == nullptr)
    {
        bug("A binding of a property needs a component selection");
    }
    if (!m_source.isWritable())
    {
        bug("A text field needs a setter");
    }
    readState();
}

StringBinding::~StringBinding() = default;

bool StringBinding::readState()
{
    const ComponentSelection& selection = *getSelection();
    std::string               value     = m_source.getValue(selection);
    const bool                enabled   = m_source.isEnabled(selection);
    const bool                changed   = value != m_value || enabled != m_enabled;
    m_value                             = std::move(value);
    m_enabled                           = enabled;
    return changed;
}

bool StringBinding::setText(std::string_view text)
{
    if (!isWidgetWriteAllowed() || text == m_value)
    {
        return false;
    }
    // The selection's event (or its own refresh) updates the cache and tells the listeners, one
    // of which may drop the last owner of this binding.
    const std::shared_ptr<Binding> keepAlive = weak_from_this().lock();
    m_source.set(*getSelection(), std::string(text));
    return true;
}

StringSource componentNameSource()
{
    return StringSource::of<RocketComponent>({
        .get = [](const RocketComponent& component) { return component.getName(); },
        .set = [](RocketComponent& component, const std::string& name) { component.setName(name); },
    });
}

StringSource componentCommentSource()
{
    return StringSource::of<RocketComponent>({
        .get = [](const RocketComponent& component) { return component.getComment(); },
        .set = [](RocketComponent&   component,
                  const std::string& comment) { component.setComment(comment); },
    });
}

}  // namespace QtRocket::Gui
