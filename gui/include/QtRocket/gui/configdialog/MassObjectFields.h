#pragma once

#include <QCoreApplication>
#include <QString>

#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"

class QGridLayout;

namespace QtRocket
{
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class BindingScope;
class BoolCheckBox;

/// The size fields of a mass object, which the four mass object panels share (MassComponentConfig,
/// ShockCordConfig, ParachuteConfig and StreamerConfig; OpenRocket writes them out in each):
/// - the (packed) length (Java: DoubleModel(component, "Length", UNITS_LENGTH, 0): at least 0,
///   slider 0, 10 cm, 50 cm);
/// - the (packed) DIAMETER (the radius times 2: DoubleModel(component, "Radius", 2, UNITS_LENGTH,
///   0): at least 0, slider 0, 4 cm, 20 cm);
/// - below it "Automatic" ("Matches the packed diameter to the inside diameter of the parent
///   component.", Java's key ParachuteCfg.checkbox.AutomaticPacked in all four panels), the
///   automatic radius (MassObject::setRadiusAutomatic(): the radius of the parent's inside).
///
/// The "Automatic" box is enabled only while the main component's parent offers a radius
/// (MassObject::getMaxParentRadius() > 0: not inside a mass component, say), decided again on
/// every change of the rocket (Java: a listener of the parent's changes, which are the rocket's).
/// As in Java, the main component decides it for every selected one.
///
/// Every field reaches every selected mass object (the user's decision U3, ComponentSelection):
/// Java's MassObject setters forward to the other selected components first (setRadius() clamps
/// the radius first), so the results are Java's.
class MassObjectFields
{
    Q_DECLARE_TR_FUNCTIONS(MassObjectFields)

public:
    MassObjectFields() = delete;

    /// The widgets addSizeRows() makes.
    struct SizeWidgets
    {
        ConfigFields::NumberWidgets length;
        ConfigFields::NumberWidgets diameter;
        BoolCheckBox*               automatic = nullptr;
    };

    /// Adds the rows of the length (labelled @p lengthLabel), the diameter (@p diameterLabel) and
    /// "Automatic" to @p grid from @p row on (three rows), with the bindings in @p scope; the
    /// widgets are named "Length", "Radius" and "RadiusAutomatic". @p grid's widget is their
    /// parent (ConfigFields).
    /// @throws BugError when the scope has no valid selection or its main component is not a
    ///         MassObject
    static SizeWidgets addSizeRows(BindingScope& scope, QGridLayout& grid, int row,
                                   const QString& lengthLabel, const QString& diameterLabel);

    /// The (packed) length of a mass object (Java: DoubleModel(component, "Length")).
    [[nodiscard]] static DoubleSource lengthSource();
    /// The (packed) radius of a mass object, with its automatic state (Java: DoubleModel(
    /// component, "Radius"), isRadiusAutomatic() and setRadiusAutomatic()).
    [[nodiscard]] static DoubleSource radiusSource();

    /// Whether the "Automatic" box of @p component's radius is enabled: it is a mass object
    /// whose parent offers a radius (getMaxParentRadius() > 0).
    [[nodiscard]] static bool isAutomaticRadiusEnabled(const RocketComponent& component);
};

}  // namespace QtRocket::Gui
