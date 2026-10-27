#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "QtRocket/gui/binding/Binding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

/// The model of a combo box of an enum's values (Java: EnumModel, swing
/// gui/adaptors/EnumModel.java, and TransitionShapeModel, see makeTransitionShapeBinding()): the
/// placement's axial method, a surface finish, a fin cross section, a mass component's type, a
/// deployment, separation or ignition event.
///
/// The values are the ones given, all the enum's (the core's kAll... arrays) or a subset (Java:
/// the values array, e.g. AxialMethod.axialOffsetMethods), in that order; removeElement() and
/// removeElementAt() hide some of them (Java: the combo box's removeItem(), e.g. the deployment
/// at the separation of a lower stage on the bottom stage). size(), elementAt() and indexOf() are
/// the displayed ones (Java: getSize(), getElementAt()). The text of a value is the widget's
/// business (util/EnumText: displayName() through tr()), so that the binding stays Qt-free.
///
/// value() is the source's value of the main component (Java: getSelectedItem()), which may be a
/// hidden one or one not given at all: then indexOf() gives -1 and the widget shows it apart.
/// select() sets a value on every selected component (ComponentSelection::apply()); nothing
/// happens when it is value() already, so that in a multi-edit picking the main component's value
/// leaves the others alone. Java skips it twice: JComboBox.setSelectedItem() calls its model only
/// for an item other than the one it shows (JDK 17 JComboBox.java:582-620, selectedItemReminder),
/// which covers TransitionShapeModel too, and EnumModel.setSelectedItem() skips its current value
/// ("no duplicate writes"). The binding caches the
/// value and the enabled state, re-reads them once per event of the rocket and tells its
/// listeners when they or the displayed values changed.
///
/// A binding without a selection holds a value of its own (Java: an EnumModel of an object that is
/// no component: the flight configuration dialogs' event combo boxes edit the mount's motor
/// configuration or a copy of a deployment or separation configuration): it is always valid and
/// enabled, and select() changes that value and tells the listeners (dropped while they are told,
/// as every widget write is).
///
/// Deviations from OpenRocket:
/// - Java's EnumModel reads its source once, when it is made: it is not a listener of it and its
///   combo shows a value changed elsewhere only after its own setSelectedItem(). Here the binding
///   re-reads once per event (the scout's decision D16); TransitionShapeModel re-reads too.
/// - select() is a widget write: dropped while the binding notifies (a combo box refreshing
///   itself) and once the selection is invalid. Java has no such guard (its combo never writes
///   back what it is told); and Java's EnumModel takes the new value as the current one before the
///   setter runs, so that its combo shows a value a setter refused, where here value() stays what
///   the component holds.
/// - Hiding a value tells the listeners (Java's removeElement() tells nobody; its combo reads the
///   list when it opens).
/// - Not ported: the null text (Java's EnumModel(source, name, values, nullText): a String item
///   that sets null, which no dialog uses: QtRocket's enums have no null), addElement() and
///   insertElementAt() (no-ops in Java). A source without a setter is a BugError (Java refuses a
///   source without set<Name>), as is a source whose getChecked gives an Error when it does
///   (only a DoubleBinding shows one).
template <class E>
    requires std::is_enum_v<E>
class EnumBinding final : public Binding
{
public:
    /// A binding of @p source on the components of @p selection, offering @p values.
    /// @throws BugError when @p selection is null, the source has no setter, or the source cannot
    ///         read the main component
    EnumBinding(std::shared_ptr<ComponentSelection> selection, EnumSource<E> source,
                std::span<const E> values)
      : Binding(std::move(selection)),
        m_source(std::move(source)),
        m_values(values.begin(), values.end()),
        m_displayed(m_values)
    {
        if (getSelection() == nullptr)
        {
            bug("A binding of a property needs a component selection");
        }
        if (!m_source->isWritable())
        {
            bug("An enum combo box needs a setter (Java: EnumModel requires set<Name>)");
        }
        readState();
    }

    /// A binding of its own value @p value (see the class comment), offering @p values.
    EnumBinding(E value, std::span<const E> values)
      : Binding(nullptr),
        m_values(values.begin(), values.end()),
        m_displayed(m_values),
        m_value(value)
    {
    }

    ~EnumBinding() override                    = default;
    EnumBinding(const EnumBinding&)            = delete;
    EnumBinding& operator=(const EnumBinding&) = delete;
    EnumBinding(EnumBinding&&)                 = delete;
    EnumBinding& operator=(EnumBinding&&)      = delete;

    /// The cached value of the main component (Java: getSelectedItem()), or the binding's own
    /// value; once the selection is invalid, the last value read.
    [[nodiscard]] E value() const noexcept { return m_value; }

    /// The combo box's write (Java: setSelectedItem()): sets @p value on every selected
    /// component, or as the binding's own value, see the class comment. False, with nothing set,
    /// when it is value(), while firing and once the selection is invalid.
    bool select(E value)
    {
        if (!isWidgetWriteAllowed() || value == m_value)
        {
            return false;
        }
        if (!m_source.has_value())
        {
            // A binding of its own value (Java: EnumModel.setSelectedItem() sets the object's
            // property and fires).
            m_value = value;
            fireChanged();
            return true;
        }
        const EnumSource<E>& source = *m_source;
        // The selection's event (or its own refresh) updates the cache and tells the listeners,
        // one of which may drop the last owner of this binding.
        const std::shared_ptr<Binding> keepAlive = weak_from_this().lock();
        source.set(*getSelection(), value);
        return true;
    }

    /// Whether the combo box can be used: the source's isEnabled of the main component (true
    /// without one, and for a binding of its own value); false once the selection is invalid.
    [[nodiscard]] bool isEnabled() const noexcept { return isValid() && m_enabled; }

    /// The values given, hidden ones included (Java: values).
    [[nodiscard]] std::span<const E> getValues() const noexcept { return m_values; }

    /// The values shown, in order (Java: displayedValues).
    [[nodiscard]] std::span<const E> displayedValues() const noexcept { return m_displayed; }

    /// The number of values shown (Java: getSize()).
    [[nodiscard]] int size() const noexcept { return static_cast<int>(m_displayed.size()); }

    /// The value shown at @p index, or nullopt out of range (Java: getElementAt(), null).
    [[nodiscard]] std::optional<E> elementAt(int index) const noexcept
    {
        if (index < 0 || index >= size())
        {
            return std::nullopt;
        }
        return m_displayed[static_cast<std::size_t>(index)];
    }

    /// The index of @p value among the values shown, or -1.
    [[nodiscard]] int indexOf(E value) const noexcept
    {
        const auto found = std::ranges::find(m_displayed, value);
        if (found == m_displayed.end())
        {
            return -1;
        }
        return static_cast<int>(std::distance(m_displayed.begin(), found));
    }

    /// Hides @p value, its first occurrence among the values shown (Java: removeElement());
    /// nothing when it is not shown.
    void removeElement(E value)
    {
        const int index = indexOf(value);
        if (index >= 0)
        {
            removeElementAt(index);
        }
    }

    /// Hides the value shown at @p index (Java: removeElementAt()); nothing out of range.
    void removeElementAt(int index)
    {
        if (index < 0 || index >= size())
        {
            return;  // Java: "bad parameter"
        }
        m_displayed.erase(std::next(m_displayed.begin(), index));
        fireChanged();
    }

protected:
    // MSVC instantiates a class template's virtual member functions with the class, GCC and Clang
    // only when they are used; the body is valid for every enum E, so either is fine.
    // NOLINTNEXTLINE(portability-template-virtual-member-function): valid for every E
    bool readState() override
    {
        if (!m_source.has_value())
        {
            return false;  // a binding of its own value has no selection to read
        }
        const EnumSource<E>&      source    = *m_source;
        const ComponentSelection& selection = *getSelection();
        const E                   value     = source.getValue(selection);
        const bool                enabled   = source.isEnabled(selection);
        const bool                changed   = value != m_value || enabled != m_enabled;
        m_value                             = value;
        m_enabled                           = enabled;
        return changed;
    }

private:
    /// nullopt for a binding of its own value.
    std::optional<EnumSource<E>> m_source;
    std::vector<E>               m_values;
    std::vector<E>               m_displayed;
    E                            m_value{};
    bool                         m_enabled{true};
};

}  // namespace QtRocket::Gui
