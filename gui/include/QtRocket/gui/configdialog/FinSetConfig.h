#pragma once

#include <memory>

#include <QObject>
#include <QString>

#include "QtRocket/gui/binding/MaterialBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/position/AxialMethod.h"

class QGridLayout;
class QGroupBox;
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
class MaterialComboBox;
class MaterialPanel;
class PlacementPanel;

/// The base of the configurations of the trapezoidal, elliptical and freeform fin sets
/// (OpenRocket's abstract FinSetConfig, swing gui/configdialog/FinSetConfig.java): the tab "Fin
/// tabs" (inserted in front of the common tabs; the subclass then inserts its General tab in front
/// of it, the freeform one its Shape tab after General), the rows and panels the three General
/// tabs share, the root fillets, and the buttons "Split fins" and "Convert to freeform" of the
/// button row.
///
/// "Fin tabs" (tooltip "Through-the-wall fin tabs"), in OpenRocket's rows and columns (label, spin
/// box, unit selector, slider): the bold heading "Through-the-wall fin tabs:"; "Tab length:" (at
/// least 0; slider from 0 to the fin's length, which it follows); "Tab height:" (0 to the largest
/// tab height FinSet::getMaxTabHeight() gives when the panel is made; slider from 0 to the largest
/// tab height as it changes); "Tab position:" (the tab offset in its method, unbounded; slider from
/// minus to plus half the fin's length); "relative to" and the tab offset method (all five
/// AxialMethod values); the button "Calculate automatically" (calculateAutoTab()). The labels carry
/// OpenRocket's tooltips.
///
/// The General tabs' shared parts (made by the subclass in its order, Java's): addFinCountRow()
/// (1 to 8 fins), addCantRow() (+-15 degrees, slider the same), addCrossSectionRow() (the three
/// cross-sections), addThicknessRow() (at least 0, slider 0 to 1 cm), and makeRightColumn(): the
/// placement (PlacementPanel) with the row "Fin rotation:" below its offset (slider -180 to 180
/// degrees; the field bounded to the same for a trapezoidal fin set, unbounded for the others, as
/// in OpenRocket), the material (MaterialPanel, bulk materials) and the root fillets: the border
/// "Root Fillets", "Fillet radius:" (at least 0, steps of 0.1 in the unit, slider 0 to 10 cm; the
/// tooltip of the fillets on the spin box, the unit selector and the slider and on the border's
/// panel) and "Fillet material:" (the bulk materials, a MaterialBinding of FinSet's fillet material
/// of its own, not a MaterialPanel, as in OpenRocket).
///
/// The buttons (addFinSetButtons(), before Cancel): "Split fins", enabled while the main fin set
/// has more than one fin (refreshed by updateFields(), as Java), and, unless the main fin set is a
/// freeform one, "Convert to freeform".
///
/// Several fin sets (the user's decision U3, ComponentSelection): every field sets each selected
/// fin set with its own setter, as OpenRocket's FinSet setters of these properties forward to the
/// others before their early returns (setFinCount, setCantAngle, setThickness, setCrossSection,
/// setBaseRotation, setTabHeight, setTabLength, setTabOffset, setTabOffsetMethod, setFilletRadius,
/// setFilletMaterial); a tab offset method keeps each fin set's own tab where it is.
class FinSetConfig : public RocketComponentConfig
{
    Q_OBJECT

public:
    // ------------------------------------------------------------------------------ Fin tabs

    [[nodiscard]] const ConfigFields::NumberWidgets& tabLengthWidgets() const noexcept
    {
        return m_tabLength;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& tabHeightWidgets() const noexcept
    {
        return m_tabHeight;
    }
    /// "Tab position:".
    [[nodiscard]] const ConfigFields::NumberWidgets& tabOffsetWidgets() const noexcept
    {
        return m_tabOffset;
    }
    /// "relative to".
    [[nodiscard]] EnumComboBox& tabOffsetMethodComboBox() const noexcept
    {
        return *m_tabOffsetMethod;
    }
    /// "Calculate automatically".
    [[nodiscard]] QPushButton& autoCalcButton() const noexcept { return *m_autoCalc; }

    // ------------------------------------------------------------------------------ General

    /// The shared rows of the General tab; null until the subclass made them.
    [[nodiscard]] IntSpinBox*   finCountSpinBox() const noexcept { return m_finCount; }
    [[nodiscard]] EnumComboBox* crossSectionComboBox() const noexcept { return m_crossSection; }
    [[nodiscard]] const ConfigFields::NumberWidgets& cantWidgets() const noexcept { return m_cant; }
    [[nodiscard]] const ConfigFields::NumberWidgets& thicknessWidgets() const noexcept
    {
        return m_thickness;
    }
    /// The right column's parts; null until the subclass made it (makeRightColumn()).
    [[nodiscard]] PlacementPanel* placementPanel() const noexcept { return m_placement; }
    /// "Fin rotation:", in the placement panel.
    [[nodiscard]] const ConfigFields::NumberWidgets& baseRotationWidgets() const noexcept
    {
        return m_baseRotation;
    }
    [[nodiscard]] MaterialPanel* materialPanel() const noexcept { return m_material; }
    /// The panel "Root Fillets", its "Fillet radius:" and its "Fillet material:".
    [[nodiscard]] QGroupBox* filletPanel() const noexcept { return m_filletPanel; }
    [[nodiscard]] const ConfigFields::NumberWidgets& filletRadiusWidgets() const noexcept
    {
        return m_filletRadius;
    }
    [[nodiscard]] MaterialComboBox* filletMaterialComboBox() const noexcept
    {
        return m_filletMaterial;
    }

    // ------------------------------------------------------------------------------ buttons

    /// "Split fins" and "Convert to freeform"; null until addFinSetButtons(), and "Convert to
    /// freeform" for a freeform fin set.
    [[nodiscard]] QPushButton* splitButton() const noexcept { return m_split; }
    [[nodiscard]] QPushButton* convertButton() const noexcept { return m_convert; }

    /// RocketComponentConfig's, then "Split fins" enabled while the main fin set has more than one
    /// fin (Java: updateFields()).
    void updateFields() override;

    // ------------------------------------------------------------------------------ actions

    /// "Calculate automatically" (Java: calculateAutoTab()), when the main fin set's parent is a
    /// SymmetricComponent, inside OpenRocketDocument::startUndo("Compute fin tabs") and stopUndo():
    /// from the inner tubes among the parent's direct children that overlap the fin and the
    /// centering rings among them (FinTabCalculator::findTabSupports()), through the fields, in
    /// Java's order:
    /// - when the rings place the tab: the tab offset method TOP, the offset and the length
    ///   FinTabCalculator::computeFinTabLength() gives for the main fin set, then the method it
    ///   had before;
    /// - otherwise the method TOP, the offset 0 and the length of the fin;
    /// - then, once the tab is placed, the height FinTabCalculator::tabHeight() gives (from the
    ///   root to the largest of those tubes), when it is at least 0, and the tab height field
    ///   shows it in the default length unit.
    /// Every value comes from the main fin set and reaches every selected fin set through the
    /// fields, as Java's models forward them to the config listeners.
    void calculateAutoTab();

    /// "Convert to freeform" (Java's action of the button): closes the dialog the panel is in
    /// (ComponentConfigDialog::disposeDialog(), which keeps and commits the changes; a panel in no
    /// dialog commits its fields and ends its bindings), adds the undo position "Convert fin set",
    /// converts the main fin set and every other selected fin set into freeform ones
    /// (FreeformFinSet::convertFinSet() without its own freeze, all inside one freeze of the
    /// rocket, so that one event tells the tree), and opens the dialog again, for the window the
    /// dialog was shown for, on the new main fin set with the other new ones selected (Java's
    /// convertFinSet() makes the converted config listeners the new fin set's).
    ///
    /// Deviations from OpenRocket:
    /// - Every selected fin set is converted. Java's loop over the config listeners removes the
    ///   listener it converts from the LinkedList it walks, which ends the loop after the first
    ///   one (the third and later selected fin sets stay as they are, and the reopened dialog
    ///   edits only the first two) or, with four or more selected, throws a
    ///   ConcurrentModificationException once the first two are converted (no dialog opens).
    /// - It runs at once; Java defers the conversion to the event queue (invokeLater) after the
    ///   dialog has gone. Nothing of the closed dialog runs in between here: its bindings have
    ///   ended (BindingScope::release()) and it is deleted later.
    void convertToFreeform();

    /// "Split fins" (Java's action of the button): closes the dialog the panel is in, as
    /// convertToFreeform() does, adds the undo position "Split fin set" and splits the main fin
    /// set and every other selected fin set of its class into single fins (FinSet::splitFins()
    /// without its own freeze, all inside one freeze of the rocket). No dialog opens.
    ///
    /// Deviation from OpenRocket: every selected fin set is split; Java's loop over the config
    /// listeners stops after the first one, or throws a ConcurrentModificationException with
    /// three or more listeners, as in convertToFreeform().
    void splitFins();

    // ------------------------------------------------------------------------------ sources

    /// The fin count (Java: IntegerModel(component, "FinCount", 1, 8), bounds by the panels).
    [[nodiscard]] static IntSource finCountSource();
    /// The cant angle (Java: DoubleModel(component, "CantAngle", UNITS_ANGLE, ...)).
    [[nodiscard]] static DoubleSource cantAngleSource();
    /// The cross-section (Java: EnumModel(component, "CrossSection")).
    [[nodiscard]] static EnumSource<FinCrossSection> crossSectionSource();
    /// The fins' thickness (Java: DoubleModel(component, "Thickness", UNITS_LENGTH, 0)).
    [[nodiscard]] static DoubleSource thicknessSource();
    /// The angle of the first fin (Java: DoubleModel(component, "BaseRotation", UNITS_ANGLE)).
    [[nodiscard]] static DoubleSource baseRotationSource();
    /// The fin's length, read-only (Java: DoubleModel(component, "Length", ...)).
    [[nodiscard]] static DoubleSource finLengthSource();
    /// The tab's length, height and offset in its method (Java: "TabLength", "TabHeight",
    /// "TabOffset").
    [[nodiscard]] static DoubleSource tabLengthSource();
    [[nodiscard]] static DoubleSource tabHeightSource();
    [[nodiscard]] static DoubleSource tabOffsetSource();
    /// The largest tab height, read-only (Java: DoubleModel(component, "MaxTabHeight", ...)).
    [[nodiscard]] static DoubleSource maxTabHeightSource();
    /// The tab offset method (Java: EnumModel(component, "TabOffsetMethod")).
    [[nodiscard]] static EnumSource<AxialMethod> tabOffsetMethodSource();
    /// The fillet radius and material (Java: "FilletRadius", MaterialModel(..., "FilletMaterial")).
    [[nodiscard]] static DoubleSource   filletRadiusSource();
    [[nodiscard]] static MaterialSource filletMaterialSource();

protected:
    /// The base of the configuration of the fin sets of @p selection (see RocketComponentConfig):
    /// inserts the tab "Fin tabs" at 0.
    /// @throws BugError when @p selection is null or invalid, or its main component is no FinSet
    FinSetConfig(OpenRocketDocument& document, const GuiServices& services,
                 std::shared_ptr<ComponentSelection> selection, QWidget* parent);

    /// The row of the fin count in @p row of @p grid, whose widget is @p page: the label @p label
    /// (with @p toolTip on the label and the spin box; none when it is empty) and the spin box.
    void addFinCountRow(QWidget& page, QGridLayout& grid, int row, const QString& label,
                        const QString& toolTip);
    /// The row of the cant angle, @p toolTip on the label.
    void addCantRow(QGridLayout& grid, int row, const QString& label, const QString& toolTip);
    /// The row of the cross-section: the label @p label and the combo box over the columns
    /// after it.
    void addCrossSectionRow(QWidget& page, QGridLayout& grid, int row, const QString& label);
    /// The row of the thickness.
    void addThicknessRow(QGridLayout& grid, int row, const QString& label);

    /// The right column of a General tab, a child of @p page: the placement with "Fin rotation:"
    /// (bounded to -pi ... pi when @p boundedRotation, as TrapezoidFinSetConfig; unbounded
    /// otherwise), the material and the root fillets.
    [[nodiscard]] QWidget& makeRightColumn(QWidget& page, bool boundedRotation);

    /// Adds "Split fins" and, unless the main fin set is a FreeformFinSet, "Convert to freeform" to
    /// the button row (Java: addFinSetButtons()).
    void addFinSetButtons();

private:
    /// The tab "Fin tabs" (Java: finTabPanel()).
    [[nodiscard]] QWidget& makeFinTabTab();
    /// The panel "Root Fillets" (Java: filletMaterialPanel()), a child of @p parent.
    [[nodiscard]] QGroupBox& makeFilletPanel(QWidget& parent);
    /// The window the dialog of this panel was shown for, and closes the dialog (or, in no
    /// dialog, commits the fields and ends the bindings); null in no dialog.
    [[nodiscard]] QWidget* closeForStructuralChange();

    ConfigFields::NumberWidgets m_tabLength;
    ConfigFields::NumberWidgets m_tabHeight;
    ConfigFields::NumberWidgets m_tabOffset;
    EnumComboBox*               m_tabOffsetMethod{nullptr};
    QPushButton*                m_autoCalc{nullptr};

    IntSpinBox*                 m_finCount{nullptr};
    ConfigFields::NumberWidgets m_cant;
    EnumComboBox*               m_crossSection{nullptr};
    ConfigFields::NumberWidgets m_thickness;
    PlacementPanel*             m_placement{nullptr};
    ConfigFields::NumberWidgets m_baseRotation;
    MaterialPanel*              m_material{nullptr};
    QGroupBox*                  m_filletPanel{nullptr};
    ConfigFields::NumberWidgets m_filletRadius;
    MaterialComboBox*           m_filletMaterial{nullptr};

    QPushButton* m_split{nullptr};
    QPushButton* m_convert{nullptr};
};

}  // namespace QtRocket::Gui
