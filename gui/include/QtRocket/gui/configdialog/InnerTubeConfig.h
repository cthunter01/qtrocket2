#pragma once

#include <memory>

#include <QObject>
#include <QString>

#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/RingComponentConfig.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"

class QPushButton;
class QRadioButton;
class QWidget;

namespace QtRocket
{
class InnerTube;
class OpenRocketDocument;
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ClusterSelectionPanel;
class ComponentSelection;
class DescriptionArea;
class DoubleBinding;
class GuiServices;
class MotorConfig;

/// The configuration of an inner tube (OpenRocket's InnerTubeConfig, swing
/// gui/configdialog/InnerTubeConfig.java), with the tabs General, Motor, Cluster and Radial
/// position before Override and Comment:
///
/// - General ("General properties"): "Length:", "Outer diameter:", "Inner diameter:" and
///   "Wall thickness:" (an inner tube has no automatic radius), then the placement and the bulk
///   material: the General tab of a tube coupler (RingComponentConfig::makeGeneralTab(); Java
///   writes the same fields out again).
/// - Motor ("Motor mount configuration"): MotorConfig.
/// - Cluster ("Cluster configuration"): "Select cluster configuration:" over the
///   ClusterSelectionPanel; "Tube separation:" ("The separation of the tubes, 1.0 = touching each
///   other") with the radio buttons "Relative" (the cluster scale: at least 0, no unit, slider 0,
///   1, 4) and "Absolute" (the gap between the tubes, getClusterScaleAbsolute(): a length, any
///   value, slider from -2 to 6 times the outer radius, following it), one field showing the one
///   chosen; "Rotation:" (the cluster rotation, -180 to 180 degrees, slider over that range);
///   "Split cluster" and "Reset settings" (scale 1, rotation 0).
/// - Radial position ("Radial position"): "Radial distance:" (at least 0, slider 0, 10 cm, 1 m),
///   "Radial direction:" (slider -180 to 180 degrees), "Reset" (both 0) and the note that an inner
///   tube does not change the aerodynamics, wherever it is.
///
/// The choice of "Relative" or "Absolute" is the preference InnerTubeSeparationRelative
/// (GuiPreferenceKeys::kInnerTubeSeparationRelative, default true), read when the panel is made
/// and stored when the user chooses. Deviation from OpenRocket (the user's decision U4): Java's
/// "Relative" stores false (InnerTubeConfig.java:353, a slip: both buttons store false), so after
/// the dialog's first opening the separation always comes up absolute; here "Relative" stores
/// true. Java also stores the choice when the dialog opens (selecting the remembered button
/// fires its listener); here only a choice of the user's stores it.
///
/// "Split cluster" (splitCluster()) makes each tube of the cluster of every selected inner tube
/// (the main one first, then the others, as Java) a component of its own: the cluster's tube at
/// each of the tube's instance locations (RocketComponent::getComponentLocations(): absolute
/// ones, as Java's, which counts the instances of a pod set the tube is in too), a copy of the
/// tube and its children named "<name> #<n>" (InnerTube::makeIndividualClusterComponent()), in
/// the tube's place among its siblings; a single tube stays as it is. It is an undo step of its
/// own, "Split cluster" (added before the split), and runs outside any freeze, as Java: each tube
/// removed and each copy added fires its TREE change at once (D2FixerProbe: four for a 3-ring),
/// and the first one closes the dialog. The tubes are found again by id for their turn, as no
/// component pointer outlives a TREE change: a selected tube inside another selected tube is split
/// where it is when its turn comes before the outer one (the copies of the outer one then hold its
/// copies); after the outer one it is in the removed original, no longer in the rocket, and is
/// passed over, which leaves the rocket as Java's split of it there does (the outer one's copies
/// keep it whole, and a split inside a detached tree tells nobody). Deviations from OpenRocket:
/// Java runs the split later (SwingUtilities.invokeLater()); here it runs at once (the dialog then
/// goes by deleteLater()). Java's other selected tubes, its config listeners while the dialog is
/// open, tell no change of their own, so the split of a tube inside one of them fires nothing
/// there; here it fires its TREE changes (the user's decision U3: no config listeners). Pending
/// edits are committed first (a button takes no focus on macOS).
///
/// The multi-edit (the user's decision U3, ComponentSelection): OpenRocket's InnerTube setters
/// forward to the other selected tubes before they set the main one, except
/// setClusterScaleAbsolute(), which has no loop of its own: it computes the RELATIVE scale from
/// the main tube's outer radius and sets it with setClusterScale(), which forwards that scale, so
/// in Java every selected tube gets the main tube's relative scale (a gap of 1 cm between 19 mm
/// tubes gives a 40 mm tube the gap 2.1 cm). Here each tube gets the gap typed (its own relative
/// scale from its own outer radius). The ring setters of the General tab are those of
/// RingComponentConfig.
class InnerTubeConfig : public RocketComponentConfig
{
    Q_OBJECT

public:
    /// The configuration of the inner tubes of @p selection.
    /// @throws BugError when @p selection is null or invalid, or its main component is not an
    ///         inner tube
    InnerTubeConfig(OpenRocketDocument& document, const GuiServices& services,
                    std::shared_ptr<ComponentSelection> selection, QWidget* parent);

    // ------------------------------------------------------------------------ General, Motor

    [[nodiscard]] const RingComponentConfig::GeneralTab& generalTab() const noexcept
    {
        return m_general;
    }
    [[nodiscard]] MotorConfig& motorConfig() const noexcept { return *m_motor; }

    // ---------------------------------------------------------------------------- Cluster

    [[nodiscard]] ClusterSelectionPanel& clusterSelection() const noexcept { return *m_cluster; }
    /// The separation field (its label, spin box, unit selector and slider), showing the relative
    /// or the absolute separation (isSeparationRelative()); its widgets are named after the
    /// property shown ("ClusterScale" or "ClusterScaleAbsolute").
    [[nodiscard]] const ConfigFields::NumberWidgets& separationWidgets() const noexcept
    {
        return m_separation;
    }
    [[nodiscard]] QRadioButton& relativeButton() const noexcept { return *m_relativeButton; }
    [[nodiscard]] QRadioButton& absoluteButton() const noexcept { return *m_absoluteButton; }
    /// Whether the separation field shows the relative separation.
    [[nodiscard]] bool isSeparationRelative() const noexcept { return m_separationRelative; }
    [[nodiscard]] const std::shared_ptr<DoubleBinding>& relativeSeparation() const noexcept
    {
        return m_relativeSeparation;
    }
    [[nodiscard]] const std::shared_ptr<DoubleBinding>& absoluteSeparation() const noexcept
    {
        return m_absoluteSeparation;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& rotationWidgets() const noexcept
    {
        return m_rotation;
    }
    [[nodiscard]] QPushButton& splitButton() const noexcept { return *m_splitButton; }
    [[nodiscard]] QPushButton& resetClusterButton() const noexcept { return *m_resetCluster; }

    // -------------------------------------------------------------------- Radial position

    [[nodiscard]] const ConfigFields::NumberWidgets& radialPositionWidgets() const noexcept
    {
        return m_radialPosition;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& radialDirectionWidgets() const noexcept
    {
        return m_radialDirection;
    }
    [[nodiscard]] QPushButton&     resetRadialButton() const noexcept { return *m_resetRadial; }
    [[nodiscard]] DescriptionArea& radialNote() const noexcept { return *m_radialNote; }

    // ---------------------------------------------------------------------------- actions

    /// "Split cluster" (see the class comment). Nothing once the selection is invalid.
    void splitCluster();
    /// "Reset settings": cluster scale 1 and rotation 0 for every selected inner tube. Nothing
    /// once the selection is invalid.
    void resetClusterSettings();
    /// "Reset" of the radial position: direction and distance 0 for every selected inner tube.
    /// Nothing once the selection is invalid.
    void resetRadialPosition();

    // ---------------------------------------------------------------------------- sources

    /// The cluster scale (Java: DoubleModel(component, "ClusterScale")).
    [[nodiscard]] static DoubleSource clusterScaleSource();
    /// The gap between the tubes of the cluster (Java: DoubleModel(component,
    /// "ClusterScaleAbsolute")).
    [[nodiscard]] static DoubleSource clusterScaleAbsoluteSource();
    /// The rotation of the cluster (Java: DoubleModel(component, "ClusterRotation")).
    [[nodiscard]] static DoubleSource clusterRotationSource();
    /// The radial distance of a ring component (Java: DoubleModel(component, "RadialPosition")).
    [[nodiscard]] static DoubleSource radialPositionSource();
    /// The radial direction of a ring component (Java: DoubleModel(component,
    /// "RadialDirection")).
    [[nodiscard]] static DoubleSource radialDirectionSource();

    /// Splits the cluster of @p tube (Java: splitAction() of the split button): when it has more
    /// than one instance, removes it from its parent and puts in its place a copy for each of its
    /// instance locations (see the class comment), returning the removed tube (keep it until the
    /// change has been told: under a freeze, until the thaw); null, with nothing changed, for a
    /// single tube.
    /// @throws BugError when @p tube has no parent, or is not among its parent's children
    [[nodiscard]] static std::unique_ptr<RocketComponent> splitClusterOf(InnerTube& tube);

private:
    void addClusterTab();
    void addRadialPositionTab();
    /// Shows the relative separation (@p relative) or the absolute one in the separation field.
    void showSeparation(bool relative);
    /// The user chose the relative separation (@p relative) or the absolute one: shows it and
    /// stores the choice.
    void chooseSeparation(bool relative);

    RingComponentConfig::GeneralTab m_general;
    MotorConfig*                    m_motor{nullptr};

    ClusterSelectionPanel*         m_cluster{nullptr};
    ConfigFields::NumberWidgets    m_separation;
    std::shared_ptr<DoubleBinding> m_relativeSeparation;
    std::shared_ptr<DoubleBinding> m_absoluteSeparation;
    /// The bounds of the absolute separation's slider: -2 and 6 times the outer radius.
    std::shared_ptr<DoubleBinding> m_absoluteMin;
    std::shared_ptr<DoubleBinding> m_absoluteMax;
    QString                        m_relativeToolTip;
    QString                        m_absoluteToolTip;
    bool                           m_separationRelative{true};
    QRadioButton*                  m_relativeButton{nullptr};
    QRadioButton*                  m_absoluteButton{nullptr};
    ConfigFields::NumberWidgets    m_rotation;
    QPushButton*                   m_splitButton{nullptr};
    QPushButton*                   m_resetCluster{nullptr};

    ConfigFields::NumberWidgets m_radialPosition;
    ConfigFields::NumberWidgets m_radialDirection;
    QPushButton*                m_resetRadial{nullptr};
    DescriptionArea*            m_radialNote{nullptr};
};

}  // namespace QtRocket::Gui
