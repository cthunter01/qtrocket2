#pragma once

#include <memory>

#include <QObject>
#include <QSlider>

#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/util/Signal.h"

class QWheelEvent;
class QWidget;

namespace QtRocket::Gui
{

class DoubleBinding;

/// The slider of a number field (OpenRocket: a BasicSlider on one of DoubleModel's
/// getSliderModel()s): a horizontal QSlider from 0 to SliderMapping::kMaximum whose position is
/// its DoubleBinding's value under a SliderMapping, linear or quadratic. Moving it sets the value
/// of the position, rounded in the binding's current unit (DoubleBinding::setSliderPosition());
/// whenever the binding tells of a change (its value, its unit: a unit whose multiplier is
/// negative turns the slider round) the slider takes the value's position, and after every move
/// it shows where the value ended up (a rounding or a clamp may put it a little off the mouse,
/// as in Java). QSlider tells of every value set, also the slider's own: a refresh is no move.
///
/// The bounds of the mapping may follow other bindings (DynamicBounds; Java: a DoubleModel given
/// as getSliderModel()'s min or max, as an inner diameter's slider ends at the outer diameter):
/// when one of them changes, the mapping takes its new bounds (SliderMapping::setBounds(), the
/// constant one kept) and the slider moves to the value's new position. The mapping is made with
/// the bound bindings' current values, as Java's slider models read them when they are made:
///
///     makeChild<ValueSlider>(panel, inner, SliderMapping::linear(0, outer->value()),
///                            ValueSlider::DynamicBounds{.max = outer});
///
/// The slider takes no focus and no wheel (Java's BasicSlider is not focusable, and neither
/// Swing's BasicSliderUI nor FlatLaf's slider takes the wheel): a turn of the wheel over it goes
/// to the widgets around it, so that a scroll area scrolls. A click on the groove moves it by a
/// tenth of its range, as Swing's BasicSliderUI.scrollByBlock(). Enabled: the binding's isEnabled()
/// and setFieldEnabled() together, as UnitSpinBox.
class ValueSlider : public QSlider
{
    Q_OBJECT

public:
    /// The bindings whose values a bound of the mapping follows; null for a constant bound.
    struct DynamicBounds
    {
        std::shared_ptr<DoubleBinding> min = nullptr;
        std::shared_ptr<DoubleBinding> max = nullptr;
    };

    /// A slider of @p binding under @p mapping, whose bounds are constant.
    /// @throws BugError when @p binding is null
    ValueSlider(std::shared_ptr<DoubleBinding> binding, const SliderMapping& mapping,
                QWidget* parent);
    /// A slider of @p binding under @p mapping, whose bounds follow @p bounds.
    /// @throws BugError when @p binding is null
    ValueSlider(std::shared_ptr<DoubleBinding> binding, const SliderMapping& mapping,
                DynamicBounds bounds, QWidget* parent);
    ~ValueSlider() override;
    ValueSlider(const ValueSlider&)            = delete;
    ValueSlider& operator=(const ValueSlider&) = delete;
    ValueSlider(ValueSlider&&)                 = delete;
    ValueSlider& operator=(ValueSlider&&)      = delete;

    [[nodiscard]] const std::shared_ptr<DoubleBinding>& binding() const noexcept
    {
        return m_binding;
    }
    [[nodiscard]] const SliderMapping& mapping() const noexcept { return m_mapping; }
    [[nodiscard]] const DynamicBounds& dynamicBounds() const noexcept { return m_bounds; }

    /// Shows and sets @p binding under @p mapping and @p bounds from now on (Java: setModel() with
    /// another slider model: InnerTubeConfig's separation, relative or absolute).
    /// @throws BugError when @p binding is null
    void setBinding(std::shared_ptr<DoubleBinding> binding, const SliderMapping& mapping,
                    DynamicBounds bounds);
    /// The same with constant bounds.
    void setBinding(std::shared_ptr<DoubleBinding> binding, const SliderMapping& mapping);

    /// Whether the field may be edited as far as the dialog is concerned (see UnitSpinBox).
    [[nodiscard]] bool isFieldEnabled() const noexcept { return m_fieldEnabled; }
    void               setFieldEnabled(bool enabled);

protected:
    void wheelEvent(QWheelEvent* event) override;

private:
    void connectBindings();
    /// Shows the value's position and the enabled state.
    void refresh();
    /// The slider moved to @p position (QSlider::valueChanged): the value of that position.
    void moved(int position);
    /// A bound binding changed: the mapping takes the bounds, the slider the value's position.
    void boundChanged();

    std::shared_ptr<DoubleBinding> m_binding;
    SliderMapping                  m_mapping;
    DynamicBounds                  m_bounds;
    Signal<>::ScopedConnection     m_connection;
    Signal<>::ScopedConnection     m_minConnection;
    Signal<>::ScopedConnection     m_maxConnection;
    bool                           m_fieldEnabled{true};
    /// Set while refresh() puts the slider at the value's position: that is no move.
    bool m_refreshing{false};
};

}  // namespace QtRocket::Gui
