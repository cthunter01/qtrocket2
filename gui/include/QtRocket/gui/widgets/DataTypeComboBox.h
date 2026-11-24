#pragma once

#include <vector>

#include <QComboBox>
#include <QObject>
#include <QString>

#include "QtRocket/gui/widgets/GroupedSearchPopup.h"
#include "QtRocket/simulation/FlightDataTypeGroup.h"

class QLineEdit;
class QListWidget;
class QMenu;
class QStyleOptionComboBox;
class QWheelEvent;
class QWidget;

namespace QtRocket
{
class FlightDataType;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The combo box of a flight data type (OpenRocket's GroupableAndSearchableComboBox over
/// FlightDataTypes, swing gui/widgets/, as PlotTypeSelector and PlotPanel make it: the X axis type
/// and each Y axis type of the plot tab), grouped by FlightDataTypeGroup.
///
/// Closed, it is a combo box of the types it was made with (types()), in the order of Java's model
/// (extractItemsFromMap(): a HashSet of them, which keeps the first of equal types and iterates in
/// the order of their hash codes' buckets), each shown by its name (util/EnumText's
/// flightDataTypeText(); Java: getDisplayString(), toString()). The current item is the selected
/// type (selectedType(); Java: getSelectedItem()), the item that equals it (FlightDataType::
/// equals(): the names ignoring case); a type that is none of them (one that setSelectedType()
/// gave) is shown all the same with no item current (shownText()), as is the selected type's own
/// name when an item only equals it. The width is fixed at kFixedWidth (Java:
/// PlotTypeSelector.DATA_TYPE_SELECTOR_WIDTH, "200lp!": a long name must not widen the row); a
/// name too long for it is elided (Java's renderer, a JLabel, ends it with "..."), and the tooltip
/// is the selected type's whole name (Java: updateTypeSelectorTooltip()). The keys and the wheel
/// choose from the items as in EnumComboBox.
///
/// Its popup (showPopup(): a click, F4, Alt+Down) is GroupedSearchPopup: the search field
/// (placeholder "Enter the data type", FlightDataComboBox.placeholder), then one submenu per
/// group that has types, by group priority (Java: a TreeMap by Group.getPriority()), each
/// group's types sorted by FlightDataType::compareTo() (the priority within the group) in a stable
/// sort of the model's order (Java's contentsChanged() groups them again from its model once a
/// type is selected, which PlotTypeSelector and PlotPanel do at once), the group's name as its
/// title (util/EnumText); the submenu of the selected type's group is checked, and so is the
/// selected type's item (Java: identity). The search lists the types whose name contains the
/// typed text ignoring case, sorted by name ignoring case.
///
/// Choosing a type (in the popup, or on the closed combo box) selects it: setSelectedType(),
/// which closes the popup (Java's setSelectedItem() hides the popups) and, when the selection
/// changed (Java's DefaultComboBoxModel: the new type does not equal the old one), tells of it
/// with selectedTypeChanged() (Java's ItemEvent SELECTED, which PlotTypeSelector and PlotPanel
/// listen to). A type equal to the selected one changes nothing.
///
/// Deviations from OpenRocket (and those of GroupedSearchPopup):
/// - The keys and the wheel are EnumComboBox's (Java's Down opens the plain list of the model).
/// - The types that compare equal (TYPE_THRUST_FORCE and TYPE_THRUST_CORRECTION, every custom type
///   of one priority) are in the model's order in their submenu from the start; Java's are in the
///   order given until a type is first selected.
/// - The model's order is that of a HashSet whose buckets hold lists: Java's HashMap turns a bucket
///   of 8 keys or more into a tree, iterated in another order (8 names whose hash codes agree in
///   their 7 lowest bits after HashMap.hash()). A name with one of the letters FlightDataType::
///   hashCode() folds otherwise than Java (the micro sign, ...) may land in another bucket.
/// - Selecting no type (null) clears the tooltip; Java's keeps the last one (it is set on an
///   ItemEvent SELECTED, which a null selection does not fire).
class DataTypeComboBox final : public QComboBox
{
    Q_OBJECT

public:
    /// PlotTypeSelector.DATA_TYPE_SELECTOR_WIDTH ("200lp!"): the fixed width, px.
    static constexpr int kFixedWidth = 200;

    /// A combo box of @p types, none selected.
    /// @throws BugError when a type of @p types is null
    DataTypeComboBox(const std::vector<const FlightDataType*>& types, QWidget* parent);
    ~DataTypeComboBox() override;
    DataTypeComboBox(const DataTypeComboBox&)            = delete;
    DataTypeComboBox& operator=(const DataTypeComboBox&) = delete;
    DataTypeComboBox(DataTypeComboBox&&)                 = delete;
    DataTypeComboBox& operator=(DataTypeComboBox&&)      = delete;

    /// The types of the items, in their order.
    [[nodiscard]] const std::vector<const FlightDataType*>& types() const noexcept
    {
        return m_types;
    }

    /// The selected type, or null.
    [[nodiscard]] const FlightDataType* selectedType() const noexcept { return m_selected; }
    /// Selects @p type (null: none), also one that is none of the items (Java: the model takes
    /// any object); see the class comment.
    void setSelectedType(const FlightDataType* type);

    /// The text the combo box shows, before eliding: the selected type's name, empty for none.
    [[nodiscard]] QString shownText() const;

    /// Opens the grouped popup below the combo box.
    void showPopup() override;
    /// Closes the popup.
    void hidePopup() override;

    /// The popup.
    [[nodiscard]] GroupedSearchPopup& popupMenu() const noexcept { return *m_popup; }
    /// The popup's search field.
    [[nodiscard]] QLineEdit& searchField() const noexcept { return m_popup->searchField(); }
    /// The popup's search list (shown while searching).
    [[nodiscard]] QListWidget& searchList() const noexcept { return m_popup->searchList(); }
    /// The submenu of @p group (shown while not searching, for a group with types).
    [[nodiscard]] QMenu& groupMenu(FlightDataTypeGroup group) const;
    /// Whether the popup shows the search list (the search field holds text).
    [[nodiscard]] bool isSearching() const noexcept { return m_popup->isSearching(); }
    /// The index into types() of each row of the search list.
    [[nodiscard]] const std::vector<int>& searchRows() const noexcept
    {
        return m_popup->searchRows();
    }

Q_SIGNALS:
    /// The selected type changed: selectedType() is another one (or none) now.
    void selectedTypeChanged();

protected:
    void initStyleOption(QStyleOptionComboBox* option) const override;
    void wheelEvent(QWheelEvent* event) override;

private:
    /// The popup, made once: a submenu per group, its content the types.
    [[nodiscard]] GroupedSearchPopup* makePopup();
    /// The groups of the popup: Java's constructItemGroupMapFromList().
    [[nodiscard]] std::vector<GroupedSearchPopup::Group> groups() const;
    /// The index of the item equal to the selected type, -1 for none.
    [[nodiscard]] int selectedIndex() const;
    /// Shows the selected type: the current item, the tooltip.
    void showSelection();
    /// Selects the type of the item @p index (a choice of the user's).
    void choose(int index);

    std::vector<const FlightDataType*> m_types;
    const FlightDataType*              m_selected{nullptr};
    GroupedSearchPopup*                m_popup{nullptr};
};

}  // namespace QtRocket::Gui
