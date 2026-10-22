#pragma once

#include <memory>

#include <QObject>

#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/FinSetConfig.h"

class QGridLayout;
class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ComponentSelection;
class GuiServices;

/// The configuration of a trapezoidal fin set (OpenRocket's TrapezoidFinSetConfig, swing
/// gui/configdialog/TrapezoidFinSetConfig.java): the tab "General" before FinSetConfig's "Fin
/// tabs" and the common ones, and FinSetConfig's buttons "Split fins" and "Convert to freeform".
///
/// General, in OpenRocket's rows and columns (label, spin box, unit selector, slider):
/// - "Number of fins:" (1 to 8; a spin box only; the tooltip "The number of fins in the fin set."
///   on the label and the spin box);
/// - "Fin cant:" (+-15 degrees, slider the same; the label's tooltip);
/// - "Root chord:", "Tip chord:" and "Height:" (at least 0; sliders 0, 5 cm, 20 cm);
/// - "Sweep length:" (unbounded; slider from -1.1 times the tip chord to 1.1 times the root chord,
///   which it follows);
/// - "Sweep angle:" (+-89 degrees; slider +-45 degrees);
/// - "Fin cross section:" and its combo box;
/// - "Thickness:" (at least 0; slider 0 to 1 cm);
/// - to the right of them FinSetConfig's column: the placement with "Fin rotation:" (bounded to
///   -180 ... 180 degrees, as OpenRocket's DoubleModel(component, "BaseRotation", UNITS_ANGLE,
///   -PI, PI) here; the other fin set panels leave it unbounded), the material and the root
///   fillets.
///
/// Several trapezoidal fin sets (the user's decision U3, ComponentSelection): every field sets
/// each selected fin set with its own setter; OpenRocket's TrapezoidFinSet setters of these
/// properties forward before their early returns (setRootChord, setTipChord, setHeight, setSweep,
/// setSweepAngle). The sweep angle is cascade 7 of ComponentSelection.h: each selected fin set gets
/// the sweep tan(angle) times its own height here; in OpenRocket the main one's setSweep(tan(angle)
/// times its height) is forwarded after the angle, so every other fin set gets the main one's
/// sweep.
class TrapezoidFinSetConfig : public FinSetConfig
{
    Q_OBJECT

public:
    /// The configuration of the trapezoidal fin sets of @p selection (see RocketComponentConfig).
    /// @throws BugError when @p selection is null or invalid, or its main component is not a
    ///         TrapezoidFinSet
    TrapezoidFinSetConfig(OpenRocketDocument& document, const GuiServices& services,
                          std::shared_ptr<ComponentSelection> selection, QWidget* parent);

    [[nodiscard]] const ConfigFields::NumberWidgets& rootChordWidgets() const noexcept
    {
        return m_rootChord;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& tipChordWidgets() const noexcept
    {
        return m_tipChord;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& heightWidgets() const noexcept
    {
        return m_height;
    }
    /// "Sweep length:".
    [[nodiscard]] const ConfigFields::NumberWidgets& sweepWidgets() const noexcept
    {
        return m_sweep;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& sweepAngleWidgets() const noexcept
    {
        return m_sweepAngle;
    }

private:
    /// The General tab.
    [[nodiscard]] QWidget& makeGeneralTab();
    /// The rows of the root chord, the tip chord and the height.
    void addOutlineRows(QGridLayout& grid);
    /// The rows of the sweep length and the sweep angle.
    void addSweepRows(QGridLayout& grid);

    ConfigFields::NumberWidgets m_rootChord;
    ConfigFields::NumberWidgets m_tipChord;
    ConfigFields::NumberWidgets m_height;
    ConfigFields::NumberWidgets m_sweep;
    ConfigFields::NumberWidgets m_sweepAngle;
};

}  // namespace QtRocket::Gui
