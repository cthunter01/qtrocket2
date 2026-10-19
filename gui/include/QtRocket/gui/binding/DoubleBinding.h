#pragma once

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/gui/binding/Binding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{
class Unit;
class UnitGroup;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class SliderMapping;

/// The model of a number field: a spin box, its unit selector, its slider and its "Automatic"
/// check box share one (Java: DoubleModel with its ValueSpinnerModel, ValueSliderModel and
/// AutomaticActionModel, swing gui/adaptors/DoubleModel.java).
///
/// The value is the source's value of the main component times the multiplier (a diameter field
/// binds the outer radius with multiplier 2), in SI units; setValue() clamps it to [minimum(),
/// maximum()], each bound a constant, another binding's value (Java: a DoubleModel given as the
/// bound) or infinite, and gives the setter value / multiplier for every selected component
/// (ComponentSelection::apply()). The binding caches the value, the automatic and enabled states
/// and the problem text, re-reads them once per event of the rocket and tells its listeners only
/// when they, the current unit or a dynamic bound changed (Java's stateChanged() compares the
/// value and the automatic state; its setValue() fires every time, which here the event does).
///
/// The current unit is the unit group's default unit when the binding is made, and changes for
/// this binding alone (setCurrentUnit(): the unit selector); the default stays (Java: the same,
/// the default units change only in the preferences).
///
/// The spin box: text() is currentUnit().toString(value()); commitText() reads an expression in
/// the current unit (util/ExpressionParser), converts it and sets it; nextValue() and
/// previousValue() give the next and the previous value in the current unit, by the unit's own
/// step (Unit::getNextValue()) or a fixed step in the current unit (Java's
/// getSpinnerModel(stepSize): the shape parameters, the fin points), nothing at the bound,
/// clamped to it otherwise, and the other way round for a unit whose multiplier is negative.
/// The slider: sliderPosition() and setSliderPosition() with a SliderMapping. Every write that
/// comes from a widget (commitText(), setUnitValue(), stepUp(), stepDown(), setSliderPosition(),
/// setAutomatic()) is dropped while the binding notifies (isFiring()) and once the selection is
/// invalid (a dialog's last focus-out commit after a TREE or UNDO change): it reaches no
/// component.
///
/// The widget shows text() again after every commit, whatever came of it: a commit that changes
/// nothing (the same number written differently, invalid text) fires nothing.
///
/// Deviations from OpenRocket:
/// - Invalid text, a NaN result ("0/0") and an infinite one ("1/0", the user's decision U4: Java
///   clamps it to the bound, or stores Infinity in an unbounded field) set nothing: commitText()
///   returns false. Java sets the last value again, which a multi-edit forwards to every other
///   component (the user's decision U3, see ComponentSelection), so that the user's typo there
///   overwrote the others with the main component's value.
/// - A dynamic bound that moves past the value does not set the value again (Java's stateChanged()
///   of a bound calls setValue(getValue()), which calls the setter; only OpenRocket's component
///   analysis, Milestone 7, gives a value a dynamic bound: the dialogs give them to sliders).
/// - The listeners hear of a change of the enabled state, the problem text and a dynamic bound as
///   well, and a NaN value is the same NaN on the next event (Java's lastValue == v is false for
///   NaN, so it fires on every event).
/// - nextValue() and previousValue() give nothing for a NaN value or a problem (Java steps NaN to
///   NaN).
class DoubleBinding final : public Binding
{
public:
    /// The multiplier and the bounds. A bound binding (minBinding, maxBinding) takes the place of
    /// the constant (Java: minModel before minValue); its value is the bound.
    struct Options
    {
        double                         multiplier = 1.0;
        double                         min        = -std::numeric_limits<double>::infinity();
        double                         max        = std::numeric_limits<double>::infinity();
        std::shared_ptr<DoubleBinding> minBinding = nullptr;
        std::shared_ptr<DoubleBinding> maxBinding = nullptr;
    };

    /// A binding of @p source on the components of @p selection, shown in @p units (which must
    /// outlive it; the process-wide groups of unitGroup() do).
    /// @throws BugError when @p selection is null, the multiplier is 0 or not finite, or the
    ///         source cannot read the main component
    DoubleBinding(std::shared_ptr<ComponentSelection> selection, DoubleSource source,
                  const UnitGroup& units, Options options);

    /// A binding of its own value @p value (Java: DoubleModel(value, units, min, max)), with the
    /// multiplier 1: a constant bound, or a field of a dialog that edits no component.
    explicit DoubleBinding(double value);
    DoubleBinding(double value, const UnitGroup& units,
                  double min = -std::numeric_limits<double>::infinity(),
                  double max = std::numeric_limits<double>::infinity());

    ~DoubleBinding() override;
    DoubleBinding(const DoubleBinding&)            = delete;
    DoubleBinding& operator=(const DoubleBinding&) = delete;
    DoubleBinding(DoubleBinding&&)                 = delete;
    DoubleBinding& operator=(DoubleBinding&&)      = delete;

    // ------------------------------------------------------------------------------ the value

    /// The cached value (Java: getValue()), the source's value times the multiplier; NaN while
    /// there is a problem. Once the selection is invalid, the last value read.
    [[nodiscard]] double value() const noexcept { return m_value; }

    /// Clamps @p value to [minimum(), maximum()] and sets value / multiplier on every selected
    /// component (Java: setValue()). Nothing happens once the selection is invalid. Not guarded
    /// by isFiring(), as in Java: code may set a value from a listener.
    /// @throws BugError when the source is read-only (Java: BugException)
    void setValue(double value);

    /// Whether setValue() can set a value (a binding of its own value always can).
    [[nodiscard]] bool isWritable() const noexcept;

    [[nodiscard]] double getMultiplier() const noexcept { return m_multiplier; }

    /// The bounds of the value (the multiplied value): the bound binding's value when there is
    /// one, otherwise the constant.
    [[nodiscard]] double minimum() const noexcept;
    [[nodiscard]] double maximum() const noexcept;

    /// The source's problem text in place of the value (getChecked's Error), or nullopt.
    [[nodiscard]] const std::optional<std::string>& getProblem() const noexcept
    {
        return m_problem;
    }

    /// Whether the field can be edited: the source's isEnabled of the main component (true
    /// without one); false once the selection is invalid.
    [[nodiscard]] bool isEnabled() const noexcept { return isValid() && m_enabled; }

    // -------------------------------------------------------------------------- automatic

    /// Whether the source has an automatic state (Java: isAutomaticAvailable()).
    [[nodiscard]] bool isAutomaticAvailable() const noexcept;

    /// The cached automatic state of the main component; false without one.
    [[nodiscard]] bool isAutomatic() const noexcept { return m_automatic; }

    /// Sets the automatic state on every selected component (Java: the automatic action's
    /// putValue()): a widget write, dropped while firing, once the selection is invalid and when
    /// there is no automatic state.
    void setAutomatic(bool automatic);

    // ------------------------------------------------------------------------------ units

    [[nodiscard]] const UnitGroup& getUnitGroup() const noexcept { return *m_units; }
    [[nodiscard]] const Unit&      getCurrentUnit() const noexcept { return *m_unit; }

    /// Shows the value in @p unit, a unit of the binding's group, from now on (Java:
    /// setCurrentUnit()); the listeners hear of it when it is another unit.
    /// @throws BugError when @p unit is not in the group
    void setCurrentUnit(const Unit& unit);

    // -------------------------------------------------------------------------- the spin box

    /// What the spin box shows: getCurrentUnit().toString(value()), the problem text while there
    /// is one, nothing ("") once the selection is invalid.
    [[nodiscard]] std::string text() const;

    /// Reads @p text as an expression in the current unit and sets its value (Java: the spinner
    /// model's setValue(String)): true when it did. False, with nothing set, for text that is not
    /// an expression, whose value or converted value is NaN or infinite, while firing and once
    /// the selection is invalid (see the class comment).
    /// @throws BugError when the source is read-only
    bool commitText(std::string_view text);

    /// The value one step up in the current unit (Java: getNextValue()); nullopt at the maximum
    /// (MathUtil::equals()), for a NaN value or a problem, and once the selection is invalid.
    /// @p fixedStep, in the current unit, replaces the unit's own step.
    /// @throws BugError when @p fixedStep is not finite and positive (Java:
    ///         IllegalArgumentException)
    [[nodiscard]] std::optional<double> nextValue(std::optional<double> fixedStep = {}) const;

    /// The value one step down; the mirror of nextValue().
    [[nodiscard]] std::optional<double> previousValue(std::optional<double> fixedStep = {}) const;

    /// Sets @p valueInUnit, a value in the current unit (Java: the spinner model's
    /// setValue(Number)): a widget write; false, with nothing set, when it is not finite, while
    /// firing and once the selection is invalid.
    bool setUnitValue(double valueInUnit);

    /// setUnitValue(nextValue(fixedStep)); false when there is no next value or nothing was set.
    bool stepUp(std::optional<double> fixedStep = {});

    /// setUnitValue(previousValue(fixedStep)); false when there is none or nothing was set.
    bool stepDown(std::optional<double> fixedStep = {});

    // ---------------------------------------------------------------------------- the slider

    /// The slider position of the value under @p mapping, in the current unit.
    [[nodiscard]] int sliderPosition(const SliderMapping& mapping) const;

    /// Sets the value of the slider position @p position under @p mapping, rounded in the current
    /// unit: a widget write, dropped while firing and once the selection is invalid.
    void setSliderPosition(const SliderMapping& mapping, int position);

protected:
    bool readState() override;

private:
    void               connectBound(const std::shared_ptr<DoubleBinding>& bound,
                                    Signal<>::ScopedConnection&           connection);
    void               boundChanged();
    [[nodiscard]] bool isWidgetWriteAllowed() const noexcept;

    std::optional<DoubleSource>    m_source;
    const UnitGroup*               m_units;
    const Unit*                    m_unit;
    double                         m_multiplier;
    double                         m_min;
    double                         m_max;
    std::shared_ptr<DoubleBinding> m_minBinding;
    std::shared_ptr<DoubleBinding> m_maxBinding;
    Signal<>::ScopedConnection     m_minConnection;
    Signal<>::ScopedConnection     m_maxConnection;

    double                     m_value;
    bool                       m_automatic{false};
    bool                       m_enabled{true};
    std::optional<std::string> m_problem;
    /// The bounds of the last boundChanged(): a bound binding's change is told on only when it
    /// moves the bound.
    double m_lastMin;
    double m_lastMax;
};

}  // namespace QtRocket::Gui
