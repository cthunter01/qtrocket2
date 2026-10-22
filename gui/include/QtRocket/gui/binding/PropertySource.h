#pragma once

#include <concepts>
#include <functional>
#include <string>
#include <utility>

#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket::Gui
{

/// How a binding reads a property of type T from the main component of a ComponentSelection and
/// writes it to every selected component, built from lambdas (Java's DoubleModel and its kin find
/// the methods get<Name>, set<Name>, is<Name>Automatic and set<Name>Automatic, is<Name>Enabled by
/// reflection). Lambdas pick the overload Java's reflection finds (the one-argument
/// setThickness(double) is setThickness(t, true)), call functions that need more than the
/// component (ComponentDrag::getOverrideCD(component, mach)), and reach the parameters a dialog
/// edits that are not the component's own (the default deployment configuration; their setter
/// fires the component's change event itself).
///
/// The accessors (Accessors<C>, a component class C):
/// - get, or getChecked (exactly one): the value of the main component, which must be a C (a
///   BugError otherwise). getChecked gives a Result: its Error's message is the problem text a
///   binding shows in place of the value (the override CD of a stage selection that has no force
///   analysis, aero/ComponentDrag.h).
/// - set (optional; read-only without): called through ComponentSelection::apply<C>() on every
///   selected component that is a C.
/// - isAutomatic and setAutomatic (both or none): the automatic state (Java's DoubleModel looks
///   for is<Name>Automatic and set<Name>Automatic, and uses them only when both exist).
/// - isEnabled (optional; enabled without): whether the field can be edited (Java's BooleanModel
///   looks for is<Name>Enabled; Transition::isClippedEnabled()).
///
///     DoubleSource::of<BodyTube>({
///         .get          = [](const BodyTube& tube) { return tube.getOuterRadius(); },
///         .set          = [](BodyTube& tube, double radius) { tube.setOuterRadius(radius); },
///         .isAutomatic  = [](const BodyTube& tube) { return tube.isOuterRadiusAutomatic(); },
///         .setAutomatic = [](BodyTube& tube, bool on) { tube.setOuterRadiusAutomatic(on); },
///     })
///
/// A plain value: copying it copies the lambdas.
template <class T>
class PropertySource
{
public:
    /// The accessors of a property of the component class C (see the class comment).
    template <class C>
    struct Accessors
    {
        std::function<T(const C&)>         get          = nullptr;
        std::function<Result<T>(const C&)> getChecked   = nullptr;
        std::function<void(C&, const T&)>  set          = nullptr;
        std::function<bool(const C&)>      isAutomatic  = nullptr;
        std::function<void(C&, bool)>      setAutomatic = nullptr;
        std::function<bool(const C&)>      isEnabled    = nullptr;
    };

    /// A source of the accessors @p accessors.
    /// @throws BugError when not exactly one of get and getChecked is given, or only one of
    ///         isAutomatic and setAutomatic (Java ignores a lone automatic method)
    template <class C>
        requires std::derived_from<C, RocketComponent>
    [[nodiscard]] static PropertySource of(Accessors<C> accessors);

    /// The value of the main component of @p selection, or the problem text as an Error.
    /// @throws BugError when the selection is invalid or its main component is not of the
    ///         source's class
    [[nodiscard]] Result<T> get(const ComponentSelection& selection) const
    {
        return m_get(selection.getMain());
    }

    /// The value of the main component of @p selection, for a binding that shows no problem text
    /// (every binding but DoubleBinding).
    /// @throws BugError when the source gives an Error, the selection is invalid or its main
    ///         component is not of the source's class
    [[nodiscard]] T getValue(const ComponentSelection& selection) const
    {
        Result<T> value = get(selection);
        if (!value.has_value())
        {
            bug("Only a number field shows a problem text, the property gave one: " +
                value.error().message);
        }
        return std::move(*value);
    }

    /// Whether the source has a setter.
    [[nodiscard]] bool isWritable() const noexcept { return static_cast<bool>(m_set); }

    /// Sets @p value on every selected component of the source's class
    /// (ComponentSelection::apply()).
    /// @throws BugError when the source is read-only or the selection is invalid
    void set(ComponentSelection& selection, const T& value) const
    {
        if (!m_set)
        {
            bug("The property is read-only: it has no setter");
        }
        m_set(selection, value);
    }

    /// Whether the property has an automatic state.
    [[nodiscard]] bool isAutomaticAvailable() const noexcept
    {
        return static_cast<bool>(m_isAutomatic);
    }

    /// Whether the main component's value is set automatically; false without an automatic
    /// state.
    /// @throws BugError when the selection is invalid
    [[nodiscard]] bool isAutomatic(const ComponentSelection& selection) const
    {
        return m_isAutomatic && m_isAutomatic(selection.getMain());
    }

    /// Sets the automatic state on every selected component of the source's class.
    /// @throws BugError when the property has no automatic state or the selection is invalid
    void setAutomatic(ComponentSelection& selection, bool automatic) const
    {
        if (!m_setAutomatic)
        {
            bug("The property has no automatic state");
        }
        m_setAutomatic(selection, automatic);
    }

    /// Whether the main component's field can be edited; true without an isEnabled accessor.
    /// @throws BugError when the selection is invalid
    [[nodiscard]] bool isEnabled(const ComponentSelection& selection) const
    {
        return !m_isEnabled || m_isEnabled(selection.getMain());
    }

private:
    PropertySource() = default;

    /// @p component as a C.
    /// @throws BugError when it is not one
    template <class C>
    [[nodiscard]] static const C& as(const RocketComponent& component)
    {
        const auto* typed = dynamic_cast<const C*>(&component);
        if (typed == nullptr)
        {
            bug("The main component is not of the class the property is defined for");
        }
        return *typed;
    }

    std::function<Result<T>(const RocketComponent&)>   m_get;
    std::function<void(ComponentSelection&, const T&)> m_set;
    std::function<bool(const RocketComponent&)>        m_isAutomatic;
    std::function<void(ComponentSelection&, bool)>     m_setAutomatic;
    std::function<bool(const RocketComponent&)>        m_isEnabled;
};

template <class T>
template <class C>
    requires std::derived_from<C, RocketComponent>
PropertySource<T> PropertySource<T>::of(Accessors<C> accessors)
{
    if ((accessors.get == nullptr) == (accessors.getChecked == nullptr))
    {
        bug("A property source needs exactly one of get and getChecked");
    }
    if ((accessors.isAutomatic == nullptr) != (accessors.setAutomatic == nullptr))
    {
        bug("A property source needs both isAutomatic and setAutomatic, or neither");
    }
    PropertySource source;
    if (accessors.get != nullptr)
    {
        source.m_get = [get = std::move(accessors.get)](const RocketComponent& component) {
            return Result<T>(get(as<C>(component)));
        };
    }
    else
    {
        source.m_get = [get = std::move(accessors.getChecked)](const RocketComponent& component) {
            return get(as<C>(component));
        };
    }
    if (accessors.set != nullptr)
    {
        source.m_set = [set = std::move(accessors.set)](ComponentSelection& selection,
                                                        const T&            value) {
            selection.apply<C>([&set, &value](C& target) { set(target, value); });
        };
    }
    if (accessors.isAutomatic != nullptr)
    {
        source.m_isAutomatic =
            [is = std::move(accessors.isAutomatic)](const RocketComponent& component) {
                return is(as<C>(component));
            };
        source.m_setAutomatic = [set = std::move(accessors.setAutomatic)](
                                    ComponentSelection& selection, bool automatic) {
            selection.apply<C>([&set, automatic](C& target) { set(target, automatic); });
        };
    }
    if (accessors.isEnabled != nullptr)
    {
        source.m_isEnabled = [is = std::move(accessors.isEnabled)](
                                 const RocketComponent& component) { return is(as<C>(component)); };
    }
    return source;
}

/// The source of a DoubleBinding.
using DoubleSource = PropertySource<double>;
/// The source of an integer property (fin counts, instance counts, line counts).
using IntSource = PropertySource<int>;
/// The source of a check box's property.
using BoolSource = PropertySource<bool>;
/// The source of an enum property (a combo box of the enum's values).
template <class E>
using EnumSource = PropertySource<E>;
/// The source of a text property (a name, a comment).
using StringSource = PropertySource<std::string>;

}  // namespace QtRocket::Gui
