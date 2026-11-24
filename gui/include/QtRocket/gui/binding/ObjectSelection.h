#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <type_traits>
#include <typeinfo>

#include "QtRocket/gui/binding/BindingContext.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

/// An object a binding can be made over: any class but a component's, which goes with the
/// rocket's TREE and UNDO changes and is bound through a ComponentSelection.
template <class O>
concept BindableObject = std::is_class_v<O> && !std::is_const_v<O> &&
                         !std::derived_from<std::remove_cv_t<O>, RocketComponent>;

/// The bindings' context (BindingContext) for one object that is no component but tells of its
/// changes through a Signal<>: the simulation options (SimulationOptions, or the launch
/// preferences through SimulationOptionsInterface), a PinkNoiseWindModel, a level of a
/// multi-level wind model (MultiLevelPinkNoiseWindModel::LevelWindModel), the Preferences.
/// OpenRocket's DoubleModel, BooleanModel, IntegerModel and EnumModel take such an object as
/// their source, find its get/set methods by reflection and listen to it when it is a
/// ChangeSource (DoubleModel.java:764-766: "programmatic changes, such as resetting simulation
/// conditions, are propagated to the Swing widgets"); here the bindings read and write it through
/// a PropertySource::ofObject() of its class, and this selection listens to its signal.
///
/// The object's class: make() is given it explicitly, and a source reads and writes the object
/// only when it is of the same class (PropertySource::ofObject<O>(), getObject<O>(): a source of
/// SimulationOptionsInterface needs a selection made with make<SimulationOptionsInterface>(),
/// whatever the object's own class). A source of another class is a BugError when the binding
/// first reads it, which is when it is made.
///
/// Refreshing: on every emission of the object's signal the selection refreshes its bindings,
/// Phase::READ then Phase::NOTIFY, as a ComponentSelection does on an event of the rocket; and
/// apply() refreshes them itself when the call it makes emitted nothing (a setter that changed
/// nothing, or one that announces nothing: a preference stored without changed()), so that they
/// are current when it returns.
///
/// Lifetime: the selection holds a pointer to the object, never the object. It learns that the
/// object has gone from its signal: the signal is destroyed with the object (a member of it, or
/// of something it owns), and its connection then reports itself disconnected (Signal). From
/// then on the selection is invalid: it touches the object no more, getObject() and apply() are
/// BugErrors, and its bindings drop every write a widget makes. Nothing is told then (an object
/// that goes calls nobody); the owner of an object that destroys it while widgets show it (a wind
/// level removed from its model) calls invalidate() first, which tells the bindings
/// (Phase::INVALIDATED), as a TREE or UNDO change does for components. It is invalid as well once
/// release() ended it (a closing dialog, widgets/BindingScope::keepContext()) and once the
/// signal's connections were removed (Signal::disconnectAll()). Two preconditions, as the
/// selection cannot see them:
/// - The object must not be moved while the selection is valid: a move hands the signal's
///   connections to the new object (Signal's move, PinkNoiseWindModel's, SimulationOptions'),
///   while the selection still points at the old one.
/// - The signal must not outlive the object: an adapter whose changed() is another object's
///   signal (an interface over the preferences) is released or invalidated by its owner before
///   it goes.
///
/// Make it with make(): the bindings share it (Binding), and a listener may drop the last owner
/// while it is being told (a dialog that closes on a change): the selection keeps itself alive
/// until it has told every listener. Not copyable or movable (the signal's slot refers to it); GUI
/// thread only, like the objects: its object's signal must be emitted on the GUI thread only, so
/// an object a worker thread changes is never bound.
// NOLINTNEXTLINE(misc-multiple-inheritance): enable_shared_from_this carries the keep-alive
class ObjectSelection final : public BindingContext,
                              public std::enable_shared_from_this<ObjectSelection>
{
    /// Only make() makes one (std::make_shared needs a public constructor).
    struct Token
    {
        explicit Token() = default;
    };

    /// The object, of the class make() was given.
    struct Holder
    {
        Holder()                         = default;
        virtual ~Holder()                = default;
        Holder(const Holder&)            = delete;
        Holder& operator=(const Holder&) = delete;
        Holder(Holder&&)                 = delete;
        Holder& operator=(Holder&&)      = delete;

        /// The name of the class, for the BugError of a source of another class.
        [[nodiscard]] virtual std::string_view className() const noexcept = 0;
    };

    template <class O>
    struct TypedHolder final : Holder
    {
        explicit TypedHolder(O& held) noexcept : object(&held) { }

        [[nodiscard]] std::string_view className() const noexcept override
        {
            return typeid(O).name();
        }

        O* object;
    };

public:
    /// A selection of @p object, refreshed on every emission of @p changed (see the class
    /// comment), which must live exactly as long as the object.
    template <BindableObject O>
    [[nodiscard]] static std::shared_ptr<ObjectSelection> make(std::type_identity_t<O>& object,
                                                               Signal<>&                changed)
    {
        return std::make_shared<ObjectSelection>(Token{}, std::make_unique<TypedHolder<O>>(object),
                                                 changed);
    }

    /// A selection of @p object, refreshed on every emission of its changed() signal.
    template <BindableObject O>
        requires requires(O& object) {
            { object.changed() } -> std::same_as<Signal<>&>;
        }
    [[nodiscard]] static std::shared_ptr<ObjectSelection> make(std::type_identity_t<O>& object)
    {
        return make<O>(object, object.changed());
    }

    /// Use make().
    ObjectSelection(Token /*token*/, std::unique_ptr<Holder> object, Signal<>& changed);

    ~ObjectSelection() override;
    ObjectSelection(const ObjectSelection&)            = delete;
    ObjectSelection& operator=(const ObjectSelection&) = delete;
    ObjectSelection(ObjectSelection&&)                 = delete;
    ObjectSelection& operator=(ObjectSelection&&)      = delete;

    /// False once the object's signal has gone (with the object) or lost its connections, and
    /// once invalidate() or release() ended the selection.
    [[nodiscard]] bool isValid() const noexcept override;

    /// Whether the selection was made for objects of the class O (make<O>()).
    template <class O>
    [[nodiscard]] bool holds() const noexcept
    {
        return dynamic_cast<const TypedHolder<O>*>(m_object.get()) != nullptr;
    }

    /// The object.
    /// @throws BugError when the selection is invalid or was made for another class than O
    template <class O>
    [[nodiscard]] O& getObject() const
    {
        checkValid();
        const auto* typed = dynamic_cast<const TypedHolder<O>*>(m_object.get());
        if (typed == nullptr)
        {
            wrongClass(typeid(O).name());
        }
        return *typed->object;
    }

    /// Calls @p f on the object (std::invoke(f, getObject<O>())), the write of a binding. When the
    /// object's signal did not reach the selection during the call (nothing changed, or the
    /// change announces nothing), the selection refreshes its bindings itself afterwards, so that
    /// they are current when apply() returns.
    /// @throws BugError when the selection is invalid or was made for another class than O;
    ///         whatever @p f throws
    template <class O, class F>
    void apply(const F& f)
    {
        O& object = getObject<O>();
        applyCall([&f, &object] { std::invoke(f, object); });
    }

    /// Refreshes the bindings (Phase::READ, then Phase::NOTIFY), as an emission of the object's
    /// signal does: for a change that announces nothing. Nothing happens once the selection is
    /// invalid.
    void refresh() override;

    /// Adds a listener of the phases (the bindings).
    PhaseSignal::Connection addListener(PhaseSignal::Slot slot) override;

    /// Ends the selection and tells its listeners once (Phase::INVALIDATED): for the owner of the
    /// object, before it destroys the object while widgets show it (see the class comment); after
    /// the object has gone it still tells them. Nothing happens once invalidate() or release()
    /// ended the selection.
    void invalidate();

    /// Ends the selection on purpose without telling anybody (a dialog that closes:
    /// BindingContext::release()); invalidate() tells nobody afterwards. Nothing happens once
    /// invalidate() or release() ended the selection.
    void release() noexcept override;

private:
    void objectChanged();
    void applyCall(const std::function<void()>& call);
    /// Lets go of the object and of the signal's connection.
    void dropObject() noexcept;
    void checkValid() const;
    /// @throws BugError, always: a source of the class @p wanted on a selection of another
    [[noreturn]] void wrongClass(std::string_view wanted) const;

    std::unique_ptr<Holder>    m_object;
    Signal<>::ScopedConnection m_connection;
    PhaseSignal                m_listeners;
    /// The emissions of the object's signal received so far (apply() refreshes when none
    /// arrived).
    std::uint64_t m_changeCount{0};
    bool          m_ended{false};
};

}  // namespace QtRocket::Gui
