#pragma once

#include <memory>
#include <vector>

#include <QComboBox>
#include <QObject>
#include <QString>

#include "QtRocket/gui/widgets/GroupedSearchPopup.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/util/Signal.h"

class QLineEdit;
class QListWidget;
class QMenu;
class QStyleOptionComboBox;
class QWheelEvent;
class QWidget;

namespace QtRocket::Gui
{

class MaterialBinding;

/// The combo box of a material (OpenRocket: MaterialComboBox.createComboBox(), a
/// GroupableAndSearchableComboBox on a MaterialModel, swing gui/widgets/): a component's material,
/// a fin set's fillet material, a parachute's line material.
///
/// Closed, it is a combo box of its binding's materials (MaterialBinding::materials(), the
/// application's then the document's), each shown as MaterialBinding::displayString() ("(ud) "
/// for a user-defined material, then the name and the density in the current default unit); the
/// current item is the binding's value, and a material that is none of them (one a file brought
/// along) is shown all the same with no item current (shownText()). The keys and the wheel choose
/// from the items as in EnumComboBox.
///
/// Its popup (showPopup(): a click, F4, Alt+Down) is the grouped menu of Java's
/// GroupableAndSearchableComboBox (widgets/GroupedSearchPopup): a search field (placeholder "Enter
/// the material name", MaterialPanel.MaterialComboBox.placeholder), a separator, and one submenu
/// per material group that has materials (MaterialBinding::groups(): by group priority, each
/// group's materials in Material order), the group's display name as its title (util/EnumText).
/// The submenu of the value's group is checked, and so is the value's item (Java draws a check
/// mark on both). Typing in the search field (searchField()) switches the popup to the search
/// (isSearching()): the submenus make way for a list (searchList()) of the materials whose text
/// contains the typed text ignoring case (Java: toLowerCase().contains()), sorted by
/// Material::toString() ignoring case (Java: a TreeSet of String.compareToIgnoreCase()), the
/// value's row after a check mark (U+2713 and a space, Java's renderer); the first row is
/// highlighted. Down and Up move the highlight, Enter takes the highlighted material, Esc closes
/// the popup; the mouse highlights and takes a row too; an empty search field brings the submenus
/// back. Choosing a material closes the popup and sets it (MaterialBinding::select(): on every
/// selected component); afterwards, whatever came of it, the combo box shows the binding's state
/// again. It also shows it whenever the binding tells of a change (a material added to or removed
/// from a database: the items and an open popup are rebuilt).
///
/// Enabled: the binding's isEnabled() AND setFieldEnabled() (FieldEnabling); once the binding's
/// selection is invalid the combo box keeps the last material, is disabled and closes its popup.
///
/// Deviations from OpenRocket (and those of GroupedSearchPopup):
/// - A check mark goes to the first material that equals the value (Material's equality), where
///   Java compares identity; materials whose texts are equal ignoring case are all listed in the
///   search (Java's TreeSet keeps one of them, whichever its HashSet of the items gives first).
/// - The keys and the wheel on the closed combo box are EnumComboBox's (Java's Down opens the
///   plain list popup of the items).
/// - Not shown: the buttons "Add custom material" and "Edit materials" (the custom material
///   dialog and the material editor, Milestone 5).
class MaterialComboBox : public QComboBox
{
    Q_OBJECT

public:
    /// A combo box of @p binding.
    /// @throws BugError when @p binding is null
    MaterialComboBox(std::shared_ptr<MaterialBinding> binding, QWidget* parent);
    ~MaterialComboBox() override;
    MaterialComboBox(const MaterialComboBox&)            = delete;
    MaterialComboBox& operator=(const MaterialComboBox&) = delete;
    MaterialComboBox(MaterialComboBox&&)                 = delete;
    MaterialComboBox& operator=(MaterialComboBox&&)      = delete;

    [[nodiscard]] const std::shared_ptr<MaterialBinding>& binding() const noexcept
    {
        return m_binding;
    }

    /// The text the combo box shows: the current item's, or that of a material no item holds.
    [[nodiscard]] QString shownText() const;

    /// Whether the combo box may be used as far as the dialog is concerned (FieldEnabling).
    [[nodiscard]] bool isFieldEnabled() const noexcept { return m_fieldEnabled; }
    void               setFieldEnabled(bool enabled);

    /// Shows the binding's state again, the texts read anew: after the default units changed
    /// (a material's text holds its density in the default unit; a unit change fires nothing).
    void refresh();

    /// Opens the grouped popup below the combo box (see the class comment).
    void showPopup() override;
    /// Closes the popup.
    void hidePopup() override;

    /// The popup.
    [[nodiscard]] GroupedSearchPopup& popupMenu() const noexcept { return *m_popup; }
    /// The popup's search field.
    [[nodiscard]] QLineEdit& searchField() const noexcept { return m_popup->searchField(); }
    /// The popup's search list (shown while searching).
    [[nodiscard]] QListWidget& searchList() const noexcept { return m_popup->searchList(); }
    /// The submenu of @p group (shown while not searching, for a group with materials).
    [[nodiscard]] QMenu& groupMenu(MaterialGroup group) const;
    /// Whether the popup shows the search list (the search field holds text).
    [[nodiscard]] bool isSearching() const noexcept { return m_popup->isSearching(); }
    /// The index into MaterialBinding::materials() of each row of the search list.
    [[nodiscard]] const std::vector<int>& searchRows() const noexcept
    {
        return m_popup->searchRows();
    }

protected:
    void initStyleOption(QStyleOptionComboBox* option) const override;
    void wheelEvent(QWheelEvent* event) override;

private:
    /// The popup, made once: a submenu per material group, its content the binding's materials.
    [[nodiscard]] GroupedSearchPopup* makePopup();
    /// Takes the material at @p index of the binding's materials: closes the popup and sets it.
    void choose(int index);
    /// A choice of the user's on the closed combo box.
    void activatedByUser(int index);
    /// The combo box's text of the material at @p index.
    [[nodiscard]] QString textAt(int index) const;

    std::shared_ptr<MaterialBinding> m_binding;
    Signal<>::ScopedConnection       m_connection;
    GroupedSearchPopup*              m_popup{nullptr};
    bool                             m_fieldEnabled{true};
    /// The text of the binding's material while no item holds it, else empty.
    QString m_apartText;
};

}  // namespace QtRocket::Gui
