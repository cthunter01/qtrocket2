#pragma once

#include <memory>

#include <QObject>

#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/position/RadiusMethod.h"

class QPushButton;
class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ComponentSelection;
class EnumComboBox;
class GuiServices;
class IntSpinBox;
class PlacementPanel;

/// The configuration of an assembly (OpenRocket's ComponentAssemblyConfig, swing
/// gui/configdialog/ComponentAssemblyConfig.java): the panel of a pod set (PodSet), and the base
/// of AxialStageConfig, the panel of a stage and of a booster set (ParallelStage). A pod set and
/// a booster set get, as Java's assemblies "off the centerline":
///
/// - the tab General ("Options for locating stages parallel to other stages", Java's
///   RocketCompCfg.tab.Assembly), first and selected: "Radius relative to:" (the radius method,
///   FREE "Center of the parent component" or RELATIVE "Surface of the parent component",
///   kRadiusMethodChoices), "Radial Distance:" (the radius offset, at least 0; its slider from 0
///   to four times the outer radius of the parent, which it follows), "Angle:" (the angle offset,
///   -pi to pi, slider the same), "Number of Copies:" (the instance count, at least 1) and the
///   PlacementPanel;
/// - the button "Split Pods" or "Split Boosters" before Cancel, enabled while the main component
///   has more than one instance (updateFields()).
///
/// A plain stage (AxialStage) gets neither.
///
/// Every field reaches every selected assembly (the user's decision U3, ComponentSelection). The
/// setters, audited in the Java (PodSet and ParallelStage): setInstanceCount(), setAngleOffset(),
/// setRadiusOffset() and setRadiusMethod() forward to the others first, so applying each to every
/// component gives Java's result, except for the radius method, whose radius Java computes from
/// the MAIN component's offset (item 12 of ComponentSelection.h's list); the placement is
/// PlacementPanel's.
///
/// The split (splitInstances(), the button's action; ComponentAssemblyConfig.java:160-186): the
/// pending edits are committed, the dialog closes (ComponentConfigDialog::disposeDialog(), when it
/// shows this panel: Java's disposeDialog()), and inside the undo operation "Split assembly"
/// (OpenRocketDocument::startUndo() and stopUndo(); Java's literal text) the main component and
/// then each other selected one are split into single-instance copies
/// (RocketComponent::splitInstances(): names " #1", " #2", ..., the angle advanced by 2 pi / count
/// each, the override mass divided by the count). The pod sets are split inside one freeze of the
/// rocket (taken only when it is not frozen already), so the tree changes once at the thaw; the
/// booster sets without one, as Java (splitInstances(false)): every child removed or added fires
/// its event. The removed originals live until the last split is told.
///
/// Deviations from OpenRocket:
/// - The split runs at once (Java: SwingUtilities.invokeLater()).
/// - Every selected assembly is split. Java's splitInstances() splits its config listeners in a
///   loop that removes each from the list it iterates (RocketComponent.java:2585-2590): with two
///   other selected pod sets the second is left as it was, with three or more a
///   ConcurrentModificationException leaves the split half done and the undo operation open
///   (D2cProbe).
/// - A selected assembly inside another selected one that was split before it is not split again:
///   it was copied into each of the parts (with its own instance count), and the original is no
///   longer in the rocket. Java calls splitInstances() on that original, which throws an
///   IllegalStateException (no rocket) after the outer split (D2cProbe).
/// - stopUndo() ends the operation also when a split throws (Java leaves it open, and every later
///   undo call reports an undo error).
class ComponentAssemblyConfig : public RocketComponentConfig
{
    Q_OBJECT

public:
    /// The General tab's fields (see the class comment); all null for a plain stage.
    struct AssemblyWidgets
    {
        QWidget*                    page         = nullptr;
        EnumComboBox*               radiusMethod = nullptr;
        ConfigFields::NumberWidgets radius;
        ConfigFields::NumberWidgets angle;
        IntSpinBox*                 count     = nullptr;
        PlacementPanel*             placement = nullptr;
    };

    /// The configuration of the assemblies of @p selection.
    /// @throws BugError when @p selection is null or invalid, or its main component is no pod set
    ///         or stage
    ComponentAssemblyConfig(OpenRocketDocument& document, const GuiServices& services,
                            std::shared_ptr<ComponentSelection> selection, QWidget* parent);

    /// Whether the panel of @p kind has the General tab and the split button: a pod set's and a
    /// booster set's (Java: ParallelStage and PodSet).
    [[nodiscard]] static bool isOffCenterline(ComponentKind kind) noexcept;

    [[nodiscard]] const AssemblyWidgets& assemblyWidgets() const noexcept { return m_assembly; }
    /// "Split Pods" or "Split Boosters"; null for a plain stage.
    [[nodiscard]] QPushButton* splitButton() const noexcept { return m_split; }

    /// The split button's action (see the class comment). Nothing when the selection has ended.
    void splitInstances();

    /// The split button is enabled while the main component has more than one instance (Java's
    /// updateFields()).
    void updateFields() override;

    /// The radius method of an assembly (Java: EnumModel(boosters, "RadiusMethod",
    /// RadiusMethod.choices())); RadiusPositionable's.
    [[nodiscard]] static EnumSource<RadiusMethod> radiusMethodSource();
    /// The radius offset of an assembly (Java: DoubleModel(boosters, "RadiusOffset")).
    [[nodiscard]] static DoubleSource radiusOffsetSource();
    /// The angle offset of an assembly (Java: DoubleModel(boosters, "AngleOffset")).
    [[nodiscard]] static DoubleSource angleOffsetSource();
    /// The outer radius of the assembly's parent, 0 when the parent has none (Java:
    /// DoubleModel(component.getParent(), "OuterRadius", 4.0), the radius slider's maximum; only a
    /// body tube holds a pod set or a booster set). Read-only.
    [[nodiscard]] static DoubleSource parentOuterRadiusSource();

private:
    void addAssemblyTab();
    void addSplitButton();

    AssemblyWidgets m_assembly;
    QPushButton*    m_split{nullptr};
};

}  // namespace QtRocket::Gui
