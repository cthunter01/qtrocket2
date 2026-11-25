#pragma once

#include <memory>

#include <QObject>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/configdialog/ConfigFields.h"

class QGridLayout;
class QLabel;

namespace QtRocket
{
class Preferences;
class SimulationOptions;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class BindingScope;
class CollapsiblePanel;
class ObjectSelection;

/// The editor of the speed thresholds that make the recovery deployment warnings (OpenRocket's
/// RecoveryWarningsPanel, swing gui/simulation/): on the settings dialog's Warnings tab over a
/// simulation's SimulationOptions (SimulationWarningsPanel), and on the Warnings preference page
/// over the preferences (WarningsPreferencesPanel). Java's one panel takes any object with the
/// three properties and finds their methods by reflection; here a constructor for each of the two
/// classes makes the bindings' sources (ObjectSelection: a source reads only the class its
/// selection was made for, K3's deviation).
///
/// What it shows, as Java's MigLayout("fillx, insets 0", "[grow]") column:
/// - the section "Single Deployment Warnings" (CollapsiblePanel, expanded): the description "Warn
///   when a recovery device deploys at an unsafe speed. Warnings apply independently by stage."
///   in italics, and "High Speed Threshold" (RecoverySpeedWarning);
/// - the section "Dual Deployment Warnings" (collapsed): the same description, the hint "Mark a
///   recovery device as a drogue in the Recovery tab of a Stage configuration dialog to enable
///   dual-deployment warnings.", and on one row "Low Speed Threshold"
///   (RecoveryDrogueMainLowSpeedWarning) and "High Speed Threshold"
///   (RecoveryDrogueMainHighSpeedWarning).
/// Each threshold is a label, a spin box and a unit selector of the velocity units (no slider),
/// the label and the spin box with the threshold's tooltip, the spin box and the unit selector
/// named after the property (objectName). The descriptions are word-wrapped labels of Java's HTML
/// (wrappingInfoHtml()): they take the width they are given and are as tall as their lines.
///
/// The bounds: Java's minimum 0, and, where Java has none, the maximum of the simulation's input
/// ranges (SimulationInputRanges::kRecoverySpeedThreshold, 10000 m/s: the user's decision
/// U-M4-7).
///
/// Lifetime: the bindings and the selection of the target (ObjectSelection::make()) are kept by
/// @p scope, which the dialog or page commits and releases when it closes; the target must stay
/// at its address while the panel shows it (a simulation's options, the application's
/// preferences); should it go first, the selection learns of it from its change signal and the
/// fields write nothing from then on.
///
/// Deviations from OpenRocket:
/// - The maximum where Java has none (see above).
/// - A collapsed section's content is hidden, not removed from the panel (CollapsiblePanel).
/// - The descriptions are word-wrapped QLabels, whose height Qt's layouts take from the width
///   they get (heightForWidth()); Java's labels measure their HTML view at the width of their
///   last layout and revalidate their ancestors in a later turn of the event loop, and say they
///   would like to be 420 pixels wide (kPreferredInfoWidth), which only a packed window sees.
class RecoveryWarningsPanel final : public QWidget
{
    Q_OBJECT

public:
    /// The widgets of a threshold: its label, spin box and unit selector (no slider).
    using Threshold = ConfigFields::NumberWidgets;

    /// Java's PREFERRED_INFO_WIDTH: the width the descriptions ask for when packed.
    static constexpr int kPreferredInfoWidth = 420;

    /// The thresholds of @p options (the settings dialog), their bindings kept by @p scope.
    RecoveryWarningsPanel(SimulationOptions& options, BindingScope& scope, QWidget* parent);
    /// The thresholds of @p preferences (the Warnings preference page), their bindings kept by
    /// @p scope.
    RecoveryWarningsPanel(Preferences& preferences, BindingScope& scope, QWidget* parent);
    ~RecoveryWarningsPanel() override;
    RecoveryWarningsPanel(const RecoveryWarningsPanel&)            = delete;
    RecoveryWarningsPanel& operator=(const RecoveryWarningsPanel&) = delete;
    RecoveryWarningsPanel(RecoveryWarningsPanel&&)                 = delete;
    RecoveryWarningsPanel& operator=(RecoveryWarningsPanel&&)      = delete;

    /// "Single Deployment Warnings" (singleDeploymentPanel), expanded at first.
    [[nodiscard]] CollapsiblePanel& singleDeploymentSection() const noexcept { return *m_single; }
    /// "Dual Deployment Warnings" (dualDeploymentPanel), collapsed at first.
    [[nodiscard]] CollapsiblePanel& dualDeploymentSection() const noexcept { return *m_dual; }
    /// The descriptions of the two sections (singleDescription, dualDescription) and the dual
    /// deployment's hint (dualDeploymentHowTo).
    [[nodiscard]] QLabel& singleDescription() const noexcept { return *m_singleDescription; }
    [[nodiscard]] QLabel& dualDescription() const noexcept { return *m_dualDescription; }
    [[nodiscard]] QLabel& dualHowTo() const noexcept { return *m_dualHowTo; }
    /// "High Speed Threshold" of the single deployment (RecoverySpeedWarning).
    [[nodiscard]] const Threshold& highSpeedThreshold() const noexcept { return m_highSpeed; }
    /// "Low Speed Threshold" and "High Speed Threshold" of the dual deployment
    /// (RecoveryDrogueMainLowSpeedWarning, RecoveryDrogueMainHighSpeedWarning).
    [[nodiscard]] const Threshold& dualLowSpeedThreshold() const noexcept { return m_dualLow; }
    [[nodiscard]] const Threshold& dualHighSpeedThreshold() const noexcept { return m_dualHigh; }

    /// The rich text of a description (Java: createWrappingInfoText()): the translated text
    /// @p text trimmed, the content between its "<html>" tag and "</html>" when it is HTML (its
    /// markup kept), otherwise escaped as HTML, in "<html><body style='margin:0;...'>" with
    /// "font-style:italic;" when @p italic.
    [[nodiscard]] static QString wrappingInfoHtml(const QString& text, bool italic);

private:
    struct Sources;

    RecoveryWarningsPanel(const std::shared_ptr<ObjectSelection>& target, const Sources& sources,
                          BindingScope& scope, QWidget* parent);

    /// A section's content, a widget of the panel with a grid of Java's columns
    /// "[][pref!][pref!]unrel[][pref!][pref!][grow]": the grid.
    [[nodiscard]] QGridLayout* makeSectionContent();

    CollapsiblePanel* m_single{nullptr};
    CollapsiblePanel* m_dual{nullptr};
    QLabel*           m_singleDescription{nullptr};
    QLabel*           m_dualDescription{nullptr};
    QLabel*           m_dualHowTo{nullptr};
    Threshold         m_highSpeed;
    Threshold         m_dualLow;
    Threshold         m_dualHigh;
};

}  // namespace QtRocket::Gui
