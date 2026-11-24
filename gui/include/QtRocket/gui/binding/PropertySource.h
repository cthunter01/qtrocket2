#pragma once

#include <concepts>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

#include "QtRocket/gui/binding/BindingContext.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/ObjectSelection.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket::Gui
{

/// How a binding reads a property of type T and writes it, built from lambdas (Java's DoubleModel
/// and its kin find the methods get<Name>, set<Name>, is<Name>Automatic and set<Name>Automatic,
/// is<Name>Enabled by reflection): of the components of a ComponentSelection (of()), or of the
/// one object of an ObjectSelection (ofObject(): the simulation options, a wind model or level,
/// the preferences). Lambdas pick the overload Java's reflection finds (the one-argument
/// setThickness(double) is setThickness(t, true)), call functions that need more than the
/// component (ComponentDrag::getOverrideCD(component, mach)), and reach the parameters a dialog
/// edits that are not the component's own (the default deployment configuration; their setter
/// fires the component's change event itself).
///
/// The accessors (Accessors<C>, of a component class C or another class O):
/// - get, or getChecked (exactly one): the value of the main component, which must be a C (a
///   BugError otherwise), or of the object. getChecked gives a Result: its Error's message is the
///   problem text a binding shows in place of the value (the override CD of a stage selection
///   that has no force analysis, aero/ComponentDrag.h).
/// - set (optional; read-only without): called through ComponentSelection::apply<C>() on every
///   selected component that is a C, or through ObjectSelection::apply<O>() on the object.
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
///     DoubleSource::ofObject<SimulationOptionsInterface>({
///         .get = [](const SimulationOptionsInterface& o) { return o.getLaunchRodLength(); },
///         .set = [](SimulationOptionsInterface& o, double length) { o.setLaunchRodLength(length);
///         },
///     })
///
/// A source reads and writes through the context its binding is made over: one of of() needs a
/// ComponentSelection, one of ofObject<O>() an ObjectSelection made for the class O
/// (ObjectSelection::make<O>()); another context is a BugError when the binding first reads it.
///
/// A plain value: copying it copies the lambdas.
template <class T>
class PropertySource
{
public:
    /// The accessors of a property of the class C (see the class comment).
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

    /// A source of the accessors @p accessors of the component class C, for a ComponentSelection.
    /// @throws BugError when not exactly one of get and getChecked is given, or only one of
    ///         isAutomatic and setAutomatic (Java ignores a lone automatic method)
    template <class C>
        requires std::derived_from<C, RocketComponent>
    [[nodiscard]] static PropertySource of(Accessors<C> accessors);

    /// A source of the accessors @p accessors of the class O, which is no component, for an
    /// ObjectSelection made for O (see the class comment).
    /// @throws BugError when not exactly one of get and getChecked is given, or only one of
    ///         isAutomatic and setAutomatic
    template <BindableObject O>
    [[nodiscard]] static PropertySource ofObject(Accessors<O> accessors);

    /// The value of the main component of @p context, a ComponentSelection, or of the object of
    /// @p context, an ObjectSelection; or the problem text as an Error.
    /// @throws BugError when the context is invalid or not the source's kind, or its main
    ///         component or object is not of the source's class
    [[nodiscard]] Result<T> get(const BindingContext& context) const { return m_get(context); }

    /// The value of the main component or the object of @p context, for a binding that shows no
    /// problem text (every binding but DoubleBinding).
    /// @throws BugError when the source gives an Error, the context is invalid or not the source's
    ///         kind, or its main component or object is not of the source's class
    [[nodiscard]] T getValue(const BindingContext& context) const
    {
        Result<T> value = get(context);
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
    /// (ComponentSelection::apply()), or on the object (ObjectSelection::apply()).
    /// @throws BugError when the source is read-only or the context is invalid or not the
    ///         source's kind
    void set(BindingContext& context, const T& value) const
    {
        if (!m_set)
        {
            bug("The property is read-only: it has no setter");
        }
        m_set(context, value);
    }

    /// Whether the property has an automatic state.
    [[nodiscard]] bool isAutomaticAvailable() const noexcept
    {
        return static_cast<bool>(m_isAutomatic);
    }

    /// Whether the main component's or the object's value is set automatically; false without an
    /// automatic state.
    /// @throws BugError when the context is invalid or not the source's kind
    [[nodiscard]] bool isAutomatic(const BindingContext& context) const
    {
        return m_isAutomatic && m_isAutomatic(context);
    }

    /// Sets the automatic state on every selected component of the source's class, or on the
    /// object.
    /// @throws BugError when the property has no automatic state or the context is invalid or not
    ///         the source's kind
    void setAutomatic(BindingContext& context, bool automatic) const
    {
        if (!m_setAutomatic)
        {
            bug("The property has no automatic state");
        }
        m_setAutomatic(context, automatic);
    }

    /// Whether the main component's or the object's field can be edited; true without an
    /// isEnabled accessor.
    /// @throws BugError when the context is invalid or not the source's kind
    [[nodiscard]] bool isEnabled(const BindingContext& context) const
    {
        return !m_isEnabled || m_isEnabled(context);
    }

private:
    PropertySource() = default;

    /// @p accessors' getters and automatic pair must be complete (see of()).
    template <class C>
    static void checkAccessors(const Accessors<C>& accessors)
    {
        if ((accessors.get == nullptr) == (accessors.getChecked == nullptr))
        {
            bug("A property source needs exactly one of get and getChecked");
        }
        if ((accessors.isAutomatic == nullptr) != (accessors.setAutomatic == nullptr))
        {
            bug("A property source needs both isAutomatic and setAutomatic, or neither");
        }
    }

    /// @p context as a ComponentSelection.
    /// @throws BugError when it is not one
    template <class Context>
    [[nodiscard]] static auto& selectionOf(Context& context)
    {
        using Selection = std::conditional_t<std::is_const_v<Context>, const ComponentSelection,
                                             ComponentSelection>;
        auto* selection = dynamic_cast<Selection*>(&context);
        if (selection == nullptr)
        {
            bug("A property of a component class is bound to a context that is no component "
                "selection");
        }
        return *selection;
    }

    /// @p context as an ObjectSelection.
    /// @throws BugError when it is not one
    template <class Context>
    [[nodiscard]] static auto& objectSelectionOf(Context& context)
    {
        using Selection =
            std::conditional_t<std::is_const_v<Context>, const ObjectSelection, ObjectSelection>;
        auto* selection = dynamic_cast<Selection*>(&context);
        if (selection == nullptr)
        {
            bug("A property of an object is bound to a context that is no object selection");
        }
        return *selection;
    }

    /// The main component of @p context as a C.
    /// @throws BugError when the context is no ComponentSelection, is invalid, or its main
    ///         component is not a C
    template <class C>
    [[nodiscard]] static const C& mainOf(const BindingContext& context)
    {
        const auto* typed = dynamic_cast<const C*>(&selectionOf(context).getMain());
        if (typed == nullptr)
        {
            bug("The main component is not of the class the property is defined for");
        }
        return *typed;
    }

    /// The object of @p context as an O.
    /// @throws BugError when the context is no ObjectSelection, is invalid, or was made for
    ///         another class
    template <class O>
    [[nodiscard]] static const O& objectOf(const BindingContext& context)
    {
        return objectSelectionOf(context).template getObject<O>();
    }

    std::function<Result<T>(const BindingContext&)> m_get;
    std::function<void(BindingContext&, const T&)>  m_set;
    std::function<bool(const BindingContext&)>      m_isAutomatic;
    std::function<void(BindingContext&, bool)>      m_setAutomatic;
    std::function<bool(const BindingContext&)>      m_isEnabled;
};

template <class T>
template <class C>
    requires std::derived_from<C, RocketComponent>
PropertySource<T> PropertySource<T>::of(Accessors<C> accessors)
{
    checkAccessors(accessors);
    PropertySource source;
    if (accessors.get != nullptr)
    {
        source.m_get = [get = std::move(accessors.get)](const BindingContext& context) {
            return Result<T>(get(mainOf<C>(context)));
        };
    }
    else
    {
        source.m_get = [get = std::move(accessors.getChecked)](const BindingContext& context) {
            return get(mainOf<C>(context));
        };
    }
    if (accessors.set != nullptr)
    {
        source.m_set = [set = std::move(accessors.set)](BindingContext& context, const T& value) {
            selectionOf(context).template apply<C>(
                [&set, &value](C& target) { set(target, value); });
        };
    }
    if (accessors.isAutomatic != nullptr)
    {
        source.m_isAutomatic =
            [is = std::move(accessors.isAutomatic)](const BindingContext& context) {
                return is(mainOf<C>(context));
            };
        source.m_setAutomatic = [set = std::move(accessors.setAutomatic)](BindingContext& context,
                                                                          bool automatic) {
            selectionOf(context).template apply<C>(
                [&set, automatic](C& target) { set(target, automatic); });
        };
    }
    if (accessors.isEnabled != nullptr)
    {
        source.m_isEnabled = [is = std::move(accessors.isEnabled)](const BindingContext& context) {
            return is(mainOf<C>(context));
        };
    }
    return source;
}

template <class T>
template <BindableObject O>
PropertySource<T> PropertySource<T>::ofObject(Accessors<O> accessors)
{
    checkAccessors(accessors);
    PropertySource source;
    if (accessors.get != nullptr)
    {
        source.m_get = [get = std::move(accessors.get)](const BindingContext& context) {
            return Result<T>(get(objectOf<O>(context)));
        };
    }
    else
    {
        source.m_get = [get = std::move(accessors.getChecked)](const BindingContext& context) {
            return get(objectOf<O>(context));
        };
    }
    if (accessors.set != nullptr)
    {
        source.m_set = [set = std::move(accessors.set)](BindingContext& context, const T& value) {
            objectSelectionOf(context).template apply<O>(
                [&set, &value](O& target) { set(target, value); });
        };
    }
    if (accessors.isAutomatic != nullptr)
    {
        source.m_isAutomatic =
            [is = std::move(accessors.isAutomatic)](const BindingContext& context) {
                return is(objectOf<O>(context));
            };
        source.m_setAutomatic = [set = std::move(accessors.setAutomatic)](BindingContext& context,
                                                                          bool automatic) {
            objectSelectionOf(context).template apply<O>(
                [&set, automatic](O& target) { set(target, automatic); });
        };
    }
    if (accessors.isEnabled != nullptr)
    {
        source.m_isEnabled = [is = std::move(accessors.isEnabled)](const BindingContext& context) {
            return is(objectOf<O>(context));
        };
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
