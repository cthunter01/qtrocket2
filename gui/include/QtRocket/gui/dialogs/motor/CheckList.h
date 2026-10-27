#pragma once

#include <vector>

#include <QAbstractItemView>
#include <QListWidget>
#include <QObject>
#include <QSize>
#include <QStringList>
#include <QtContainerFwd>

class QEvent;
class QKeyEvent;
class QModelIndex;
class QMouseEvent;
class QWidget;

namespace QtRocket::Gui
{

/// A list whose items each carry a check box (OpenRocket's CheckList with its
/// DefaultCheckListModel, CheckListEditor and CheckListRenderer, swing gui/util/CheckList*.java and
/// DefaultCheckListModel.java, EzWare's decorator of a JList), as the motor chooser's filter panel
/// lists the manufacturers in it: an item is a text, checked or not, and the list knows its items
/// by their index. Single selection.
///
/// What checks and unchecks an item, as in Java:
/// - a click of the left mouse button anywhere on the item, its text included (CheckListEditor's
///   mouseClicked(): the press and the release on the same item); the second click of a double
///   click does nothing, so a double click toggles once;
/// - Space (without modifiers) toggles the selected item (the "toggle-check" action);
/// - the functions below.
/// Every one of them emits checksChanged() (Java's contentsChanged of the model), also when it
/// changes nothing: clearAll() of a list without checks, setChecked() to the state an item has.
///
/// The items are a QListWidget's, user-checkable, their check state the check box the style
/// draws; Qt's own toggling of a click on the check box and of Space is replaced by Java's above
/// (QAbstractItemView::edit() does not hand the list's mouse and key events to the delegate). A
/// check state that something else sets on an item (the accessibility's toggle action) also
/// emits checksChanged().
///
/// Deviation from OpenRocket: a click toggles unless it is the second one of a double click; the
/// third click of a triple click toggles again (Swing's click count is 3 there, and CheckListEditor
/// toggles for a count of 1 only). Not ported: Java's drop location highlight (the list takes no
/// drops), setModel() and the generic item type (the items are texts; the caller keeps what they
/// stand for by index).
class CheckList final : public QListWidget
{
    Q_OBJECT

public:
    /// An empty list (Java: new CheckList.Builder().build()).
    explicit CheckList(QWidget* parent);
    ~CheckList() override;
    CheckList(const CheckList&)            = delete;
    CheckList& operator=(const CheckList&) = delete;
    CheckList(CheckList&&)                 = delete;
    CheckList& operator=(CheckList&&)      = delete;

    /// Replaces the items by @p texts, in that order, none checked (setData(), which makes a new
    /// DefaultCheckListModel; the new model fires nothing, so neither does this).
    void setItems(const QStringList& texts);

    /// Whether the item @p index is checked (isCheckedIndex()).
    /// @throws BugError when there is no such item
    [[nodiscard]] bool isChecked(int index) const;

    /// Checks or unchecks the item @p index (setCheckedIndex()).
    /// @throws BugError when there is no such item
    void setChecked(int index, bool checked);

    /// Toggles the item @p index (toggleIndex()); nothing for an index that is no item's (-1:
    /// no selection).
    void toggleIndex(int index);

    /// Checks every item (checkAll()).
    void checkAll();

    /// Unchecks every item (clearAll()).
    void clearAll();

    /// Checks exactly the items @p indices (setCheckedItems()); an index that is no item's is
    /// left out.
    void setCheckedIndices(const std::vector<int>& indices);

    /// Checks every item but @p indices (setUncheckedItems()); an index that is no item's is left
    /// out.
    void setUncheckedIndices(const std::vector<int>& indices);

    /// The checked items, in the list's order (getCheckedItems()).
    [[nodiscard]] std::vector<int> checkedIndices() const;

    /// The unchecked items (getUncheckedItems()), in Java's order: from the last item to the
    /// first.
    [[nodiscard]] std::vector<int> uncheckedIndices() const;

    /// The selected item (JList.getSelectedIndex()), or -1.
    [[nodiscard]] int selectedIndex() const;

    /// The list's preferred and minimum sizes from now on (Java's setPreferredSize() and
    /// setMinimumSize() of the scroll pane around the list, which in Qt is the list itself);
    /// Qt's own hints until then.
    void setSizeHints(const QSize& preferred, const QSize& minimum);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    /// The checks changed, or one of the functions above ran (see the class comment).
    void checksChanged();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    /// Hands no mouse or key event to the delegate (see the class comment); the rest as Qt does.
    bool edit(const QModelIndex& index, QAbstractItemView::EditTrigger trigger,
              QEvent* event) override;

private:
    /// A BugError unless @p index is an item's.
    void checkIndex(int index) const;
    /// Sets the check state of the item @p index without telling anyone.
    void setCheckState(int index, bool checked);
    /// An item's check state changed: from outside, unless the list is changing it itself.
    void itemCheckChanged();

    /// Whether the list is setting check states itself (no checksChanged() per item).
    bool m_updating = false;
    /// The item the last press of the left button was on, or -1.
    int m_pressedIndex = -1;
    /// Whether that press was the second one of a double click.
    bool m_doubleClick = false;
    /// The sizes of setSizeHints(); invalid until then.
    QSize m_preferredSize;
    QSize m_minimumSize;
};

}  // namespace QtRocket::Gui
