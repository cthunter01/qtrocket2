#include "QtRocket/gui/binding/DoubleBinding.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/gui/binding/Binding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/ExpressionParser.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

namespace
{

/// Whether @p a and @p b are the same value for the listeners: equal, or both NaN.
[[nodiscard]] bool sameValue(double a, double b) noexcept
{
    return a == b || (std::isnan(a) && std::isnan(b));
}

/// Java's check of getSpinnerModel(stepSize).
void checkStep(std::optional<double> fixedStep)
{
    const bool usable = !fixedStep.has_value() || (std::isfinite(*fixedStep) && *fixedStep > 0);
    if (!usable)
    {
        bug("A spinner step size must be finite and positive");
    }
}

}  // namespace

DoubleBinding::DoubleBinding(std::shared_ptr<ComponentSelection> selection, DoubleSource source,
                             const UnitGroup& units, Options options)
  : Binding(std::move(selection)),
    m_source(std::move(source)),
    m_units(&units),
    m_unit(&units.getDefaultUnit()),
    m_multiplier(options.multiplier),
    m_min(options.min),
    m_max(options.max),
    m_minBinding(std::move(options.minBinding)),
    m_maxBinding(std::move(options.maxBinding)),
    m_value(std::numeric_limits<double>::quiet_NaN()),
    m_lastMin(0),
    m_lastMax(0)
{
    if (getSelection() == nullptr)
    {
        bug("A binding of a property needs a component selection");
    }
    if (!std::isfinite(m_multiplier) || m_multiplier == 0)
    {
        bug("A binding's multiplier must be finite and not 0");
    }
    connectBound(m_minBinding, m_minConnection);
    connectBound(m_maxBinding, m_maxConnection);
    m_lastMin = minimum();
    m_lastMax = maximum();
    readState();
}

DoubleBinding::DoubleBinding(double value) : DoubleBinding(value, unitGroup(UnitGroupId::NONE)) { }

DoubleBinding::DoubleBinding(double value, const UnitGroup& units, double min, double max)
  : Binding(nullptr),
    m_units(&units),
    m_unit(&units.getDefaultUnit()),
    m_multiplier(1.0),
    m_min(min),
    m_max(max),
    m_value(value),
    m_lastMin(min),
    m_lastMax(max)
{
}

DoubleBinding::~DoubleBinding() = default;

void DoubleBinding::connectBound(const std::shared_ptr<DoubleBinding>& bound,
                                 Signal<>::ScopedConnection&           connection)
{
    if (bound != nullptr)
    {
        connection = bound->addChangeListener([this] { boundChanged(); });
    }
}

// ------------------------------------------------------------------------------------ state

bool DoubleBinding::readState()
{
    if (!m_source.has_value())
    {
        return false;  // a binding of its own value has no selection to read
    }
    const DoubleSource&       source    = *m_source;
    const ComponentSelection& selection = *getSelection();
    const Result<double>      read      = source.get(selection);
    const double              value =
        read.has_value() ? *read * m_multiplier : std::numeric_limits<double>::quiet_NaN();
    std::optional<std::string> problem;
    if (!read.has_value())
    {
        problem = read.error().message;
    }
    const bool automatic = source.isAutomatic(selection);
    const bool enabled   = source.isEnabled(selection);

    const bool changed = !sameValue(value, m_value) || automatic != m_automatic ||
                         enabled != m_enabled || problem != m_problem;
    m_value            = value;
    m_automatic        = automatic;
    m_enabled          = enabled;
    m_problem          = std::move(problem);
    return changed;
}

void DoubleBinding::boundChanged()
{
    const double min = minimum();
    const double max = maximum();
    if (sameValue(min, m_lastMin) && sameValue(max, m_lastMax))
    {
        return;
    }
    m_lastMin = min;
    m_lastMax = max;
    fireChanged();
}

bool DoubleBinding::isWidgetWriteAllowed() const noexcept
{
    return !isFiring() && isValid();
}

// ------------------------------------------------------------------------------------ value

void DoubleBinding::setValue(double value)
{
    const double clamped = MathUtil::clamp(value, minimum(), maximum());
    if (!m_source.has_value())
    {
        // A binding of its own value (Java: lastValue = v; fireStateChanged()).
        if (!sameValue(clamped, m_value))
        {
            m_value = clamped;
            fireChanged();
        }
        return;
    }
    const DoubleSource& source = *m_source;
    if (!source.isWritable())
    {
        bug("The binding's property is read-only: it has no setter");
    }
    if (!isValid())
    {
        return;
    }
    // The selection's event (or its own refresh) updates the cache and tells the listeners, one
    // of which may drop the last owner of this binding.
    const std::shared_ptr<Binding> keepAlive = weak_from_this().lock();
    source.set(*getSelection(), clamped / m_multiplier);
}

bool DoubleBinding::isWritable() const noexcept
{
    return !m_source.has_value() || m_source->isWritable();
}

double DoubleBinding::minimum() const noexcept
{
    return m_minBinding != nullptr ? m_minBinding->value() : m_min;
}

double DoubleBinding::maximum() const noexcept
{
    return m_maxBinding != nullptr ? m_maxBinding->value() : m_max;
}

// -------------------------------------------------------------------------------- automatic

bool DoubleBinding::isAutomaticAvailable() const noexcept
{
    return m_source.has_value() && m_source->isAutomaticAvailable();
}

void DoubleBinding::setAutomatic(bool automatic)
{
    if (!m_source.has_value())
    {
        return;
    }
    const DoubleSource& source = *m_source;
    if (!isWidgetWriteAllowed() || !source.isAutomaticAvailable())
    {
        return;
    }
    const std::shared_ptr<Binding> keepAlive = weak_from_this().lock();
    source.setAutomatic(*getSelection(), automatic);
}

// ------------------------------------------------------------------------------------ units

void DoubleBinding::setCurrentUnit(const Unit& unit)
{
    const int index = m_units->getUnitIndex(unit);
    if (index < 0)
    {
        bug("The unit '" + unit.getUnit() + "' is not in the binding's unit group");
    }
    const Unit& own = m_units->getUnit(index);
    if (&own == m_unit)
    {
        return;
    }
    m_unit = &own;
    fireChanged();
}

// -------------------------------------------------------------------------------- spin box

std::string DoubleBinding::text() const
{
    if (!isValid())
    {
        return {};
    }
    if (m_problem.has_value())
    {
        return *m_problem;
    }
    return m_unit->toString(m_value);
}

bool DoubleBinding::commitText(std::string_view text)
{
    if (!isWidgetWriteAllowed())
    {
        return false;
    }
    const Result<double> parsed = ExpressionParser::parse(text);
    if (!parsed.has_value() || !std::isfinite(*parsed))
    {
        return false;
    }
    const double converted = m_unit->fromUnit(*parsed);
    if (!std::isfinite(converted))
    {
        return false;
    }
    setValue(converted);
    return true;
}

std::optional<double> DoubleBinding::nextValue(std::optional<double> fixedStep) const
{
    checkStep(fixedStep);
    if (!isValid() || m_problem.has_value() || std::isnan(m_value))
    {
        return std::nullopt;
    }
    double       d        = m_unit->toUnit(m_value);
    const bool   inverted = m_unit->getMultiplier() < 0;
    const double max      = inverted ? m_unit->toUnit(minimum()) : m_unit->toUnit(maximum());
    if (MathUtil::equals(d, max))
    {
        return std::nullopt;
    }
    d = fixedStep.has_value() ? d + *fixedStep : m_unit->getNextValue(d);
    // Java: if (d > max) d = max.
    return std::min(d, max);
}

std::optional<double> DoubleBinding::previousValue(std::optional<double> fixedStep) const
{
    checkStep(fixedStep);
    if (!isValid() || m_problem.has_value() || std::isnan(m_value))
    {
        return std::nullopt;
    }
    double       d        = m_unit->toUnit(m_value);
    const bool   inverted = m_unit->getMultiplier() < 0;
    const double min      = inverted ? m_unit->toUnit(maximum()) : m_unit->toUnit(minimum());
    if (MathUtil::equals(d, min))
    {
        return std::nullopt;
    }
    d = fixedStep.has_value() ? d - *fixedStep : m_unit->getPreviousValue(d);
    // Java: if (d < min) d = min.
    return std::max(d, min);
}

bool DoubleBinding::setUnitValue(double valueInUnit)
{
    if (!isWidgetWriteAllowed() || !std::isfinite(valueInUnit))
    {
        return false;
    }
    const double converted = m_unit->fromUnit(valueInUnit);
    if (!std::isfinite(converted))
    {
        return false;
    }
    setValue(converted);
    return true;
}

bool DoubleBinding::stepUp(std::optional<double> fixedStep)
{
    const std::optional<double> next = nextValue(fixedStep);
    return next.has_value() && setUnitValue(*next);
}

bool DoubleBinding::stepDown(std::optional<double> fixedStep)
{
    const std::optional<double> previous = previousValue(fixedStep);
    return previous.has_value() && setUnitValue(*previous);
}

// ---------------------------------------------------------------------------------- slider

int DoubleBinding::sliderPosition(const SliderMapping& mapping) const
{
    return mapping.positionOf(m_value, *m_unit);
}

void DoubleBinding::setSliderPosition(const SliderMapping& mapping, int position)
{
    if (!isWidgetWriteAllowed())
    {
        return;
    }
    setValue(mapping.valueAt(position, *m_unit));
}

}  // namespace QtRocket::Gui
