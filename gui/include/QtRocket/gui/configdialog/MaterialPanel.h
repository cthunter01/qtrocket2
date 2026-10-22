#pragma once

#include <QGroupBox>
#include <QObject>
#include <QString>

#include "QtRocket/gui/binding/MaterialBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/Finish.h"

class QPushButton;
class QWidget;

namespace QtRocket
{
class MaterialStorage;
class OpenRocketDocument;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class BindingScope;
class EnumComboBox;
class MaterialComboBox;

/// The material of a component and, for an external component, its surface finish (OpenRocket's
/// MaterialPanel, swing gui/configdialog/MaterialPanel.java): the border "Material", the material
/// label and the material combo box (MaterialComboBox: the application's materials of the type,
/// then the document's, grouped and searchable), then, only for an ExternalComponent, "Component
/// finish:", the finish combo box (each finish with its roughness in the default roughness unit,
/// Java's Finish.toString()) and "Set for all", which gives the main component's finish to every
/// external component of the rocket in one undo step "Set rocket finish"
/// (OpenRocketDocument::startUndo() and stopUndo(), as Java).
///
/// The material is the component's own (Java: MaterialModel's part "Material": ExternalComponent,
/// StructuralComponent, RecoveryDevice and ShockCord each have getMaterial() and setMaterial()):
/// componentMaterialSource(). The bindings live in the dialog's BindingScope.
///
/// Deviation from OpenRocket: the combo box has no "Add custom material" and "Edit materials"
/// buttons yet (the material editor is Milestone 5; MaterialComboBox).
class MaterialPanel : public QGroupBox
{
    Q_OBJECT

public:
    /// The material of type @p type of the components of @p scope's selection, offering the
    /// materials of @p applicationMaterials and of @p document, with OpenRocket's texts
    /// "Component material:" and "Component finish:".
    /// @throws BugError when the scope has no valid selection or the main component has no
    ///         material of @p type
    MaterialPanel(BindingScope& scope, OpenRocketDocument& document,
                  MaterialStorage& applicationMaterials, Material::Type type, QWidget* parent);

    /// The same with the label texts @p materialText and @p finishLabelText (Java: a shock
    /// cord's "Shock cord material:").
    MaterialPanel(BindingScope& scope, OpenRocketDocument& document,
                  MaterialStorage& applicationMaterials, Material::Type type,
                  const QString& materialText, const QString& finishLabelText, QWidget* parent);

    [[nodiscard]] MaterialComboBox& materialComboBox() const noexcept { return *m_material; }
    /// The finish combo box and the "Set for all" button; null for a component that is not an
    /// ExternalComponent.
    [[nodiscard]] EnumComboBox* finishComboBox() const noexcept { return m_finish; }
    [[nodiscard]] QPushButton*  setForAllButton() const noexcept { return m_setForAll; }

    /// The material of a component: getMaterial() and setMaterial() of an ExternalComponent, a
    /// StructuralComponent, a RecoveryDevice or a ShockCord, whichever it is (Java finds them by
    /// reflection); setting reaches every selected component that has one.
    [[nodiscard]] static MaterialSource componentMaterialSource();

    /// The surface finish of an ExternalComponent (Java: EnumModel(component, "Finish")).
    [[nodiscard]] static EnumSource<Finish> finishSource();

    /// The text of @p finish in the combo box: its name and its roughness in the default unit of
    /// roughness, "Regular paint (60 um)" with a micro sign (Java's Finish.toString()).
    [[nodiscard]] static QString finishText(Finish finish);

private:
    /// "Set for all": the main component's finish for every external component of the rocket.
    void setFinishForAll(OpenRocketDocument& document);

    BindingScope*     m_scope;
    MaterialComboBox* m_material{nullptr};
    EnumComboBox*     m_finish{nullptr};
    QPushButton*      m_setForAll{nullptr};
};

}  // namespace QtRocket::Gui
