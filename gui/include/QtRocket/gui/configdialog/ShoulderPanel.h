#pragma once

#include <QObject>
#include <QWidget>

class QGroupBox;

namespace QtRocket::Gui
{

class BindingScope;

/// The shoulder tab of a transition or a nose cone (OpenRocket's
/// RocketComponentConfig.shoulderTab(), swing gui/configdialog/RocketComponentConfig.java:835-1012,
/// which TransitionConfig and NoseConeConfig show as their "Shoulder" tab): for each shoulder its
/// diameter (shown as a diameter, the property being the radius; slider from 0 to the diameter of
/// that end), its length (slider 0, 2 cm, 20 cm, quadratic), its thickness (slider from 0 to the
/// shoulder's radius) and "End capped".
///
/// A transition has the fore shoulder (border "Fore shoulder") and the aft shoulder (border "Aft
/// shoulder"); a nose cone only its shoulder, without a border, whose properties are NoseCone's
/// own (ShoulderRadius, ShoulderLength, ShoulderThickness, ShoulderCapped and the base radius),
/// which cope with a flipped nose cone. The sliders' maxima follow the radii they end at (Java:
/// a DoubleModel as getSliderModel()'s maximum).
///
/// The main component of the scope's selection must be a Transition (a NoseCone is one).
class ShoulderPanel : public QWidget
{
    Q_OBJECT

public:
    /// The shoulders of the components of @p scope's selection.
    /// @throws BugError when the scope has no valid selection or its main component is not a
    ///         Transition
    ShoulderPanel(BindingScope& scope, QWidget* parent);

    /// The fore shoulder's border; null for a nose cone.
    [[nodiscard]] QGroupBox* foreShoulder() const noexcept { return m_fore; }
    /// The aft shoulder: its border, a QGroupBox, for a transition; a nose cone's shoulder, which
    /// has no border, for a nose cone.
    [[nodiscard]] QWidget& aftShoulder() const noexcept { return *m_aft; }

private:
    QGroupBox* m_fore{nullptr};
    QWidget*   m_aft{nullptr};
};

}  // namespace QtRocket::Gui
