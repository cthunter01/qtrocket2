#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/util/Signal.h"

namespace QtRocket
{

class RocketComponent;

/// A change in a rocket component tree: which component changed (the source) and what kind of
/// change it was, as a bit mask of the Type values (OpenRocket's ComponentChangeEvent). Components
/// fire them through RocketComponent::fireComponentChangeEvent(); the root Rocket updates its
/// modification ids, lets every component react (componentChanged()) and emits them to its
/// listeners.
///
/// OpenRocket's ComponentChangeListener, StateChangeListener and ChangeSource are not ported:
/// listeners are slots of a ComponentChangeSignal (Rocket::addComponentChangeListener()), and a
/// ComponentChangeAdapter is not needed since a slot takes the event directly.
///
/// The source is a non-owning pointer, valid while the component exists; a listener must not keep
/// it beyond the call. The frozen events of Rocket::freeze() are resolved by id when the rocket
/// thaws (see there).
class ComponentChangeEvent
{
public:
    /// The change types (Java: ComponentChangeEvent.TYPE), with their bit values.
    enum class Type
    {
        ERROR          = -1,
        NON_FUNCTIONAL = 1,
        MASS           = 2,
        AERODYNAMIC    = 4,
        TREE           = 8,
        UNDO           = 16,
        MOTOR          = 32,
        EVENT          = 64,
        TEXTURE        = 128,
        GRAPHIC        = 256,
        TREE_CHILDREN  = 512,
    };

    /// Every type in declaration order (Java: TYPE.values()).
    static constexpr std::array<Type, 11> kAllTypes{
        Type::ERROR,   Type::NON_FUNCTIONAL, Type::MASS,         Type::AERODYNAMIC,
        Type::TREE,    Type::UNDO,           Type::MOTOR,        Type::EVENT,
        Type::TEXTURE, Type::GRAPHIC,        Type::TREE_CHILDREN};

    /// A change that does not affect simulation results in any way (name, color, ...).
    static constexpr int kNonFunctionalChange = 1;
    /// A change that affects the mass properties of the rocket.
    static constexpr int kMassChange = 2;
    /// A change that affects the aerodynamic properties of the rocket.
    static constexpr int kAerodynamicChange = 4;
    /// A change that affects the mass and aerodynamic properties of the rocket.
    static constexpr int kAeromassChange = kMassChange | kAerodynamicChange;
    /// The same as kAeromassChange (Java: BOTH_CHANGE, kept for backward compatibility).
    static constexpr int kBothChange = kAeromassChange;
    /// A change that affects the rocket tree structure.
    static constexpr int kTreeChange = 8;
    /// A change caused by undo/redo.
    static constexpr int kUndoChange = 16;
    /// A change in the motor configurations or names.
    static constexpr int kMotorChange = 32;
    /// A change that affects the events occurring during flight.
    static constexpr int kEventChange = 64;
    /// A change to the 3D texture assigned to a component.
    static constexpr int kTextureChange = 128;
    /// A UI-only change (a flight configuration fires this type).
    static constexpr int kGraphicChange = 256;
    /// A change that affects the children's tree structure.
    static constexpr int kTreeChangeChildren = 512;

    /// The bit value of @p type (Java: TYPE.value).
    [[nodiscard]] static constexpr int value(Type type) noexcept { return static_cast<int>(type); }

    /// Whether @p type's bit is set in @p mask (Java: TYPE.matches()).
    [[nodiscard]] static constexpr bool matches(Type type, int mask) noexcept
    {
        return (value(type) & mask) != 0;
    }

    /// The type whose value is exactly @p typeNumber, or nullopt (Java: getTypeEnum(), which
    /// throws IllegalArgumentException).
    [[nodiscard]] static std::optional<Type> typeFromValue(int typeNumber) noexcept;

    /// The type's name as Java spells it: "Error", "nonFunctional", "Mass", "Aerodynamic",
    /// "TREE", "UNDO", "Motor", "Event", "Texture", "Configuration", "TREE_CHILDREN".
    [[nodiscard]] static std::string_view typeName(Type type) noexcept;

    /// An event of the change types in @p type (any combination of the k...Change bits) from
    /// @p source, which must not be null (Java's EventObject throws IllegalArgumentException).
    ComponentChangeEvent(RocketComponent* source, int type);

    /// An event of the single change @p type from @p source.
    /// @throws BugError when @p type is Type::ERROR (Java: IllegalArgumentException "no event
    ///         type provided") or @p source is null.
    ComponentChangeEvent(RocketComponent* source, Type type);

    /// The component the change comes from.
    [[nodiscard]] RocketComponent* getSource() const noexcept { return m_source; }

    /// The change bits.
    [[nodiscard]] int getType() const noexcept { return m_type; }

    [[nodiscard]] bool isAerodynamicChange() const noexcept
    {
        return matches(Type::AERODYNAMIC, m_type);
    }
    [[nodiscard]] bool isEventChange() const noexcept { return matches(Type::EVENT, m_type); }
    /// Any change that is not purely non-functional (see isNonFunctionalChange()).
    [[nodiscard]] bool isFunctionalChange() const noexcept { return !isNonFunctionalChange(); }
    /// True when no bit other than NON_FUNCTIONAL, TEXTURE and GRAPHIC is set (texture and
    /// graphic changes are visual only); an event of type 0 is non-functional too.
    [[nodiscard]] bool isNonFunctionalChange() const noexcept;
    [[nodiscard]] bool isMassChange() const noexcept { return matches(Type::MASS, m_type); }
    [[nodiscard]] bool isTextureChange() const noexcept { return matches(Type::TEXTURE, m_type); }
    [[nodiscard]] bool isTreeChange() const noexcept { return matches(Type::TREE, m_type); }
    [[nodiscard]] bool isTreeChildrenChange() const noexcept
    {
        return matches(Type::TREE_CHILDREN, m_type);
    }
    [[nodiscard]] bool isUndoChange() const noexcept { return matches(Type::UNDO, m_type); }
    [[nodiscard]] bool isMotorChange() const noexcept { return matches(Type::MOTOR, m_type); }

    /// "ComponentChangeEvent[nonfunc,mass,aero,tree,treechild,undo,motor,event]" with only the
    /// flags that apply, in that order; texture and graphic changes are not listed (as in Java).
    [[nodiscard]] std::string toString() const;

private:
    RocketComponent* m_source;
    int              m_type;
};

/// The signal a Rocket emits its change events on (Java: the ComponentChangeListener list).
using ComponentChangeSignal = Signal<const ComponentChangeEvent&>;

}  // namespace QtRocket
