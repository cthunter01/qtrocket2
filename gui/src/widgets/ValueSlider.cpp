#include "QtRocket/gui/widgets/ValueSlider.h"

#include <memory>
#include <utility>

#include <QPointer>
#include <QScopedValueRollback>
#include <QSlider>
#include <QWheelEvent>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

namespace
{

/// @p binding, which must not be null.
[[nodiscard]] std::shared_ptr<DoubleBinding> checkedBinding(std::shared_ptr<DoubleBinding> binding)
{
    if (binding == nullptr)
    {
        bug("A value slider needs a binding");
    }
    return binding;
}

}  // namespace

ValueSlider::ValueSlider(std::shared_ptr<DoubleBinding> binding, const SliderMapping& mapping,
                         QWidget* parent)
  : ValueSlider(std::move(binding), mapping, DynamicBounds{}, parent)
{
}

ValueSlider::ValueSlider(std::shared_ptr<DoubleBinding> binding, const SliderMapping& mapping,
                         DynamicBounds bounds, QWidget* parent)
  : QSlider(Qt::Horizontal, parent),
    m_binding(checkedBinding(std::move(binding))),
    m_mapping(mapping),
    m_bounds(std::move(bounds))
{
    setRange(0, SliderMapping::kMaximum);
    // A click on the groove moves a tenth of the range (Swing's BasicSliderUI.scrollByBlock()).
    setPageStep(SliderMapping::kMaximum / 10);
    // Java's BasicSlider: not focusable, no labels.
    setFocusPolicy(Qt::NoFocus);
    setTickPosition(QSlider::NoTicks);
    connect(this, &QSlider::valueChanged, this, [this](int position) {
        Guard::run([this, position] { moved(position); }, objectName());
    });
    connectBindings();
    refresh();
}

ValueSlider::~ValueSlider() = default;

void ValueSlider::connectBindings()
{
    m_connection =
        m_binding->addChangeListener([this] { Guard::run([this] { refresh(); }, objectName()); });
    m_minConnection.disconnect();
    m_maxConnection.disconnect();
    if (m_bounds.min != nullptr)
    {
        m_minConnection = m_bounds.min->addChangeListener(
            [this] { Guard::run([this] { boundChanged(); }, objectName()); });
    }
    if (m_bounds.max != nullptr)
    {
        m_maxConnection = m_bounds.max->addChangeListener(
            [this] { Guard::run([this] { boundChanged(); }, objectName()); });
    }
}

void ValueSlider::setBinding(std::shared_ptr<DoubleBinding> binding, const SliderMapping& mapping,
                             DynamicBounds bounds)
{
    std::shared_ptr<DoubleBinding> checked = checkedBinding(std::move(binding));
    m_connection.disconnect();
    m_binding = std::move(checked);
    m_mapping = mapping;
    m_bounds  = std::move(bounds);
    connectBindings();
    refresh();
}

void ValueSlider::setBinding(std::shared_ptr<DoubleBinding> binding, const SliderMapping& mapping)
{
    setBinding(std::move(binding), mapping, DynamicBounds{});
}

void ValueSlider::setFieldEnabled(bool enabled)
{
    m_fieldEnabled = enabled;
    setEnabled(m_fieldEnabled && m_binding->isEnabled());
}

void ValueSlider::refresh()
{
    const QScopedValueRollback<bool> refreshing(m_refreshing, true);
    setValue(m_binding->sliderPosition(m_mapping));
    setEnabled(m_fieldEnabled && m_binding->isEnabled());
}

void ValueSlider::moved(int position)
{
    if (m_refreshing)
    {
        return;
    }
    // The binding's listeners may replace or drop it, or delete a dialog with deleteLater().
    const std::shared_ptr<DoubleBinding> binding = m_binding;
    const QPointer<ValueSlider>          self(this);
    binding->setSliderPosition(m_mapping, position);
    if (!self.isNull())
    {
        refresh();  // where the value ended up: a rounding or a clamp fires nothing when the
                    // value stays
    }
}

void ValueSlider::boundChanged()
{
    // Java's slider model reads its bound models' values; the constant bound stays.
    const double min = m_bounds.min != nullptr ? m_bounds.min->value() : m_mapping.getMin();
    const double max = m_bounds.max != nullptr ? m_bounds.max->value() : m_mapping.getMax();
    m_mapping.setBounds(min, max);
    refresh();
}

void ValueSlider::wheelEvent(QWheelEvent* event)
{
    event->ignore();  // on to the widgets around it: a scroll area scrolls
}

}  // namespace QtRocket::Gui
