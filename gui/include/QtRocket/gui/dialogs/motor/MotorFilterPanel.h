#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <QObject>
#include <QWidget>

#include "QtRocket/util/Signal.h"

class QCheckBox;
class QGroupBox;
class QLabel;
class QVBoxLayout;

namespace QtRocket
{
class Manufacturer;
class MotorMount;
class Preferences;
class ThrustCurveMotorSelection;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class CheckList;
class DoubleBinding;
class RangeSlider;
class UnitSelector;
class UnitSpinBox;

/// The "Filter Motors" tab of the motor chooser (OpenRocket's MotorFilterPanel, swing
/// gui/dialogs/motor/thrustcurve/MotorFilterPanel.java): the criteria of the chooser's
/// MotorRowFilter, in a scroll area, from the top:
/// - "Hide motors already used in the mount" (not stored; off in a new chooser): the motors of the
///   mount the chooser is opened for, which ThrustCurveMotorSelection::setMotorMountAndConfig()
///   gives the filter;
/// - "Manufacturer": a CheckList of the manufacturers of the database
///   (ThrustCurveMotorSelection::getManufacturers(), sorted by simple name, shown by display
///   name), unchecked = excluded, with "Clear All" and "Select All"; the excluded ones are stored
///   (MotorPreferences::setExcludedMotorManufacturers()) at every change, and the list starts
///   from the stored ones;
/// - "Total Impulse": a RangeSlider over the impulse classes A to O;
/// - "Motor Dimensions": the mount's dimensions ("Motor mount dimensions: 28 mm x 100 mm"),
///   "Nominal Diameter" with "Limit motor diameter to mount diameter" and a RangeSlider over the
///   stops of MotorFilterSliders, and "Length" with "Limit motor length to mount length", the
///   minimum and maximum lengths (spin boxes with unit selectors) and a RangeSlider from 0 to
///   1000 mm, its end meaning no limit. Each limit box is stored (MotorPreferences) when the
///   chooser opens for a mount and when it is clicked; while it is on, its slider (and the
///   length's spin boxes) are disabled and the filter's limit follows the mount; turning it off
///   keeps the limit (OpenRocket's sticky limits).
///
/// Every change of a criterion emits filterChanged() (Java's abstract onSelectionChanged(), which
/// the selection panel implements: it sorts the table again, MotorTableModel::refresh(), and
/// scrolls to the selected motor). The listeners are Java's, line by line, so that a change also
/// emits what Java's nested listeners do (a length typed in a spin box moves the slider, whose
/// listener sets both lengths, and so on): the sliders' listeners run for every change event of
/// the slider (RangeSlider::stateChanged()), the length's for every change of a spin box's value
/// (a DoubleBinding of its own value, as Java's DoubleModel on the filter's MinimumLength and
/// MaximumLength, from 0 up, in the motor dimension unit group). Each listener runs once per change
/// of what it listens to; Java's DoubleModel also runs the length fields' listeners for every
/// setValue() that changes nothing, and once more for the filter's own change event, so that
/// OpenRocket sorts its table more often (the probe tools/openrocket-probes/motor-filter-panel
/// counts the sorts): what the listeners leave is the same, which the probe's transcript pins.
/// The one effect of such a repeated setValue() that shows is the length slider's upper thumb when
/// "Limit motor length to mount length" applies the mount's length again (the chooser opened for
/// the same mount again): Java's listener puts it at the truncated length in mm (69 for 69.85 mm,
/// where setLimitLength() first rounds it to 70), and so does setLimitLength() here (pinned for
/// the example designs' mounts by tools/openrocket-gui-pins' motor-chooser family).
///
/// The selection panel (ThrustCurveMotorSelectionPanel) makes it with the chooser's
/// ThrustCurveMotorSelection, and calls setMotorMount() when the chooser is opened for a mount and
/// setHideUnavailable() for "Hide motors which are not in regular production". The selection and
/// the preferences must outlive the panel.
///
/// Deviations from OpenRocket:
/// - "Hide motors already used in the mount" hides the motors of the current mount only
///   (QtRocket fixes M3's bug 9 in MotorRowFilter::setUsedMotors(), which
///   ThrustCurveMotorSelection::setMotorMountAndConfig() calls; OpenRocket's filter keeps the
///   motors of every mount the chooser was opened for).
/// - A length typed as an infinite expression ("1/0") sets nothing (DoubleBinding; Java's spin box
///   takes infinity). The spin boxes' other rules are UnitSpinBox's.
/// - The length slider's listener takes the slider's own upper value when "Limit motor length to
///   mount length" is on before the chooser was opened for a mount (Java unboxes the mount length,
///   still null: a NullPointerException; no user action gets there, the slider being disabled).
/// - The titled boxes are QGroupBoxes whose titles are bold, the small texts one point smaller
///   (Java: changeFontSize(-1)), the gaps the style's.
class MotorFilterPanel final : public QWidget
{
    Q_OBJECT

public:
    /// MotorFilterPanel(allManufacturers, filter): the panel of @p selection's filter, its
    /// manufacturers getManufacturers(), with the excluded manufacturers and the limit boxes of
    /// @p preferences.
    MotorFilterPanel(ThrustCurveMotorSelection& selection, Preferences& preferences,
                     QWidget* parent);
    ~MotorFilterPanel() override;
    MotorFilterPanel(const MotorFilterPanel&)            = delete;
    MotorFilterPanel& operator=(const MotorFilterPanel&) = delete;
    MotorFilterPanel(MotorFilterPanel&&)                 = delete;
    MotorFilterPanel& operator=(MotorFilterPanel&&)      = delete;

    /// setMotorMount(): the chooser is opened for @p mount (null: none). Emits filterChanged()
    /// (the used motors are the filter's own, ThrustCurveMotorSelection::setMotorMountAndConfig()),
    /// shows the mount's dimensions (nothing without a mount), resets the length's units to the
    /// default one, stores both limit boxes, relabels the diameter slider in the default unit and
    /// applies the limit boxes that are on to the mount. Only the mount's diameter and length are
    /// kept.
    void setMotorMount(const MotorMount* mount);

    /// setHideUnavailable(): "Hide motors which are not in regular production" of the selection
    /// panel, set and stored (ThrustCurveMotorSelection::setHideUnavailable()); emits
    /// filterChanged().
    void setHideUnavailable(bool hideUnavailable);

Q_SIGNALS:
    /// A criterion of the filter changed (Java's onSelectionChanged()).
    void filterChanged();

private:
    /// The rows of the panel: each adds its widgets to @p content (or @p box), the boxes return
    /// themselves for the caller's layout.
    void                     makeHideUsedBox(QWidget& content, QVBoxLayout& contentLayout);
    [[nodiscard]] QGroupBox& makeManufacturerBox(QWidget& content);
    [[nodiscard]] QGroupBox& makeImpulseBox(QWidget& content);
    [[nodiscard]] QGroupBox& makeDimensionsBox(QWidget& content);
    void                     makeDiameterRows(QWidget& box, QVBoxLayout& boxLayout);
    void                     makeLengthRows(QWidget& box, QVBoxLayout& boxLayout);

    /// The listeners (Java's anonymous classes, in its order).
    void hideUsedClicked();
    void manufacturersChanged();
    void impulseChanged();
    void limitDiameterClicked();
    void diameterChanged();
    void limitLengthClicked();
    void minimumLengthChanged();
    void maximumLengthChanged();
    void lengthSliderChanged();

    /// setLimitLength() and setLimitDiameter().
    void setLimitLength();
    void setLimitDiameter();
    /// The enabled state of the slider (and the spin boxes) a limit box controls.
    void updateEnabled();

    /// The manufacturers of the list items @p indices.
    [[nodiscard]] std::vector<const Manufacturer*> manufacturersAt(
        const std::vector<int>& indices) const;

    ThrustCurveMotorSelection* m_selection;
    Preferences*               m_preferences;
    /// The list's manufacturers, in its order.
    std::vector<const Manufacturer*> m_manufacturers;

    QCheckBox*    m_hideUsedBox{nullptr};
    CheckList*    m_manufacturerCheckList{nullptr};
    RangeSlider*  m_impulseSlider{nullptr};
    QLabel*       m_motorMountDimension{nullptr};
    QCheckBox*    m_limitDiameterCheckBox{nullptr};
    RangeSlider*  m_diameterSlider{nullptr};
    QCheckBox*    m_limitByLengthCheckBox{nullptr};
    UnitSpinBox*  m_minLengthSpinner{nullptr};
    UnitSelector* m_minLengthUnitSelect{nullptr};
    UnitSpinBox*  m_maxLengthSpinner{nullptr};
    UnitSelector* m_maxLengthUnitSelect{nullptr};
    RangeSlider*  m_lengthSlider{nullptr};

    /// The length fields' models (Java's minLengthModel and maxLengthModel).
    std::shared_ptr<DoubleBinding> m_minLengthModel;
    std::shared_ptr<DoubleBinding> m_maxLengthModel;
    /// Destroyed before the models.
    Signal<>::ScopedConnection m_minLengthConnection;
    Signal<>::ScopedConnection m_maxLengthConnection;

    bool                  m_limitDiameter{false};
    bool                  m_limitByLength{false};
    std::optional<double> m_mountLength;
    std::optional<double> m_mountDiameter;
};

}  // namespace QtRocket::Gui
