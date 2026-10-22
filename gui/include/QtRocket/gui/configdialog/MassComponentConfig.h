#pragma once

#include <memory>

#include <QObject>

#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/MassObjectFields.h"
#include "QtRocket/gui/configdialog/RadialPositionPanel.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/rocket/MassComponent.h"

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
class PlacementPanel;

/// The configuration of a mass component (OpenRocket's MassComponentConfig, swing
/// gui/configdialog/MassComponentConfig.java), with the tabs General and Radial position before
/// Override and Comment:
///
/// - General ("General properties"): on the left "Mass type:" (the combo box of the eight types,
///   MassComponent::kAllMassComponentTypes), "Mass:" (at least 0, slider 0, 50 g, 500 g),
///   "Approximate density:" (bulk density, at least 0, slider 500, 2000, 10000 kg/m3: the mass is
///   the density times the component's volume), "Length:", "Diameter:" and "Automatic"
///   (MassObjectFields); on the right the PlacementPanel. No material.
/// - Radial position ("Radial position configuration"): RadialPositionPanel.
///
/// Every field reaches every selected mass component (the user's decision U3,
/// ComponentSelection). Of the setters, MassComponent.setDensity() is the one whose result differs
/// from Java's: it computes the mass from the MAIN component's volume and forwards that mass
/// (setComponentMass()), so in Java every selected mass component gets the main's mass (item 6 in
/// ComponentSelection.h); here each gets the density times its own volume.
class MassComponentConfig : public RocketComponentConfig
{
    Q_OBJECT

public:
    /// The configuration of the mass components of @p selection.
    /// @throws BugError when @p selection is null or invalid, or its main component is not a mass
    ///         component
    MassComponentConfig(OpenRocketDocument& document, const GuiServices& services,
                        std::shared_ptr<ComponentSelection> selection, QWidget* parent);

    [[nodiscard]] EnumComboBox& typeComboBox() const noexcept { return *m_type; }
    [[nodiscard]] const ConfigFields::NumberWidgets& massWidgets() const noexcept { return m_mass; }
    [[nodiscard]] const ConfigFields::NumberWidgets& densityWidgets() const noexcept
    {
        return m_density;
    }
    [[nodiscard]] const MassObjectFields::SizeWidgets& sizeWidgets() const noexcept
    {
        return m_size;
    }
    [[nodiscard]] PlacementPanel&      placement() const noexcept { return *m_placement; }
    [[nodiscard]] RadialPositionPanel& radialPosition() const
    {
        return RadialPositionPanel::of(*this);
    }

    /// The type of a mass component (Java: EnumModel(component, "MassComponentType")).
    [[nodiscard]] static EnumSource<MassComponent::MassComponentType> typeSource();
    /// The mass of a mass component (Java: DoubleModel(component, "ComponentMass")).
    [[nodiscard]] static DoubleSource massSource();
    /// The density of a mass component (Java: DoubleModel(component, "Density")).
    [[nodiscard]] static DoubleSource densitySource();

private:
    EnumComboBox*                 m_type{nullptr};
    ConfigFields::NumberWidgets   m_mass;
    ConfigFields::NumberWidgets   m_density;
    MassObjectFields::SizeWidgets m_size;
    PlacementPanel*               m_placement{nullptr};
};

}  // namespace QtRocket::Gui
