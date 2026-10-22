#pragma once

#include <memory>

#include <QObject>

#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/MassObjectFields.h"
#include "QtRocket/gui/configdialog/RadialPositionPanel.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"

class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class BoolCheckBox;
class ComponentSelection;
class GuiServices;
class MaterialPanel;
class PlacementPanel;

/// The configuration of a shock cord (OpenRocket's ShockCordConfig, swing
/// gui/configdialog/ShockCordConfig.java), with the tabs General and Radial position before
/// Override and Comment:
///
/// - General ("General properties"): on the left "Shock cord length:" (at least 0, slider 0, 1 m,
///   10 m) with "Automatic" ("Sets the shock cord length to 3x the rocket length.") below it, and
///   the MaterialPanel of the line materials ("Shock cord material:", no finish); on the right
///   the PlacementPanel, whose box also holds "Packed length:", "Packed diameter:" and its
///   "Automatic" (MassObjectFields).
/// - Radial position ("Radial position configuration"): RadialPositionPanel.
///
/// Every field reaches every selected shock cord (the user's decision U3, ComponentSelection);
/// Java's ShockCord setters forward to the others first, so the results are Java's.
class ShockCordConfig : public RocketComponentConfig
{
    Q_OBJECT

public:
    /// The configuration of the shock cords of @p selection.
    /// @throws BugError when @p selection is null or invalid, or its main component is not a
    ///         shock cord
    ShockCordConfig(OpenRocketDocument& document, const GuiServices& services,
                    std::shared_ptr<ComponentSelection> selection, QWidget* parent);

    [[nodiscard]] const ConfigFields::NumberWidgets& cordLengthWidgets() const noexcept
    {
        return m_cordLength;
    }
    [[nodiscard]] BoolCheckBox&   cordLengthAutomatic() const noexcept { return *m_automatic; }
    [[nodiscard]] MaterialPanel&  material() const noexcept { return *m_material; }
    [[nodiscard]] PlacementPanel& placement() const noexcept { return *m_placement; }
    [[nodiscard]] const MassObjectFields::SizeWidgets& sizeWidgets() const noexcept
    {
        return m_size;
    }
    [[nodiscard]] RadialPositionPanel& radialPosition() const
    {
        return RadialPositionPanel::of(*this);
    }

    /// The cord length of a shock cord, with its automatic state (Java: DoubleModel(component,
    /// "CordLength"), isCordLengthAutomatic() and setCordLengthAutomatic()).
    [[nodiscard]] static DoubleSource cordLengthSource();

private:
    ConfigFields::NumberWidgets   m_cordLength;
    BoolCheckBox*                 m_automatic{nullptr};
    MaterialPanel*                m_material{nullptr};
    PlacementPanel*               m_placement{nullptr};
    MassObjectFields::SizeWidgets m_size;
};

}  // namespace QtRocket::Gui
