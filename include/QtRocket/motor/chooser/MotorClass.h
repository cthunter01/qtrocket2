#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace QtRocket
{

/// The NAR motor classes of the motor chooser's information panel (OpenRocket's swing MotorClass,
/// gui/dialogs/motor/thrustcurve/MotorClass.java: "Total impulse: 8.82 Ns (76% C)" and the
/// tooltip "Class C (5 Ns - 10 Ns)"), in declaration order: 1/8A [0, 0.3125] Ns, 1/4A, 1/2A, A
/// (1.25, 2.5], then doubling up to O (20480, 40960], and OVER above that.
///
/// The texts are English, as in OpenRocket, which does not translate them. getDescription() and
/// getClassDescription() compose them exactly as OpenRocket does; a GUI that translates them
/// composes its own from className(), getPercent(), getMin() and getMax().
///
/// These functions throw BugError for a value outside the enum (a programming error).
enum class MotorClass
{
    A18,  ///< "1/8A"
    A14,  ///< "1/4A"
    A12,  ///< "1/2A"
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
    OVER,  ///< "> O": above 40960 Ns
};

/// MotorClass.values(), in declaration order.
inline constexpr std::array<MotorClass, 19> kAllMotorClasses{
    MotorClass::A18, MotorClass::A14, MotorClass::A12, MotorClass::A,    MotorClass::B,
    MotorClass::C,   MotorClass::D,   MotorClass::E,   MotorClass::F,    MotorClass::G,
    MotorClass::H,   MotorClass::I,   MotorClass::J,   MotorClass::K,    MotorClass::L,
    MotorClass::M,   MotorClass::N,   MotorClass::O,   MotorClass::OVER,
};

/// The name of the enum constant (name()): "A18", "A14", "A12", "A", ..., "O", "OVER".
[[nodiscard]] std::string_view enumName(MotorClass motorClass);

/// The class's name in the texts (the className field): "1/8A", "1/4A", "1/2A", "A", ..., "O" and
/// "> O" for OVER.
[[nodiscard]] std::string_view className(MotorClass motorClass);

/// The lower end of the class in Ns (min): 0 for 1/8A, 0.3125 for 1/4A, ..., 40960 for OVER.
[[nodiscard]] double getMin(MotorClass motorClass);

/// The upper end of the class in Ns (max): 0.3125 for 1/8A, ..., 40960 for O and the largest double
/// (Double.MAX_VALUE) for OVER.
[[nodiscard]] double getMax(MotorClass motorClass);

/// The percentage getDescription() shows: (impulse - min) / (max - min) * 100, raised to 1 when
/// below 1 ("0% looks stupid"), rounded as Java's Math.round does (MathUtil::javaRound(), so a
/// NaN impulse gives 0). OVER has none: getDescription() shows "Over O" instead.
[[nodiscard]] std::int64_t getPercent(MotorClass motorClass, double impulse);

/// getDescription(impulse): "<getPercent()>% <className()>" ("76% C", String.format("%d%% %s")),
/// and "Over O" for OVER.
[[nodiscard]] std::string getDescription(MotorClass motorClass, double impulse);

/// getClassDescription(): "Class <className()> (<min> - <max>)" with both ends in the impulse
/// unit group's default unit ("Class C (5 Ns - 10 Ns)", UnitGroup.UNITS_IMPULSE.toStringUnit()),
/// and "Over O-class (over <40960 in that unit>)" for OVER ("Over O-class (over 40960 Ns)").
[[nodiscard]] std::string getClassDescription(MotorClass motorClass);

/// MotorClass.getMotorClass(): the class of @p impulse (in Ns). An impulse of 100 Ns or more is
/// first rounded to the nearest integer, half to even (Math.rint, so that 640.1 Ns, shown as
/// "640 Ns", is an I); then it is the first class whose max, plus 1e-7, is not below it: a negative
/// impulse is 1/8A, and a class's upper end belongs to it (10 Ns is a C).
/// @throws BugError for NaN and +infinity, which fit no class ("Could not find motor class for
///         impulse <impulse>", the impulse as Java's Double.toString writes it, as OpenRocket's
///         BugException says).
[[nodiscard]] MotorClass getMotorClass(double impulse);

}  // namespace QtRocket
