#include "QtRocket/gui/widgets/GroupedSearchPopup.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QAbstractItemView>
#include <QAction>
#include <QChar>
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QPoint>
#include <QScopedValueRollback>
#include <QSize>
#include <QString>
#include <QWidget>
#include <QWidgetAction>
#include <Qt>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/MenuText.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket::Gui
{

namespace
{

/// @p content, every function of which must be set.
[[nodiscard]] GroupedSearchPopup::Content checkedContent(GroupedSearchPopup::Content content)
{
    if (!content.itemCount || !content.text || !content.sortKey || !content.groups ||
        !content.current || !content.currentGroup || !content.choose)
    {
        bug("A grouped search popup needs every function of its content");
    }
    return content;
}

/// The search field of the popup: Esc, Enter, Up and Down are the popup's (Java:
/// GroupableAndSearchableComboBox.SearchFieldKeyAdapter.overrideActionKeys(), which consumes
/// them), every other key edits the text.
class SearchField final : public QLineEdit
{
public:
    SearchField(std::function<bool(int key)> popupKey, QWidget* parent)
      : QLineEdit(parent), m_popupKey(std::move(popupKey))
    {
    }

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        if ((event->modifiers() == Qt::NoModifier || event->modifiers() == Qt::KeypadModifier) &&
            m_popupKey(event->key()))
        {
            event->accept();
            return;
        }
        QLineEdit::keyPressEvent(event);
    }

private:
    std::function<bool(int key)> m_popupKey;
};

}  // namespace

GroupedSearchPopup::GroupedSearchPopup(const QString&              placeholder,
                                       const std::vector<QString>& groupTitles, Content content,
                                       QWidget* parent)
  : QMenu(parent),
    m_content(checkedContent(std::move(content))),
    m_searchField(makeChild<SearchField>(this, [this](int key) { return searchKey(key); })),
    m_searchList(makeChild<QListWidget>(this)),
    m_searchListAction(makeChild<QWidgetAction>(this))
{
    m_searchField->setPlaceholderText(placeholder);
    auto* searchAction = makeChild<QWidgetAction>(this);
    searchAction->setDefaultWidget(m_searchField);
    addAction(searchAction);
    connect(m_searchField, &QLineEdit::textEdited, this,
            [this](const QString& text) { Guard::run([this, &text] { search(text); }); });
    addSeparator();

    m_groupMenus.reserve(groupTitles.size());
    for (const QString& title : groupTitles)
    {
        // Shown as it is (Java's JMenu): a menu would take an '&' for a shortcut's mark.
        m_groupMenus.push_back(addMenu(MenuText::escaped(title)));
        m_groupMenus.back()->menuAction()->setCheckable(true);
    }

    m_searchList->setMouseTracking(true);
    m_searchList->setFocusPolicy(Qt::NoFocus);  // the search field keeps the keyboard
    m_searchList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_searchList, &QListWidget::itemEntered, this,
            [this](QListWidgetItem* item) { m_searchList->setCurrentItem(item); });
    connect(m_searchList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        const int row = m_searchList->row(item);
        if (row >= 0 && static_cast<std::size_t>(row) < m_searchRows.size())
        {
            take(m_searchRows.at(static_cast<std::size_t>(row)));
        }
    });
    m_searchListAction->setDefaultWidget(m_searchList);
    addAction(m_searchListAction);
    m_searchListAction->setVisible(false);
    m_searchList->setVisible(false);
}

GroupedSearchPopup::~GroupedSearchPopup() = default;

QMenu& GroupedSearchPopup::groupMenu(std::size_t group) const
{
    if (group >= m_groupMenus.size())
    {
        bug("No such group in the grouped search popup");
    }
    return *m_groupMenus.at(group);
}

void GroupedSearchPopup::popUpBelow(const QWidget& anchor)
{
    m_searchField->clear();
    search(QString());
    fillGroups();
    popup(anchor.mapToGlobal(QPoint(0, anchor.height())));
    m_searchField->setFocus(Qt::PopupFocusReason);
}

void GroupedSearchPopup::refill()
{
    if (!isVisible())
    {
        return;
    }
    fillGroups();
    if (m_searching)
    {
        fillSearchList();
    }
}

void GroupedSearchPopup::fillGroups()
{
    const int current = m_content.current();
    for (QMenu* menu : m_groupMenus)
    {
        menu->clear();  // deletes the items it made
        menu->menuAction()->setVisible(false);
    }
    for (const Group& group : m_content.groups())
    {
        QMenu& menu = groupMenu(group.group);
        for (const int item : group.items)
        {
            // Shown as it is (Java's JCheckBoxMenuItem), as in the search list: a menu would take
            // an '&' (a material's or a custom data type's name) for a shortcut's mark.
            QAction* action = menu.addAction(MenuText::escaped(m_content.text(item)));
            action->setCheckable(true);
            action->setChecked(item == current);
            connect(action, &QAction::triggered, this, [this, item] { take(item); });
        }
        menu.menuAction()->setVisible(!m_searching);
    }
    // Java: the group of the value is checked, also when the value is none of the items.
    const std::optional<std::size_t> checked = m_content.currentGroup();
    for (std::size_t index = 0; index < m_groupMenus.size(); ++index)
    {
        m_groupMenus.at(index)->menuAction()->setChecked(checked == index);
    }
}

void GroupedSearchPopup::fillSearchList()
{
    const int     count  = m_content.itemCount();
    const QString needle = m_searchField->text().toLower();
    m_searchRows.clear();
    for (int item = 0; item < count; ++item)
    {
        if (m_content.text(item).toLower().contains(needle))
        {
            m_searchRows.push_back(item);
        }
    }
    // Java's SortedListModel: the items' toString() by String.compareToIgnoreCase().
    std::vector<std::string> keys;
    keys.reserve(static_cast<std::size_t>(std::max(count, 0)));
    for (int item = 0; item < count; ++item)
    {
        keys.push_back(Strings::javaCaseFold(m_content.sortKey(item)));
    }
    std::ranges::stable_sort(m_searchRows, [&keys](int left, int right) {
        return Strings::javaCompareTo(keys.at(static_cast<std::size_t>(left)),
                                      keys.at(static_cast<std::size_t>(right))) < 0;
    });

    const int current = m_content.current();
    m_searchList->clear();
    for (const int item : m_searchRows)
    {
        QString text = m_content.text(item);
        if (item == current)
        {
            text = QString(QChar(0x2713)) + QStringLiteral(" ") + text;
        }
        m_searchList->addItem(text);
    }
    if (m_searchList->count() > 0)
    {
        m_searchList->setCurrentRow(0);  // Java: highlightedListIdx = 0 after every key
    }
}

void GroupedSearchPopup::search(const QString& text)
{
    m_searching = !text.isEmpty();
    for (const QMenu* menu : m_groupMenus)
    {
        menu->menuAction()->setVisible(!m_searching && !menu->isEmpty());
    }
    if (m_searching)
    {
        // Java: the search popup takes the groups popup's size.
        const QSize size = sizeHint();
        m_searchList->setFixedWidth(std::max(size.width(), m_searchField->sizeHint().width()));
        fillSearchList();
    }
    m_searchListAction->setVisible(m_searching);
    // A widget action's visibility does not reach its widget in a menu.
    m_searchList->setVisible(m_searching);
    if (isVisible())
    {
        resize(sizeHint());
    }
}

bool GroupedSearchPopup::searchKey(int key)
{
    switch (key)
    {
        case Qt::Key_Escape:
            hide();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        {
            const int row = m_searchList->currentRow();
            if (m_searching && row >= 0 && static_cast<std::size_t>(row) < m_searchRows.size())
            {
                take(m_searchRows.at(static_cast<std::size_t>(row)));
            }
            return true;
        }
        case Qt::Key_Down:
            if (m_searching && m_searchList->currentRow() + 1 < m_searchList->count())
            {
                m_searchList->setCurrentRow(m_searchList->currentRow() + 1);
            }
            return true;
        case Qt::Key_Up:
            if (m_searching && m_searchList->currentRow() > 0)
            {
                m_searchList->setCurrentRow(m_searchList->currentRow() - 1);
            }
            return true;
        default:
            return false;
    }
}

void GroupedSearchPopup::take(int item)
{
    hide();  // Java: setSelectedItem() hides the popups first
    // The choice may destroy the popup (with its owner's dialog): nothing of it is used after.
    const QWidget* const owner   = parentWidget();
    const QString        context = owner != nullptr ? owner->objectName() : QString();
    const auto           choose  = m_content.choose;
    Guard::run([&choose, item] { choose(item); }, context);
}

bool GroupedSearchPopup::event(QEvent* event)
{
    const auto* const key = dynamic_cast<const QKeyEvent*>(event);
    if (key == nullptr || event->type() != QEvent::KeyPress || m_forwardingKey)
    {
        return QMenu::event(event);
    }
    const bool typed =
        key->key() == Qt::Key_Backspace || (!key->text().isEmpty() && key->text().at(0).isPrint());
    if (!typed)
    {
        return QMenu::event(event);
    }
    // A key the search field leaves goes on to the popup again: not back to the field.
    const QScopedValueRollback<bool> forwarding(m_forwardingKey, true);
    m_searchField->setFocus(Qt::PopupFocusReason);
    QCoreApplication::sendEvent(m_searchField, event);
    return true;
}

}  // namespace QtRocket::Gui
