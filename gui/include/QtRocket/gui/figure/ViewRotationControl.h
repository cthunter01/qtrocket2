#pragma once

#include <memory>

#include <QObject>
#include <QPointer>
#include <QWidget>

#include "QtRocket/util/Signal.h"

class QEvent;
class QToolButton;

namespace QtRocket
{
class Preferences;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class DoubleBinding;
class RocketFigure;
class UnitSelector;
class UnitSpinBox;
class ValueSlider;

/// The rotation control on the left of the design view (OpenRocket's ViewRotationControl): the
/// roll rotation of a RocketFigure about the rocket's axis, from 0 to 2 pi.
///
/// - At the top a spin box without step buttons, 50 pixels wide, and the angle's unit selector
///   beside it; below them an inverted vertical slider from 0 (at the top) to 2 pi (at the
///   bottom) that takes the rest of the height; at the bottom the lock toggle (24 x 24 pixels,
///   the icon UNLOCKED, LOCKED while it is checked). The spin box, the unit selector and the
///   slider share one DoubleBinding of the angle unit group, the figure's rotation: their object
///   name is the Java property, "Rotation", and their tooltip RocketPanel.ttip.Rotation.
/// - The figure and the binding follow each other (Java's DoubleModel(figure, "Rotation", ...),
///   which the figure tells of every change): a rotation of the figure (a drag in the design
///   view) shows in the fields, and a value of a field rotates the figure, clamped to 0 .. 2 pi
///   (DoubleModel.setValue()).
/// - Holding Shift while the slider's thumb is dragged snaps the rotation to the nearest multiple
///   of 30 or 45 degrees, whichever is nearer (snapRotation(), which the design view's drag uses
///   too).
/// - The lock (isDragRotationLocked()) is the preference LockClickDragRotation
///   (SwingPreferences.isClickDragRotationLocked()), read when the control is made and written
///   when the user toggles the button; while it is on, a left drag in the design view pans
///   instead of rotating. Every change of it is told by dragRotationLockChanged().
///
/// The figure must outlive the control or be destroyed with the same parent (the design view's
/// scroll area); once it is gone, the fields rotate nothing. The preferences must outlive the
/// control.
///
/// Deviations from OpenRocket:
/// - The fields show the figure's rotation as it is; Java's spinner does too, but a value of the
///   fields is clamped on its way to the figure here (DoubleModel clamps it on its way in), so
///   that a figure turned outside 0 .. 2 pi by a long drag keeps its angle until a field is used.
/// - The Shift state of a slider drag is read from the slider's own mouse events, as Java's
///   listeners read it; the snap is made when the binding tells of a value of the dragged slider
///   (Java: the slider's ChangeListener with getValueIsAdjusting()).
class ViewRotationControl final : public QWidget
{
    Q_OBJECT

public:
    /// The spin box's width ("width 50!").
    static constexpr int kSpinnerWidth = 50;
    /// The lock button's size (24 x 24).
    static constexpr int kLockButtonSize = 24;
    /// How far a snapped rotation must lie from the slider's value for the snap to set it
    /// (Java's "> 0.001", against setting the value again and again).
    static constexpr double kSnapTolerance = 0.001;

    /// The rotation control of @p figure, the lock read from @p preferences.
    ViewRotationControl(RocketFigure& figure, Preferences& preferences, QWidget* parent);
    ~ViewRotationControl() override;
    ViewRotationControl(const ViewRotationControl&)            = delete;
    ViewRotationControl& operator=(const ViewRotationControl&) = delete;
    ViewRotationControl(ViewRotationControl&&)                 = delete;
    ViewRotationControl& operator=(ViewRotationControl&&)      = delete;

    /// ViewRotationControl.snapRotation(): @p angle in radians snapped to the nearest multiple of
    /// 30 degrees or of 45 degrees, whichever is nearer (30 on a tie), in [0, 2 pi), with Java's
    /// arithmetic (Math.toDegrees(), Math.round(), %, Math.toRadians()): a NaN angle gives 0.
    [[nodiscard]] static double snapRotation(double angle) noexcept;

    /// The binding the fields share: the figure's rotation in the angle unit group.
    [[nodiscard]] const std::shared_ptr<DoubleBinding>& binding() const noexcept
    {
        return m_binding;
    }
    [[nodiscard]] UnitSpinBox*  spinner() const noexcept { return m_spinner; }
    [[nodiscard]] UnitSelector* unitSelector() const noexcept { return m_unitSelector; }
    /// The slider (getRotationSlider()).
    [[nodiscard]] ValueSlider* rotationSlider() const noexcept { return m_slider; }
    [[nodiscard]] QToolButton* lockButton() const noexcept { return m_lockButton; }

    /// Whether a left drag in the design view no longer rotates the figure
    /// (isDragRotationLocked()).
    [[nodiscard]] bool isDragRotationLocked() const noexcept { return m_locked; }
    /// Checks or unchecks the lock (setDragRotationLocked()); the preference is left alone, as in
    /// Java (only the user's toggle stores it).
    void setDragRotationLocked(bool locked);

    /// Watches the slider's mouse events for the Shift key.
    bool eventFilter(QObject* watched, QEvent* event) override;

Q_SIGNALS:
    /// The lock was turned on (@p locked) or off.
    void dragRotationLockChanged(bool locked);

private:
    /// The figure's rotation changed: the fields show it.
    void figureChanged();
    /// The binding changed: the figure takes its value, snapped while Shift drags the slider.
    void bindingChanged();
    /// The lock button was toggled to @p locked.
    void lockToggled(bool locked);
    /// The user clicked the lock button: the preference takes its state.
    void lockClicked(bool locked);
    void updateLockIcon();

    QPointer<RocketFigure>         m_figure;
    Preferences*                   m_preferences;
    std::shared_ptr<DoubleBinding> m_binding;
    Signal<>::ScopedConnection     m_bindingConnection;
    /// The panel of the spin box and the unit selector (Java's controlsPanel).
    QWidget*      m_controls;
    UnitSpinBox*  m_spinner;
    UnitSelector* m_unitSelector;
    ValueSlider*  m_slider;
    QToolButton*  m_lockButton;
    bool          m_locked{false};
    /// Whether Shift was held at the slider's last mouse press or drag (Java's shiftKeyPressed).
    bool m_shiftPressed{false};
    /// Set while the fields take the figure's rotation: that is not a value for the figure.
    bool m_followingFigure{false};
};

}  // namespace QtRocket::Gui
