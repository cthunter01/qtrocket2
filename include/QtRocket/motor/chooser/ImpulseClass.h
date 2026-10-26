#pragma once

#include <array>
#include <string_view>

namespace QtRocket
{

/// The total impulse classes of the motor chooser's "Total Impulse" filter (OpenRocket's swing
/// ImpulseClass, gui/dialogs/motor/thrustcurve/ImpulseClass.java), in declaration order: the
/// filter panel's two-thumb slider runs over them from A to O, and MotorRowFilter keeps a motor set
/// whose total impulse lies above the low end of the minimum class and not above the high end of
/// the maximum one.
///
/// Each class spans (low, high]: A from -1 Ns ("to catch the MMX motors", whose impulse rounds to
/// 0) to 2.5 Ns, then doubling, B (2.5, 5] to N (10240, 20480], and O from 20480 Ns to the largest
/// double. (MotorClass is the information panel's finer NAR classification.)
enum class ImpulseClass
{
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
};

/// ImpulseClass.values(), in declaration order: the slider's stops.
inline constexpr std::array<ImpulseClass, 15> kAllImpulseClasses{
    ImpulseClass::A, ImpulseClass::B, ImpulseClass::C, ImpulseClass::D, ImpulseClass::E,
    ImpulseClass::F, ImpulseClass::G, ImpulseClass::H, ImpulseClass::I, ImpulseClass::J,
    ImpulseClass::K, ImpulseClass::L, ImpulseClass::M, ImpulseClass::N, ImpulseClass::O,
};

/// The class's letter, "A" to "O": its toString() and its name(), the slider's tick labels.
///
/// These functions throw BugError for a value outside the enum (a programming error).
[[nodiscard]] std::string_view toString(ImpulseClass impulseClass);

/// The low end of the class in Ns (getLow()), excluded from it: -1 for A, 2.5 for B, ...
[[nodiscard]] double getLow(ImpulseClass impulseClass);

/// The high end of the class in Ns (getHigh()), included in it: 2.5 for A, ..., 20480 for N and
/// the largest double (Double.MAX_VALUE) for O.
[[nodiscard]] double getHigh(ImpulseClass impulseClass);

}  // namespace QtRocket
