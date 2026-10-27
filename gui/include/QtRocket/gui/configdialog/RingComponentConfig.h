#pragma once

#include <memory>

#include <QObject>
#include <QString>

#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"

class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class BoolCheckBox;
class ComponentSelection;
class GuiServices;
class MaterialPanel;
class PlacementPanel;

/// The configuration of a ring component (OpenRocket's RingComponentConfig, swing
/// gui/configdialog/RingComponentConfig.java): the base of ThicknessRingComponentConfig (tube
/// couplers and engine blocks), CenteringRingConfig and BulkheadConfig, which make their General
/// tab with addGeneralTab() (Java: generalTab(length, outer, inner, thickness), a null text
/// leaving its row out).
///
/// The General tab: on the left the rows whose texts are given, each a number field
/// (ConfigFields):
/// - the length ("Length", at least 0, slider 0, 10 cm, 1 m);
/// - the outer DIAMETER (the outer radius times 2, at least 0, slider 0, 4 cm, 20 cm) and below it
///   "Automatic" ("Matches outer diameter to the inner diameter of the parent component.") when
///   the component's class can take the outer radius from its parent (outerRadiusSource());
/// - the inner diameter (at least 0; its slider from 0 to the outer diameter, following it, or
///   0, 4 cm, 20 cm without an outer row) and "Automatic" ("Matches inner diameter to the outer
///   diameter of the child inner tube.") when the class can (innerRadiusSource());
/// - the wall thickness (at least 0, slider 0 to 1 cm);
/// on the right the PlacementPanel and the MaterialPanel (bulk materials). Every field edits every
/// selected component (ComponentSelection::apply()).
///
/// Which "Automatic" boxes a component gets is decided by its class, as Java's DoubleModel does
/// by reflection (an automatic state needs a public set...Automatic()): the outer radius of a tube
/// coupler, an engine block, a centering ring and a bulkhead, the inner radius of a centering ring
/// only; an inner tube has neither.
///
/// The multi-edit (the user's decision U3, ComponentSelection): OpenRocket's setters of these
/// fields all forward to the other selected components before they set the main one. Two of them
/// derive a value from the main component that the forwarding then gives the others (both listed
/// in ComponentSelection.h): ThicknessRingComponent.setInnerRadius() (the thickness from the main's
/// outer radius) and RadiusRingComponent.setThickness() (the inner radius from the main's outer
/// radius). Here each component derives it from its own values.
///
/// The length field takes the focus once a preset is chosen (RocketComponentConfig's presets;
/// Java: focusElement) for a ThicknessRingComponent only (a tube coupler and an engine block:
/// Java's "component instanceof ThicknessRingComponent"), not for a centering ring or a bulkhead.
class RingComponentConfig : public RocketComponentConfig
{
    Q_OBJECT

public:
    /// The texts of the rows of the General tab; an empty text leaves its row out (Java: null).
    struct GeneralTabTexts
    {
        QString length;
        QString outer;
        QString inner;
        QString thickness;
    };

    /// The widgets of the General tab; the widgets of a row it has not are null.
    struct GeneralTab
    {
        QWidget*                    page = nullptr;
        ConfigFields::NumberWidgets length;
        ConfigFields::NumberWidgets outerRadius;
        BoolCheckBox*               outerRadiusAutomatic = nullptr;
        ConfigFields::NumberWidgets innerRadius;
        BoolCheckBox*               innerRadiusAutomatic = nullptr;
        ConfigFields::NumberWidgets thickness;
        PlacementPanel*             placement = nullptr;
        MaterialPanel*              material  = nullptr;
    };

    /// The configuration of the ring components of @p selection, without tabs of its own yet
    /// (the subclasses add the General tab).
    /// @throws BugError when @p selection is null or invalid
    RingComponentConfig(OpenRocketDocument& document, const GuiServices& services,
                        std::shared_ptr<ComponentSelection> selection, QWidget* parent);

    /// The widgets of the General tab (all null before addGeneralTab()).
    [[nodiscard]] const GeneralTab& generalTab() const noexcept { return m_general; }

    /// Makes the General tab of @p panel with the rows of @p texts (see the class comment), as a
    /// child of @p panel; the caller inserts the page. Also InnerTubeConfig's General tab, which
    /// Java writes out again with the same fields (the inner tube has no automatic radius).
    /// @throws BugError when @p panel's selection is invalid or its main component is not a
    ///         RingComponent
    [[nodiscard]] static GeneralTab makeGeneralTab(RocketComponentConfig& panel,
                                                   const GeneralTabTexts& texts);

    /// The length of a ring component (Java: DoubleModel(component, "Length")).
    [[nodiscard]] static DoubleSource lengthSource();
    /// The outer radius of a ring component, with its automatic state when the class of @p main
    /// has a public setOuterRadiusAutomatic() (a tube coupler, an engine block, a centering ring, a
    /// bulkhead; see the class comment).
    [[nodiscard]] static DoubleSource outerRadiusSource(const RocketComponent& main);
    /// The inner radius of a ring component, with its automatic state when @p main is a centering
    /// ring.
    [[nodiscard]] static DoubleSource innerRadiusSource(const RocketComponent& main);
    /// The wall thickness of a ring component (Java: DoubleModel(component, "Thickness"): the
    /// one-argument setThickness()).
    [[nodiscard]] static DoubleSource thicknessSource();

    /// Whether the class of @p component has a public setOuterRadiusAutomatic() (Java's
    /// DoubleModel(component, "OuterRadius").isAutomaticAvailable()).
    [[nodiscard]] static bool hasAutomaticOuterRadius(const RocketComponent& component) noexcept;
    /// Whether the class of @p component has a public setInnerRadiusAutomatic() (a centering
    /// ring).
    [[nodiscard]] static bool hasAutomaticInnerRadius(const RocketComponent& component) noexcept;

protected:
    /// Makes the General tab with the rows of @p texts (makeGeneralTab()), inserts it as the first
    /// tab titled @p title with the tooltip @p toolTip and selects it (Java: generalTab(),
    /// tabbedPane.insertTab(..., 0) and setSelectedIndex(0)).
    void addGeneralTab(const GeneralTabTexts& texts, const QString& title, const QString& toolTip);

private:
    GeneralTab m_general;
};

}  // namespace QtRocket::Gui
