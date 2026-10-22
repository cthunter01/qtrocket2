#include "QtRocket/gui/figure/ViewRotationControl.h"

#include <cmath>
#include <memory>
#include <numbers>

#include <QAbstractSpinBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QObject>
#include <QSize>
#include <QSlider>
#include <QString>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/figure/RocketFigure.h"
#include "QtRocket/gui/figure/ScaleFigure.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket::Gui
{

namespace
{

/// The rotation's bounds (DoubleModel(figure, "Rotation", UNITS_ANGLE, 0, 2 * Math.PI)).
constexpr double kMaximumRotation = 2 * std::numbers::pi;

/// The property the fields edit (their object name).
QString propertyName()
{
    return QStringLiteral("Rotation");
}

/// The distance of @p angleDegrees to the snap point @p snapped and to its neighbours a turn
/// away (snapRotation()'s Math.min of the three).
[[nodiscard]] double snapDistance(double angleDegrees, double snapped) noexcept
{
    return MathUtil::javaMin(std::abs(angleDegrees - snapped),
                             MathUtil::javaMin(std::abs(angleDegrees - (snapped - 360)),
                                               std::abs(angleDegrees - (snapped + 360))));
}

/// The multiple of @p step degrees nearest to @p angleDegrees, in [0, 360) (Math.round(), then
/// ((x % 360) + 360) % 360).
[[nodiscard]] double nearestMultiple(double angleDegrees, double step) noexcept
{
    const double nearest = static_cast<double>(MathUtil::javaRound(angleDegrees / step)) * step;
    return std::fmod(std::fmod(nearest, 360.0) + 360.0, 360.0);
}

}  // namespace

ViewRotationControl::ViewRotationControl(RocketFigure& figure, Preferences& preferences,
                                         QWidget* parent)
  : QWidget(parent),
    m_figure(&figure),
    m_preferences(&preferences),
    // The bounds are the figure's to keep (bindingChanged()): the fields show its rotation as
    // it is.
    m_binding(std::make_shared<DoubleBinding>(figure.rotation(), unitGroup(UnitGroupId::ANGLE))),
    // The spin box and the unit selector in a panel of their own (Java's controlsPanel), the
    // vertical slider and the lock.
    m_controls(makeChild<QWidget>(this)),
    m_spinner(makeChild<UnitSpinBox>(m_controls, m_binding)),
    m_unitSelector(makeChild<UnitSelector>(m_controls, m_binding)),
    m_slider(makeChild<ValueSlider>(this, m_binding, SliderMapping::linear(0.0, kMaximumRotation))),
    m_lockButton(makeChild<QToolButton>(this)),
    m_locked(preferences.getBoolean(GuiPreferenceKeys::kLockClickDragRotation,
                                    GuiPreferenceKeys::kLockClickDragRotationDefault))
{
    setObjectName(QStringLiteral("rotationControl"));
    //: RocketPanel.ttip.Rotation
    const QString tip =
        tr("<html>Adjust the view's roll rotation.<br>You can also drag empty space "
           "in the rocket view (view lock must be disabled).<br>Hold Shift for "
           "30\u00B0 or 45\u00B0 increments.</html>");

    // The spin box without its buttons, and the unit selector beside it (MigLayout "fill, insets
    // 0, gap 0": the spin box "width 50!", the selector "growx").
    m_controls->setObjectName(QStringLiteral("controlsPanel"));
    m_spinner->setObjectName(propertyName());
    m_spinner->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_spinner->setFixedWidth(kSpinnerWidth);
    m_spinner->setToolTip(tip);
    m_unitSelector->setObjectName(propertyName());
    m_unitSelector->setToolTip(tip);

    // The vertical slider, inverted: 0 at the top.
    m_slider->setObjectName(propertyName());
    m_slider->setOrientation(Qt::Vertical);
    m_slider->setInvertedAppearance(true);
    m_slider->setToolTip(tip);
    m_slider->installEventFilter(this);

    // The lock.
    m_lockButton->setObjectName(QStringLiteral("lockButton"));
    m_lockButton->setCheckable(true);
    m_lockButton->setChecked(m_locked);
    m_lockButton->setFixedSize(kLockButtonSize, kLockButtonSize);
    //: RocketPanel.ttip.lockDragRotation
    m_lockButton->setToolTip(tr("Lock/unlock click-drag rotation"));
    updateLockIcon();
    connect(m_lockButton, &QToolButton::toggled, this,
            [this](bool locked) { lockToggled(locked); });
    connect(m_lockButton, &QToolButton::clicked, this,
            guarded([this](bool locked) { lockClicked(locked); }, QStringLiteral("Rotation lock")));

    auto* fields = makeChild<QHBoxLayout>(m_controls);
    fields->setContentsMargins(0, 0, 0, 0);
    fields->setSpacing(0);
    fields->addWidget(m_spinner);
    fields->addWidget(m_unitSelector, 1);

    // MigLayout "fill, insets 0, gap 0": the fields ("growx"), the slider ("ax 50%, growy,
    // pushy"), the lock ("ax 50%").
    auto* layout = makeChild<QVBoxLayout>(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_controls);
    layout->addWidget(m_slider, 1, Qt::AlignHCenter);
    layout->addWidget(m_lockButton, 0, Qt::AlignHCenter);

    m_bindingConnection = m_binding->addChangeListener(
        [this] { Guard::run([this] { bindingChanged(); }, QStringLiteral("Rotation")); });
    connect(&figure, &ScaleFigure::changed, this,
            guarded([this] { figureChanged(); }, QStringLiteral("Rotation")));
}

ViewRotationControl::~ViewRotationControl() = default;

double ViewRotationControl::snapRotation(double angle) noexcept
{
    const double angleDegrees = MathUtil::javaToDegrees(angle);
    const double nearest30    = nearestMultiple(angleDegrees, 30.0);
    const double nearest45    = nearestMultiple(angleDegrees, 45.0);
    // Whichever is nearer; 30 on a tie (and 45 for a NaN angle, whose distances are NaN).
    if (snapDistance(angleDegrees, nearest30) <= snapDistance(angleDegrees, nearest45))
    {
        return MathUtil::javaToRadians(nearest30);
    }
    return MathUtil::javaToRadians(nearest45);
}

void ViewRotationControl::setDragRotationLocked(bool locked)
{
    m_lockButton->setChecked(locked);
}

bool ViewRotationControl::eventFilter(QObject* watched, QEvent* event)
{
    const auto* const mouse = dynamic_cast<const QMouseEvent*>(event);
    if (watched == m_slider && mouse != nullptr)
    {
        switch (mouse->type())
        {
            case QEvent::MouseButtonPress:
            case QEvent::MouseMove:
                // Java: mousePressed() and mouseDragged() read isShiftDown().
                m_shiftPressed = mouse->modifiers().testFlag(Qt::ShiftModifier);
                break;
            case QEvent::MouseButtonRelease:
                m_shiftPressed = false;
                break;
            default:
                break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ViewRotationControl::figureChanged()
{
    const RocketFigure* figure = m_figure.data();
    if (figure == nullptr)
    {
        return;
    }
    m_followingFigure = true;
    m_binding->setValue(figure->rotation());
    m_followingFigure = false;
}

void ViewRotationControl::bindingChanged()
{
    RocketFigure* figure = m_figure.data();
    if (m_followingFigure || figure == nullptr)
    {
        return;
    }
    // Java's DoubleModel.setValue(): the value is clamped to the bounds before it is set.
    figure->setRotation(MathUtil::clamp(m_binding->value(), 0.0, kMaximumRotation));
    // The slider's ChangeListener: while Shift drags the thumb, the value snaps.
    if (m_slider->isSliderDown() && m_shiftPressed)
    {
        const double current = m_binding->value();
        const double snapped = snapRotation(current);
        if (std::abs(current - snapped) > kSnapTolerance)
        {
            m_binding->setValue(snapped);
        }
    }
}

void ViewRotationControl::lockToggled(bool locked)
{
    m_locked = locked;
    updateLockIcon();
    Q_EMIT dragRotationLockChanged(locked);
}

void ViewRotationControl::lockClicked(bool locked)
{
    m_preferences->putBoolean(GuiPreferenceKeys::kLockClickDragRotation, locked);
}

void ViewRotationControl::updateLockIcon()
{
    m_lockButton->setIcon(Icons::icon(m_locked ? Icons::Id::LOCKED : Icons::Id::UNLOCKED));
}

}  // namespace QtRocket::Gui
