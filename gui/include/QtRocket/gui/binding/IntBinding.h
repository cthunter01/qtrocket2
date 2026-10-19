#pragma once

#include <limits>
#include <memory>
#include <optional>

#include "QtRocket/gui/binding/Binding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/PropertySource.h"

namespace QtRocket::Gui
{

/// The model of a count field, an integer spin box (Java: IntegerModel with its
/// IntegerSpinnerModel, swing gui/adaptors/IntegerModel.java): the fin count (1 to 8), an
/// instance count (from 1), a parachute's line count (from 0).
///
/// The value is the source's value of the main component. setValue() clamps it to [minimum(),
/// maximum()] and sets it on every selected component (ComponentSelection::apply()), also when
/// it is the main component's value already (Java calls the setter every time; the main's setter
/// forwards it to the others). The binding caches the value and the enabled state, re-reads them
/// once per event of the rocket and tells its listeners when they changed.
///
/// The spin box: nextValue() is the value plus one and previousValue() the value minus one,
/// nothing at the maximum and the minimum (Java: null), so that a value beyond a bound (a file's
/// fin count of 10) steps back in. The text is the widget's (a QSpinBox, which takes plain
/// integers only, as Java's editor does: its Integer formatter reads no expression, "2*3" and
/// "4.0" revert). The widget writes (setSpinnerValue(), stepUp(), stepDown()) are dropped while
/// the binding notifies (isFiring(): a spin box refreshing itself) and once the selection is
/// invalid.
///
/// Deviations from OpenRocket:
/// - Java's IntegerModel is not a listener of its source: it reads the source when asked, and its
///   spinner shows a value changed elsewhere only when the model itself fires. Here the binding
///   re-reads once per event and tells its listeners (the scout's decision D16), and only of a
///   change (Java's setValue() fires every time; the event here does it).
/// - A minimum above the maximum, and a source without a setter, are BugErrors (Java clamps with
///   such bounds; it refuses a source without "set" + name). A source whose getChecked gives an
///   Error is a BugError when it does: only a DoubleBinding shows a problem text.
/// - Not ported: the slider model (getSliderModel()), which no dialog uses.
class IntBinding final : public Binding
{
public:
    /// A binding of @p source on the components of @p selection, bounded by [@p min, @p max]
    /// (Java: IntegerModel(source, name, min, max); the defaults are Integer.MIN_VALUE and
    /// Integer.MAX_VALUE, as its shorter constructors).
    /// @throws BugError when @p selection is null, the source has no setter, @p min > @p max, or
    ///         the source cannot read the main component
    IntBinding(std::shared_ptr<ComponentSelection> selection, IntSource source,
               int min = std::numeric_limits<int>::min(),
               int max = std::numeric_limits<int>::max());

    ~IntBinding() override;
    IntBinding(const IntBinding&)            = delete;
    IntBinding& operator=(const IntBinding&) = delete;
    IntBinding(IntBinding&&)                 = delete;
    IntBinding& operator=(IntBinding&&)      = delete;

    /// The cached value of the main component (Java: getValue()); once the selection is invalid,
    /// the last value read.
    [[nodiscard]] int value() const noexcept { return m_value; }

    /// Clamps @p value to [minimum(), maximum()] and sets it on every selected component (Java:
    /// setValue()). Nothing happens once the selection is invalid. Not guarded by isFiring(), as
    /// in Java: code may set a value from a listener.
    void setValue(int value);

    [[nodiscard]] int minimum() const noexcept { return m_min; }
    [[nodiscard]] int maximum() const noexcept { return m_max; }

    /// Whether the field can be edited: the source's isEnabled of the main component (true
    /// without one); false once the selection is invalid.
    [[nodiscard]] bool isEnabled() const noexcept { return isValid() && m_enabled; }

    /// The value one step up (Java: the spinner model's getNextValue()): value() + 1, or nullopt
    /// when value() >= maximum() and once the selection is invalid.
    [[nodiscard]] std::optional<int> nextValue() const noexcept;

    /// The value one step down: value() - 1, or nullopt when value() <= minimum() and once the
    /// selection is invalid.
    [[nodiscard]] std::optional<int> previousValue() const noexcept;

    /// Sets @p value from the spin box (Java: the spinner model's setValue(Object)): a widget
    /// write; false, with nothing set, while firing and once the selection is invalid.
    bool setSpinnerValue(int value);

    /// setSpinnerValue(nextValue()); false when there is no next value or nothing was set.
    bool stepUp();

    /// setSpinnerValue(previousValue()); false when there is none or nothing was set.
    bool stepDown();

protected:
    bool readState() override;

private:
    IntSource m_source;
    int       m_min;
    int       m_max;
    int       m_value{0};
    bool      m_enabled{true};
};

}  // namespace QtRocket::Gui
