#pragma once

#include <memory>

#include <QObject>
#include <QString>

#include "QtRocket/gui/binding/MaterialBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/MassObjectFields.h"
#include "QtRocket/gui/configdialog/RadialPositionPanel.h"
#include "QtRocket/gui/configdialog/RecoveryDeviceConfig.h"

class QGroupBox;
class QLabel;
class QPushButton;
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
class HtmlLabel;
class IntSpinBox;
class MaterialComboBox;
class PlacementPanel;

/// The configuration of a parachute (OpenRocket's ParachuteConfig, swing
/// gui/configdialog/ParachuteConfig.java), with the tabs General and Radial position before
/// Override and Comment:
///
/// - General ("General properties"), on the left:
///   - the box "Canopy": "Diameter:" (at least 0, slider 0, 40 cm, 1.5 m), "Material:" and the
///     combo box of the surface materials ("The component material affects the weight of the
///     component."), "Drag coefficient CD:" (at least 0, no unit selector, no slider; the
///     tooltip of the label and the spin box explains it) and "Reset", which sets the CD of every
///     selected parachute to Parachute::kDefaultCd ("Resets the drag coefficient (CD) to a value
///     of 0.80.": the value printed with two decimals in the locale's notation, Java's
///     String.format("%.2f"));
///     Java's spill hole is commented out there and not ported;
///   - the box "Shroud lines": "Number of lines:" (at least 0), "Line length:" (at least 0,
///     slider 0, 40 cm, 1.5 m) with "Automatic" ("Sets the line length to 1.5x the canopy
///     diameter."), "Material:" and the combo box of the line materials (the parachute's own
///     LineMaterial binding: Java's MaterialModel(..., Material.Type.LINE, "LineMaterial"), no
///     MaterialPanel);
///   on the right the PlacementPanel, whose box also holds "Packed length:", "Packed diameter:"
///   and its "Automatic" (MassObjectFields), and the group "Deployment" (RecoveryDeviceConfig).
/// - Radial position ("Radial position configuration"): RadialPositionPanel, whose Reset button
///   has the tooltip "ParachuteCfg.but.ResetRadial.ttip": Java sets the key itself as the text
///   (ParachuteConfig.java:364, a slip of OpenRocket's kept here, as the config-dialogs scout
///   recorded it; the key's text would be "Resets the component to the rocket centerline."), and
///   the tab's margin is Java's 25 px.
///
/// Every field reaches every selected parachute (the user's decision U3, ComponentSelection);
/// Java's Parachute, RecoveryDevice and MassObject setters forward to the others first, so the
/// results are Java's. Deviation from OpenRocket: Java leaves the packed diameter's "Automatic"
/// out of its focus order (ParachuteConfig.java:226, an omission); here the focus reaches it.
class ParachuteConfig : public RecoveryDeviceConfig
{
    Q_OBJECT

public:
    /// The configuration of the parachutes of @p selection.
    /// @throws BugError when @p selection is null or invalid, or its main component is not a
    ///         parachute
    ParachuteConfig(OpenRocketDocument& document, const GuiServices& services,
                    std::shared_ptr<ComponentSelection> selection, QWidget* parent);

    [[nodiscard]] QGroupBox& canopyGroup() const noexcept { return *m_canopy; }
    [[nodiscard]] const ConfigFields::NumberWidgets& diameterWidgets() const noexcept
    {
        return m_diameter;
    }
    [[nodiscard]] MaterialComboBox& materialComboBox() const noexcept { return *m_material; }
    [[nodiscard]] HtmlLabel&        cdLabel() const noexcept { return *m_cdLabel; }
    [[nodiscard]] const ConfigFields::NumberWidgets& cdWidgets() const noexcept { return m_cd; }
    [[nodiscard]] QPushButton& resetCdButton() const noexcept { return *m_resetCd; }

    [[nodiscard]] QGroupBox&  shroudGroup() const noexcept { return *m_shroud; }
    [[nodiscard]] IntSpinBox& lineCountSpinBox() const noexcept { return *m_lineCount; }
    [[nodiscard]] const ConfigFields::NumberWidgets& lineLengthWidgets() const noexcept
    {
        return m_lineLength;
    }
    [[nodiscard]] BoolCheckBox& lineLengthAutomatic() const noexcept { return *m_lineAutomatic; }
    [[nodiscard]] MaterialComboBox& lineMaterialComboBox() const noexcept
    {
        return *m_lineMaterial;
    }

    [[nodiscard]] PlacementPanel& placement() const noexcept { return *m_placement; }
    [[nodiscard]] const MassObjectFields::SizeWidgets& sizeWidgets() const noexcept
    {
        return m_size;
    }
    [[nodiscard]] RadialPositionPanel& radialPosition() const
    {
        return RadialPositionPanel::of(*this);
    }

    /// "Reset" of the drag coefficient: Parachute::kDefaultCd for every selected parachute (Java:
    /// setCD(DEFAULT_CD), which makes it manual). Nothing once the selection is invalid.
    void resetCd();

    /// The canopy diameter of a parachute (Java: DoubleModel(component, "Diameter")).
    [[nodiscard]] static DoubleSource diameterSource();
    /// The number of shroud lines of a parachute (Java: IntegerModel(component, "LineCount")).
    [[nodiscard]] static IntSource lineCountSource();
    /// The shroud line length of a parachute, with its automatic state (Java: DoubleModel(
    /// component, "LineLength"), isLineLengthAutomatic() and setLineLengthAutomatic()).
    [[nodiscard]] static DoubleSource lineLengthSource();
    /// The shroud line material of a parachute (Java: MaterialModel(..., "LineMaterial")).
    [[nodiscard]] static MaterialSource lineMaterialSource();

    /// The tooltip of the CD's "Reset": "Resets the drag coefficient (C<sub>D</sub>) to a value of
    /// 0.80." (Java: String.format() of ParachuteCfg.but.ResetCd.ttip with DEFAULT_CD, in the
    /// default locale's notation, as QLocale() here).
    [[nodiscard]] static QString resetCdToolTip();

private:
    /// Makes the box "Canopy" as a child of @p left; the caller lays it out.
    QGroupBox& addCanopyGroup(QWidget& left);
    /// Makes the box "Shroud lines" as a child of @p left; the caller lays it out.
    QGroupBox& addShroudGroup(QWidget& left);

    QGroupBox*                  m_canopy{nullptr};
    ConfigFields::NumberWidgets m_diameter;
    MaterialComboBox*           m_material{nullptr};
    HtmlLabel*                  m_cdLabel{nullptr};
    ConfigFields::NumberWidgets m_cd;
    QPushButton*                m_resetCd{nullptr};

    QGroupBox*                  m_shroud{nullptr};
    IntSpinBox*                 m_lineCount{nullptr};
    ConfigFields::NumberWidgets m_lineLength;
    BoolCheckBox*               m_lineAutomatic{nullptr};
    MaterialComboBox*           m_lineMaterial{nullptr};

    PlacementPanel*               m_placement{nullptr};
    MassObjectFields::SizeWidgets m_size;
};

}  // namespace QtRocket::Gui
