#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "QtRocket/gui/binding/Binding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialDatabase.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{
class MaterialStorage;
class OpenRocketDocument;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The source of a MaterialBinding: a component's material of one type (Java: get<Name> and
/// set<Name>(Material), the name "Material", "LineMaterial" or "FilletMaterial").
using MaterialSource = PropertySource<Material>;

/// The model of a material combo box (Java: MaterialModel, swing gui/adaptors/MaterialModel.java,
/// and the item grouping of its GroupableAndSearchableComboBox): a component's material, its
/// fillet material, a parachute's line material.
///
/// The materials offered, materials(), are those of the binding's type in the application's
/// storage (MaterialStorage::database(type): the built-in materials and the user's), followed by
/// the document's (OpenRocketDocument::getDocumentMaterials()), each part in its database's order
/// (Material::compareTo()) (Java: getElementAt() over Databases.getDatabase(type), then the
/// document's database of the type). The binding re-reads them whenever a material is added to
/// or removed from either database, and tells its listeners. groups() is the combo box's view of
/// the same materials (Java: GroupableAndSearchableComboBox.constructItemGroupMapFromList()): one
/// entry per material group that has materials, by group priority, each group's materials sorted
/// by Material::compareTo() (a stable sort, as Java's List.sort()). A material's text is
/// displayString(): Material::toString() (the name and the density in the default unit), after
/// "(ud) " for a user-defined material (Java: MaterialComboBox.getDisplayString()).
///
/// value() is the main component's material (Java: getSelectedItem()); indexOf() finds it among
/// materials() (Material's equality), -1 when it is none of them. select() sets a material on
/// every selected component (ComponentSelection::apply()), also the main's own (Java's
/// setSelectedItem() calls the setter every time: in a multi-edit the main's material reaches the
/// others). The binding caches the value and the enabled state, re-reads them once per event of
/// the rocket and tells its listeners when they changed.
///
/// The databases outlive the binding or the selection: the application's storage lives as long
/// as the application, the document's as long as its rocket, whose end makes the selection
/// invalid; after that the binding reads neither.
///
/// Deviations from OpenRocket:
/// - select() is a widget write: dropped while the binding notifies (a combo box refreshing
///   itself) and once the selection is invalid. Java has no such guard (its combo never writes
///   back what it is told), and here a refreshing combo's echo would give every other selected
///   component the main's material.
/// - The listeners hear of a change of the value or the enabled state on any event of the rocket
///   (Java: on a mass change, the event a component's material change fires).
/// - A material of another type is a BugError in select() and in the main component (Java's
///   setters throw an IllegalArgumentException), as are a source without a setter, a source
///   whose getChecked gives an Error (only a DoubleBinding shows one) and a document of another
///   rocket than the selection's.
/// - Not ported: addCustomMaterial() and the combo box's "Add custom material" and "Edit
///   materials" buttons (the custom material dialog and the material editor, Milestone 5).
class MaterialBinding final : public Binding
{
public:
    /// The materials of one group, as groups() gives them.
    struct GroupItems
    {
        MaterialGroup group{MaterialGroup::OTHER};
        /// Indices into materials(), sorted by Material::compareTo().
        std::vector<int> indices;
    };

    /// A binding of @p source, a material of @p type, on the components of @p selection, offering
    /// the materials of @p applicationMaterials and of @p document.
    /// @throws BugError when @p selection is null, the source has no setter, @p type is CUSTOM,
    ///         @p document is not the selection's rocket's, or the main component's material is
    ///         not of @p type
    MaterialBinding(std::shared_ptr<ComponentSelection> selection, MaterialSource source,
                    Material::Type type, MaterialStorage& applicationMaterials,
                    OpenRocketDocument& document);

    ~MaterialBinding() override;
    MaterialBinding(const MaterialBinding&)            = delete;
    MaterialBinding& operator=(const MaterialBinding&) = delete;
    MaterialBinding(MaterialBinding&&)                 = delete;
    MaterialBinding& operator=(MaterialBinding&&)      = delete;

    /// The type of the materials.
    [[nodiscard]] Material::Type getType() const noexcept { return m_type; }

    /// The cached material of the main component (Java: getSelectedItem()); once the selection
    /// is invalid, the last material read.
    [[nodiscard]] const Material& value() const noexcept { return m_value; }

    /// The combo box's write (Java: setSelectedItem()): sets @p material on every selected
    /// component, see the class comment. False, with nothing set, while firing and once the
    /// selection is invalid.
    /// @throws BugError when @p material is not of the binding's type
    bool select(const Material& material);

    /// Whether the combo box can be used: the source's isEnabled of the main component (true
    /// without one); false once the selection is invalid.
    [[nodiscard]] bool isEnabled() const noexcept { return isValid() && m_enabled; }

    /// The materials offered (Java: getAllMaterials()); see the class comment.
    [[nodiscard]] const std::vector<Material>& materials() const noexcept { return m_materials; }

    /// The number of materials offered (Java: getSize()).
    [[nodiscard]] int size() const noexcept;

    /// The material at @p index of materials(), or nullptr out of range (Java: getElementAt(),
    /// null).
    [[nodiscard]] const Material* materialAt(int index) const noexcept;

    /// The index of the first material of materials() that equals @p material, or -1.
    [[nodiscard]] int indexOf(const Material& material) const noexcept;

    /// The materials by group, as the combo box's menu shows them (see the class comment).
    [[nodiscard]] std::vector<GroupItems> groups() const;

    /// The text of @p material in the combo box: "(ud) " for a user-defined material, then
    /// Material::toString() (Java: MaterialComboBox.getDisplayString()).
    [[nodiscard]] static std::string displayString(const Material& material);

protected:
    bool readState() override;

private:
    [[nodiscard]] std::vector<Material> readMaterials() const;
    void                                databaseChanged();

    using DatabaseConnection = Signal<const Material&, const MaterialDatabase&>::ScopedConnection;

    MaterialSource                    m_source;
    Material::Type                    m_type;
    MaterialDatabase*                 m_applicationDatabase;
    MaterialDatabase*                 m_documentDatabase;
    std::vector<Material>             m_materials;
    Material                          m_value;
    bool                              m_enabled{true};
    std::array<DatabaseConnection, 4> m_databaseConnections;
};

}  // namespace QtRocket::Gui
