#pragma once

#include <memory>

#include <QObject>

#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/FinSetConfig.h"

class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ComponentSelection;
class GuiServices;

/// The configuration of an elliptical fin set (OpenRocket's EllipticalFinSetConfig, swing
/// gui/configdialog/EllipticalFinSetConfig.java): the tab "General" before FinSetConfig's "Fin
/// tabs" and the common ones, and FinSetConfig's buttons "Split fins" and "Convert to freeform".
///
/// General, in OpenRocket's rows and columns (label, spin box, unit selector, slider):
/// - "Number of fins:" (1 to 8; a spin box only; no tooltip, unlike the trapezoidal fin set's);
/// - "Fin cant:" (+-15 degrees, slider the same; the label's tooltip);
/// - "Root chord:" (the fin set's length, at least 0) and "Height:" (at least 0), sliders 0, 5 cm,
///   20 cm;
/// - "Fin cross section:" and its combo box;
/// - "Thickness:" (at least 0; slider 0 to 1 cm);
/// - to the right of them FinSetConfig's column: the placement with "Fin rotation:" (unbounded,
///   the setter reduces it to -180 ... 180 degrees; slider -180 to 180 degrees), the material and
///   the root fillets.
///
/// Several elliptical fin sets (the user's decision U3, ComponentSelection): every field sets each
/// selected fin set with its own setter, as OpenRocket's EllipticalFinSet.setLength() and
/// setHeight() forward before their early returns.
class EllipticalFinSetConfig : public FinSetConfig
{
    Q_OBJECT

public:
    /// The configuration of the elliptical fin sets of @p selection (see RocketComponentConfig).
    /// @throws BugError when @p selection is null or invalid, or its main component is not an
    ///         EllipticalFinSet
    EllipticalFinSetConfig(OpenRocketDocument& document, const GuiServices& services,
                           std::shared_ptr<ComponentSelection> selection, QWidget* parent);

    /// "Root chord:", the fin set's length (Java's property "Length").
    [[nodiscard]] const ConfigFields::NumberWidgets& rootChordWidgets() const noexcept
    {
        return m_rootChord;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& heightWidgets() const noexcept
    {
        return m_height;
    }

private:
    /// The General tab.
    [[nodiscard]] QWidget& makeGeneralTab();

    ConfigFields::NumberWidgets m_rootChord;
    ConfigFields::NumberWidgets m_height;
};

}  // namespace QtRocket::Gui
