#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "QtRocket/gui/binding/Binding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/PropertySource.h"

namespace QtRocket::Gui
{

/// The model of a text field (widgets/TextCommitter): a component's name and comment, the
/// rocket's designer, revision and kit name. OpenRocket has no model for them: the private
/// TextFieldListener of RocketComponentConfig (swing gui/configdialog/RocketComponentConfig.java,
/// 1025-1047) and of RocketConfig (RocketConfig.java:209-232) call the component's setters
/// themselves.
///
/// value() is the source's text of the main component. setText() is the field's write (Java's
/// setName(): `if (!component.getName().equals(field.getText())) component.setName(...)`): it
/// sets the text on every selected component (ComponentSelection::apply(); Java's setName() and
/// setComment() forward it to the config listeners, so a multi-edit renames every selected
/// component), unless it is value() already. The binding caches the text and the enabled state,
/// re-reads them once per event of the rocket and tells its listeners when they changed.
///
/// Deviations from OpenRocket: setText() is a widget write, dropped while the binding notifies
/// and once the selection is invalid (Java's listener has no such guard, and a closing dialog's
/// focus loss reaches the component). A source without a setter is a BugError, as is a source
/// whose getChecked gives an Error (only a DoubleBinding shows one).
class StringBinding final : public Binding
{
public:
    /// A binding of @p source on the components of @p selection.
    /// @throws BugError when @p selection is null, the source has no setter, or the source cannot
    ///         read the main component
    StringBinding(std::shared_ptr<ComponentSelection> selection, StringSource source);

    ~StringBinding() override;
    StringBinding(const StringBinding&)            = delete;
    StringBinding& operator=(const StringBinding&) = delete;
    StringBinding(StringBinding&&)                 = delete;
    StringBinding& operator=(StringBinding&&)      = delete;

    /// The cached text of the main component; once the selection is invalid, the last text read.
    [[nodiscard]] const std::string& value() const noexcept { return m_value; }

    /// Whether the field can be edited: the source's isEnabled of the main component (true
    /// without one); false once the selection is invalid.
    [[nodiscard]] bool isEnabled() const noexcept { return isValid() && m_enabled; }

    /// The field's write (see the class comment): sets @p text on every selected component unless
    /// it is value(); false, with nothing set, then, while firing and once the selection is
    /// invalid.
    bool setText(std::string_view text);

protected:
    bool readState() override;

private:
    StringSource m_source;
    std::string  m_value;
    bool         m_enabled{true};
};

/// The source of a component's name (RocketComponent::getName() and setName(), Java's name field:
/// a blank name gives the component's default name back).
[[nodiscard]] StringSource componentNameSource();

/// The source of a component's comment (RocketComponent::getComment() and setComment(), Java's
/// comment area).
[[nodiscard]] StringSource componentCommentSource();

}  // namespace QtRocket::Gui
