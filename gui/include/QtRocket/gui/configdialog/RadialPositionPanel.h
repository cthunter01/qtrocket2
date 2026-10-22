#pragma once

#include <memory>

#include <QObject>
#include <QWidget>

#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"

class QGridLayout;
class QPushButton;

namespace QtRocket::Gui
{

class BindingScope;
class ComponentSelection;
class RocketComponentConfig;

/// The tab "Radial position" of a mass object's configuration: MassComponentConfig,
/// ShockCordConfig, ParachuteConfig and StreamerConfig (OpenRocket writes it out in each of the
/// four panels, as positionTab() of swing gui/configdialog/MassComponentConfig.java,
/// ShockCordConfig.java, ParachuteConfig.java and StreamerConfig.java, with each panel's own keys
/// and the same texts; written once here). Three rows in OpenRocket's columns (ConfigFields):
/// - "Radial distance:", the distance of the object's center from the parent's axis (at least 0,
///   slider 0, 10 cm, 1 m; Java: DoubleModel(component, "RadialPosition", UNITS_LENGTH, 0));
/// - "Radial direction:", the direction of that displacement (any angle, which the core reduces
///   to -180 ... 180 degrees; slider -180 to 180 degrees; Java: DoubleModel(component,
///   "RadialDirection", UNITS_ANGLE));
/// - "Reset" at the right, which sets the direction and then the distance to 0
///   (resetRadialPosition()).
/// No tooltips: ParachuteConfig gives the Reset button its own (a Java slip it keeps, see there).
/// An inner tube has a tab of its own (InnerTubeConfig, Java's InnerTubeConfig keys, with
/// tooltips and a note).
///
/// Every field and the Reset button reach every selected mass object (the user's decision U3,
/// ComponentSelection): MassObject's setters of these values forward to the other selected
/// components in Java too (setRadialPosition() clamps the distance first). Deviation from
/// OpenRocket: Reset sets both values on each component inside one ComponentSelection::apply(),
/// so the rocket fires one merged event; Java's two setter calls fire one each. The values are
/// the same.
class RadialPositionPanel : public QWidget
{
    Q_OBJECT

public:
    /// The tab of the mass objects of @p scope's selection.
    /// @throws BugError when the scope has no valid selection or its main component is not a
    ///         MassObject
    RadialPositionPanel(BindingScope& scope, QWidget* parent);

    /// Makes the tab for @p panel and inserts it as the panel's second tab, "Radial position"
    /// ("Radial position configuration"), after "General" (Java: tabbedPane.insertTab(...,
    /// positionTab(), ..., 1)).
    /// @throws BugError as the constructor
    static RadialPositionPanel& addTab(RocketComponentConfig& panel);

    /// The tab addTab() made for @p panel.
    /// @throws BugError when @p panel has none
    [[nodiscard]] static RadialPositionPanel& of(const RocketComponentConfig& panel);

    [[nodiscard]] QGridLayout&                       grid() const noexcept { return *m_grid; }
    [[nodiscard]] const ConfigFields::NumberWidgets& positionWidgets() const noexcept
    {
        return m_position;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& directionWidgets() const noexcept
    {
        return m_direction;
    }
    [[nodiscard]] QPushButton& resetButton() const noexcept { return *m_reset; }

    /// "Reset": the direction 0, then the distance 0, for every selected mass object. Nothing
    /// once the selection is invalid.
    void resetRadialPosition();

    /// The radial distance of a mass object (Java: DoubleModel(component, "RadialPosition")).
    [[nodiscard]] static DoubleSource radialPositionSource();
    /// The radial direction of a mass object (Java: DoubleModel(component, "RadialDirection")).
    [[nodiscard]] static DoubleSource radialDirectionSource();

private:
    std::shared_ptr<ComponentSelection> m_selection;
    QGridLayout*                        m_grid;
    ConfigFields::NumberWidgets         m_position;
    ConfigFields::NumberWidgets         m_direction;
    QPushButton*                        m_reset{nullptr};
};

}  // namespace QtRocket::Gui
