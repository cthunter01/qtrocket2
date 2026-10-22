#pragma once

#include <QGroupBox>
#include <QObject>

#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/rocket/position/AxialMethod.h"

class QGridLayout;
class QWidget;

namespace QtRocket::Gui
{

class BindingScope;
class EnumComboBox;

/// The placement of a component relative to its parent (OpenRocket's PlacementPanel, swing
/// gui/configdialog/PlacementPanel.java): the border "Placement", the combo box "Position relative
/// to:" over AxialMethod's offset methods (kAxialOffsetMethods: the tip of the rocket, the top,
/// middle or bottom of the parent) and the row "plus" with the axial offset, its unit and a slider
/// from minus to plus the parent's length (both bounds follow the parent's length, Java's
/// DoubleModel(parent, "Length", -1) and DoubleModel(parent, "Length")).
///
/// Choosing a method converts the offset of every selected component (the core keeps each one's
/// position) and the offset field shows the main component's new offset: RocketComponent::
/// setAxialMethod() fires no event (as in Java), so ComponentSelection::apply() refreshes the
/// bindings itself (Java's combo listener calls the offset model's stateChanged()).
///
/// Deviation from OpenRocket: in a multi-edit the offset field and slider set the offset of every
/// selected component (the user's decision U3: each setter on every selected component). Java's
/// setAxialOffset(double) has no config-listener loop, so there they move the main component only
/// and the others keep their offsets (ComponentSelection lists this with the other differences).
///
/// The panels that use it add rows of their own below the offset (Java: placementPanel.add() of
/// the fin rotation, a packed length, the rail button's angle): grid() and nextRow().
///
/// The bindings live in the dialog's BindingScope (Java: register() with the panel's
/// invalidatables); the selection's main component must have a parent.
class PlacementPanel : public QGroupBox
{
    Q_OBJECT

public:
    /// The placement of the components of @p scope's selection.
    /// @throws BugError when the scope has no valid selection or its main component no parent
    PlacementPanel(BindingScope& scope, QWidget* parent);

    /// The grid of the panel (columns as ConfigFields), for rows a panel adds.
    [[nodiscard]] QGridLayout& grid() const noexcept { return *m_grid; }

    /// The first row of grid() that is free.
    [[nodiscard]] int nextRow() const;

    /// The combo box of the axial method and the widgets of the offset.
    [[nodiscard]] EnumComboBox& methodComboBox() const noexcept { return *m_method; }
    [[nodiscard]] const ConfigFields::NumberWidgets& offsetWidgets() const noexcept
    {
        return m_offset;
    }

    /// The axial method of a component (Java's EnumModel(component, "AxialMethod")).
    [[nodiscard]] static EnumSource<AxialMethod> axialMethodSource();
    /// The axial offset of a component in its axial method (Java's DoubleModel(component,
    /// "AxialOffset")).
    [[nodiscard]] static DoubleSource axialOffsetSource();
    /// The length of a component's parent, 0 without a parent (Java's DoubleModel(parent,
    /// "Length")): read-only.
    [[nodiscard]] static DoubleSource parentLengthSource();

private:
    QGridLayout*                m_grid;
    EnumComboBox*               m_method{nullptr};
    ConfigFields::NumberWidgets m_offset;
};

}  // namespace QtRocket::Gui
