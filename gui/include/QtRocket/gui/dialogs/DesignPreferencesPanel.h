#pragma once

#include <memory>

#include <QObject>
#include <QWidget>

class QCheckBox;

namespace QtRocket
{
class Preferences;
}

namespace QtRocket::Gui
{

class BindingScope;
class DoubleBinding;
class PrefChoiceSelector;
class UnitSpinBox;

/// The Design page of the preferences dialog (OpenRocket's DesignPreferencesPanel), each item
/// written to the preferences at once:
/// - "Position to insert new body components:" and "Position to insert new stages:" (Always ask,
///   Insert in middle, Add to end: BodyComponentInsertPosition, StageInsertPosition) and "Size of
///   text in rocket design panel: " (Small, Medium, Large: RocketInfoFontSize), each a
///   PrefChoiceSelector;
/// - "Default Mach Number for C.P. Estimate:", a spin box of Preferences::getDefaultMach() from
///   0.1 to 0.9 (Java: DoubleModel(preferences, "DefaultMach", 1.0, UNITS_COEFFICIENT, 0.1, 0.9));
/// - the check boxes "Always open leftmost tab when opening a component edit dialog",
///   "Show confirmation dialog for discarding component changes", "Show confirmation dialog for
///   discarding simulation changes" (IgnoreDiscardSimulationEditingWarning: whether the simulation
///   settings dialog's Cancel asks before it discards, Milestone 4), "Update estimated flight
///   parameters in design window" (GuiPreferenceKeys::kBackgroundFlight: whether the design view
///   computes the flight of its configuration in the background and shows the flight block,
///   Milestone 4; stored by the user's clicks, as Java's ActionListener; the design views read it
///   when the dialog refreshes them, on Apply and OK) and "Only show pod set/booster markers when
///   the pod set/booster is selected"; a change of the last one is told by showMarkersChanged(),
///   on which the dialog brings the figure of every window up to date (Java: updateFigures() of
///   every BasicFrame's rocket panel).
///
/// The spin box's binding and its listener are kept by the dialog's BindingScope: the dialog
/// commits a pending edit before Apply and OK (BindingScope::commitAll()) and releases the scope
/// when it closes, after which a late commit (the focus leaving the spin box of a hidden dialog)
/// stores nothing.
///
/// Deviation: the spin box edits a value of its own (an own-value DoubleBinding) that is written
/// to the preferences on every change, where Java's DoubleModel reads and writes the preference
/// through its getter and setter; the preference changes nowhere else while the page is open.
/// The spin box shows a stored value outside the bounds as it is (0.05; as Java's getter), and
/// sets nothing for text whose value is infinite ("1/0"; Java clamps it to 0.9: the user's
/// decision U4, DoubleBinding).
class DesignPreferencesPanel final : public QWidget
{
    Q_OBJECT

public:
    /// The bounds of the default Mach number.
    static constexpr double kMinimumMach = 0.1;
    static constexpr double kMaximumMach = 0.9;

    /// The page over @p preferences, which must outlive it, its binding kept by @p scope (see the
    /// class comment).
    DesignPreferencesPanel(Preferences& preferences, BindingScope& scope, QWidget* parent);
    ~DesignPreferencesPanel() override;
    DesignPreferencesPanel(const DesignPreferencesPanel&)            = delete;
    DesignPreferencesPanel& operator=(const DesignPreferencesPanel&) = delete;
    DesignPreferencesPanel(DesignPreferencesPanel&&)                 = delete;
    DesignPreferencesPanel& operator=(DesignPreferencesPanel&&)      = delete;

    [[nodiscard]] PrefChoiceSelector* bodyComponentInsertPosition() const noexcept
    {
        return m_bodyPosition;
    }
    [[nodiscard]] PrefChoiceSelector* stageInsertPosition() const noexcept
    {
        return m_stagePosition;
    }
    [[nodiscard]] PrefChoiceSelector* rocketInfoFontSize() const noexcept { return m_fontSize; }
    /// The default Mach number's spin box (object name "DefaultMach").
    [[nodiscard]] UnitSpinBox* defaultMachSpinner() const noexcept { return m_machSpinner; }
    [[nodiscard]] const std::shared_ptr<DoubleBinding>& defaultMachBinding() const noexcept
    {
        return m_mach;
    }
    [[nodiscard]] QCheckBox* alwaysOpenLeftmostTab() const noexcept { return m_leftmostTab; }
    [[nodiscard]] QCheckBox* showDiscardConfirmation() const noexcept { return m_discard; }
    /// "Show confirmation dialog for discarding simulation changes".
    [[nodiscard]] QCheckBox* showDiscardSimulationConfirmation() const noexcept
    {
        return m_discardSimulation;
    }
    /// "Update estimated flight parameters in design window".
    [[nodiscard]] QCheckBox* updateEstimates() const noexcept { return m_updateEstimates; }
    [[nodiscard]] QCheckBox* showMarkers() const noexcept { return m_markers; }

Q_SIGNALS:
    /// The user changed "Only show pod set/booster markers ..." (the preference is stored).
    void showMarkersChanged();

private:
    /// The spin box's value changed: stored as the default Mach number.
    void machChanged();

    Preferences*                   m_preferences;
    PrefChoiceSelector*            m_bodyPosition{nullptr};
    PrefChoiceSelector*            m_stagePosition{nullptr};
    PrefChoiceSelector*            m_fontSize{nullptr};
    std::shared_ptr<DoubleBinding> m_mach;
    UnitSpinBox*                   m_machSpinner{nullptr};
    QCheckBox*                     m_leftmostTab{nullptr};
    QCheckBox*                     m_discard{nullptr};
    QCheckBox*                     m_discardSimulation{nullptr};
    QCheckBox*                     m_updateEstimates{nullptr};
    QCheckBox*                     m_markers{nullptr};
};

}  // namespace QtRocket::Gui
