#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <QMenu>
#include <QObject>
#include <QString>

class QEvent;
class QLineEdit;
class QListWidget;
class QWidget;
class QWidgetAction;

namespace QtRocket::Gui
{

/// The popup of a grouped, searchable combo box (OpenRocket's GroupableAndSearchableComboBox,
/// swing gui/widgets/: its groups popup and its search popup), which MaterialComboBox and
/// DataTypeComboBox show below themselves: a search field (the placeholder given), a separator,
/// one submenu per group that has items, and the search list.
///
/// The items are its owner's, numbered from 0 to Content::itemCount() - 1, and read through
/// Content each time the popup fills (popUpBelow(), refill()). The submenus are made once, one
/// for each group title given, in that order, each with a checkable menu action; a fill hides
/// those of the groups Content::groups() leaves out and gives the others their items in the
/// order given, each checkable and checked when it is Content::current(), and checks the submenu
/// of Content::currentGroup() (Java draws a check mark on that group, which is the value's group
/// also when the value is none of the items).
///
/// Typing in the search field (searchField()) switches the popup to the search (isSearching()):
/// the submenus make way for a list (searchList()) of the items whose text contains the typed text
/// ignoring case (Java's filter(): toLowerCase().contains()), sorted by their Content::sortKey()
/// ignoring case (Java's SortedListModel: a TreeSet of String.compareToIgnoreCase() of the items'
/// toString()), the current item's row after a check mark (U+2713 and a space, Java's renderer);
/// the first row is highlighted. Down and Up move the highlight, Enter takes the highlighted item,
/// Esc closes the popup; the mouse highlights and takes a row too; an empty search field brings
/// the submenus back. A key typed while the popup itself has the keyboard (the mouse over a
/// submenu takes it) goes to the search field. Taking an item (a submenu's item, a row, Enter)
/// closes the popup and hands the item to Content::choose(), through Guard (the context is the
/// owner's object name).
///
/// Deviations from OpenRocket:
/// - One popup whose content switches between the submenus and the search list (Java swaps two
///   popups of the same size and copies the typed text from one search field into the other);
///   the search field has the focus when the popup opens.
/// - Items whose sort keys are equal ignoring case are all listed in the search (Java's TreeSet
///   keeps one of them).
/// - The search list does not underline the typed text in the rows (Java's HTML renderer).
/// - Not ported: the extra widgets below the groups (Java's extraGroupPopupWidgets: the material
///   combo box's "Add custom material" and "Edit materials" buttons, Milestone 5), and Java's
///   delayed hover handling of the submenus (DeselectMenuListener: Qt's menus open and close their
///   submenus themselves).
class GroupedSearchPopup final : public QMenu
{
    Q_OBJECT

public:
    /// The items of one group, as Content::groups() gives them.
    struct Group
    {
        /// The group: the index of its title among the titles the popup was made with.
        std::size_t group{0};
        /// Its items, in the order of its submenu.
        std::vector<int> items;
    };

    /// The owner's items and what becomes of a choice. Every function must be set.
    struct Content
    {
        /// The number of items.
        std::function<int()> itemCount;
        /// The text of an item, in the submenus and the search list.
        std::function<QString(int item)> text;
        /// The key the search list is sorted by, UTF-8, compared ignoring case as Java's
        /// String.compareToIgnoreCase() (Strings::javaCaseFold(), then Strings::javaCompareTo()).
        std::function<std::string(int item)> sortKey;
        /// The groups that have items, in the order of the popup, each with its items.
        std::function<std::vector<Group>()> groups;
        /// The current item, checked in the submenus and the search list; -1 for none.
        std::function<int()> current;
        /// The group whose submenu is checked; nullopt for none.
        std::function<std::optional<std::size_t>()> currentGroup;
        /// Takes the item the user chose; the popup is closed by then. It may destroy the popup.
        std::function<void(int item)> choose;
    };

    /// A popup with the search field's placeholder @p placeholder and a submenu titled with each
    /// of @p groupTitles, over @p content; @p parent is the combo box it belongs to.
    /// @throws BugError when a function of @p content is not set
    GroupedSearchPopup(const QString& placeholder, const std::vector<QString>& groupTitles,
                       Content content, QWidget* parent);
    ~GroupedSearchPopup() override;
    GroupedSearchPopup(const GroupedSearchPopup&)            = delete;
    GroupedSearchPopup& operator=(const GroupedSearchPopup&) = delete;
    GroupedSearchPopup(GroupedSearchPopup&&)                 = delete;
    GroupedSearchPopup& operator=(GroupedSearchPopup&&)      = delete;

    /// Opens the popup below @p anchor (Java: show(comboBox, 0, height)) showing the submenus, with
    /// an empty search field, which takes the keyboard focus.
    void popUpBelow(const QWidget& anchor);
    /// Fills an open popup again from its content: its submenus and, while searching, its search
    /// list (the owner's items or value changed). Nothing while it is closed.
    void refill();

    /// The search field.
    [[nodiscard]] QLineEdit& searchField() const noexcept { return *m_searchField; }
    /// The search list (shown while searching).
    [[nodiscard]] QListWidget& searchList() const noexcept { return *m_searchList; }
    /// The submenu of the group @p group (an index of the titles the popup was made with).
    /// @throws BugError when there is no such group
    [[nodiscard]] QMenu& groupMenu(std::size_t group) const;
    /// Whether the popup shows the search list (the search field holds text).
    [[nodiscard]] bool isSearching() const noexcept { return m_searching; }
    /// The item of each row of the search list.
    [[nodiscard]] const std::vector<int>& searchRows() const noexcept { return m_searchRows; }

protected:
    /// A key typed while the popup has the keyboard goes to the search field (see the class
    /// comment).
    bool event(QEvent* event) override;

private:
    /// The submenus' items and check marks, from the content.
    void fillGroups();
    /// What the search field's @p text shows: the submenus (empty) or the search list.
    void search(const QString& text);
    /// The search list for the search field's text, the first row highlighted.
    void fillSearchList();
    /// A key of the search field: whether it was one of the popup's (Esc, Enter, Up, Down).
    bool searchKey(int key);
    /// Takes @p item: closes the popup and hands it to the content.
    void take(int item);

    // The state comes first: the children below are made in the constructor's member
    // initializers, and each sends this popup an event (QEvent::ChildAdded) that event() sees.
    Content m_content;
    bool    m_searching{false};
    /// Set while event() hands a key to the search field.
    bool                m_forwardingKey{false};
    std::vector<QMenu*> m_groupMenus;
    std::vector<int>    m_searchRows;
    QLineEdit*          m_searchField{nullptr};
    QListWidget*        m_searchList{nullptr};
    QWidgetAction*      m_searchListAction{nullptr};
};

}  // namespace QtRocket::Gui
