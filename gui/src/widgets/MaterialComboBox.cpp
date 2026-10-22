#include "QtRocket/gui/widgets/MaterialComboBox.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <QAbstractItemView>
#include <QAction>
#include <QChar>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QPoint>
#include <QPointer>
#include <QScopedValueRollback>
#include <QSize>
#include <QString>
#include <QStyleOptionComboBox>
#include <QWheelEvent>
#include <QWidget>
#include <QWidgetAction>
#include <Qt>

#include "QtRocket/gui/binding/MaterialBinding.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket::Gui
{

namespace
{

/// @p binding, which must not be null.
[[nodiscard]] std::shared_ptr<MaterialBinding> checkedBinding(
    std::shared_ptr<MaterialBinding> binding)
{
    if (binding == nullptr)
    {
        bug("A material combo box needs a binding");
    }
    return binding;
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

/// The index of @p group in kAllMaterialGroups.
[[nodiscard]] std::size_t groupIndex(MaterialGroup group)
{
    for (std::size_t index = 0; index < kAllMaterialGroups.size(); ++index)
    {
        if (kAllMaterialGroups.at(index) == group)
        {
            return index;
        }
    }
    QTROCKET_UNREACHABLE();
}

}  // namespace

MaterialComboBox::MaterialComboBox(std::shared_ptr<MaterialBinding> binding, QWidget* parent)
  : QComboBox(parent), m_binding(checkedBinding(std::move(binding)))
{
    // Not Qt::WheelFocus (QComboBox's): the wheel must not take the focus.
    setFocusPolicy(Qt::StrongFocus);
    buildPopup();
    m_connection =
        m_binding->addChangeListener([this] { Guard::run([this] { refresh(); }, objectName()); });
    connect(this, &QComboBox::activated, this, [this](int index) { activatedByUser(index); });
    refresh();
}

MaterialComboBox::~MaterialComboBox()
{
    // The popup goes after this object's own part: it must not reach eventFilter() then.
    m_popup->removeEventFilter(this);
}

void MaterialComboBox::buildPopup()
{
    m_popup = makeChild<QMenu>(this);
    m_popup->installEventFilter(this);

    m_searchField = makeChild<SearchField>(m_popup, [this](int key) { return searchKey(key); });
    //: MaterialPanel.MaterialComboBox.placeholder
    m_searchField->setPlaceholderText(tr("Enter the material name"));
    auto* searchAction = makeChild<QWidgetAction>(m_popup);
    searchAction->setDefaultWidget(m_searchField);
    m_popup->addAction(searchAction);
    connect(m_searchField, &QLineEdit::textEdited, this,
            [this](const QString& text) { Guard::run([this, &text] { search(text); }); });
    m_popup->addSeparator();

    for (std::size_t index = 0; index < kAllMaterialGroups.size(); ++index)
    {
        QMenu* menu = m_popup->addMenu(enumText(kAllMaterialGroups.at(index)));
        menu->menuAction()->setCheckable(true);
        m_groupMenus.at(index) = menu;
    }

    m_searchList = makeChild<QListWidget>(m_popup);
    m_searchList->setMouseTracking(true);
    m_searchList->setFocusPolicy(Qt::NoFocus);  // the search field keeps the keyboard
    m_searchList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_searchList, &QListWidget::itemEntered, this,
            [this](QListWidgetItem* item) { m_searchList->setCurrentItem(item); });
    connect(m_searchList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        const int row = m_searchList->row(item);
        if (row >= 0 && static_cast<std::size_t>(row) < m_searchRows.size())
        {
            choose(m_searchRows.at(static_cast<std::size_t>(row)));
        }
    });
    m_searchListAction = makeChild<QWidgetAction>(m_popup);
    m_searchListAction->setDefaultWidget(m_searchList);
    m_popup->addAction(m_searchListAction);
    m_searchListAction->setVisible(false);
    m_searchList->setVisible(false);
}

QMenu& MaterialComboBox::groupMenu(MaterialGroup group) const
{
    return *m_groupMenus.at(groupIndex(group));
}

QString MaterialComboBox::textAt(int index) const
{
    const Material* material = m_binding->materialAt(index);
    QTROCKET_ASSERT(material != nullptr);
    return QtStrings::fromUtf8(MaterialBinding::displayString(*material));
}

void MaterialComboBox::refresh()
{
    const MaterialBinding& binding = *m_binding;
    QList<QString>         texts;
    texts.reserve(binding.size());
    for (int index = 0; index < binding.size(); ++index)
    {
        texts.append(textAt(index));
    }
    QList<QString> shown;
    shown.reserve(count());
    for (int index = 0; index < count(); ++index)
    {
        shown.append(itemText(index));
    }
    if (texts != shown)
    {
        clear();
        addItems(texts);
    }
    const int current = binding.indexOf(binding.value());
    setCurrentIndex(current);
    m_apartText = current < 0 ? QtStrings::fromUtf8(MaterialBinding::displayString(binding.value()))
                              : QString();
    setEnabled(m_fieldEnabled && binding.isEnabled());
    update();
    if (!binding.isValid())
    {
        hidePopup();
    }
    else if (m_popup->isVisible())
    {
        fillGroups();
        if (m_searching)
        {
            fillSearchList();
        }
    }
}

void MaterialComboBox::setFieldEnabled(bool enabled)
{
    m_fieldEnabled = enabled;
    setEnabled(m_fieldEnabled && m_binding->isEnabled());
    if (!isEnabled())
    {
        hidePopup();
    }
}

QString MaterialComboBox::shownText() const
{
    return currentIndex() < 0 ? m_apartText : currentText();
}

void MaterialComboBox::fillGroups()
{
    const MaterialBinding& binding = *m_binding;
    const int              current = binding.indexOf(binding.value());
    for (QMenu* menu : m_groupMenus)
    {
        menu->clear();  // deletes the items it made
        menu->menuAction()->setVisible(false);
    }
    for (const MaterialBinding::GroupItems& group : binding.groups())
    {
        QMenu& menu = groupMenu(group.group);
        menu.setTitle(enumText(group.group));
        for (const int index : group.indices)
        {
            QAction* item = menu.addAction(textAt(index));
            item->setCheckable(true);
            item->setChecked(index == current);
            connect(item, &QAction::triggered, this,
                    [this, index] { Guard::run([this, index] { choose(index); }, objectName()); });
        }
        menu.menuAction()->setVisible(!m_searching);
    }
    // Java: the group of the value is checked, also when the value is in no database.
    for (std::size_t index = 0; index < kAllMaterialGroups.size(); ++index)
    {
        m_groupMenus.at(index)->menuAction()->setChecked(kAllMaterialGroups.at(index) ==
                                                         binding.value().getGroup());
    }
}

void MaterialComboBox::fillSearchList()
{
    const MaterialBinding&       binding   = *m_binding;
    const std::vector<Material>& materials = binding.materials();
    const QString                needle    = m_searchField->text().toLower();
    m_searchRows.clear();
    for (int index = 0; index < binding.size(); ++index)
    {
        if (textAt(index).toLower().contains(needle))
        {
            m_searchRows.push_back(index);
        }
    }
    // Java's SortedListModel: Material.toString() by String.compareToIgnoreCase().
    std::vector<std::string> keys;
    keys.reserve(materials.size());
    for (const Material& material : materials)
    {
        keys.push_back(Strings::javaCaseFold(material.toString()));
    }
    std::ranges::stable_sort(m_searchRows, [&keys](int left, int right) {
        return Strings::javaCompareTo(keys.at(static_cast<std::size_t>(left)),
                                      keys.at(static_cast<std::size_t>(right))) < 0;
    });

    const int current = binding.indexOf(binding.value());
    m_searchList->clear();
    for (const int index : m_searchRows)
    {
        QString text = textAt(index);
        if (index == current)
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

void MaterialComboBox::search(const QString& text)
{
    m_searching = !text.isEmpty();
    for (const QMenu* menu : m_groupMenus)
    {
        menu->menuAction()->setVisible(!m_searching && !menu->isEmpty());
    }
    if (m_searching)
    {
        // Java: the search popup takes the groups popup's size.
        const QSize size = m_popup->sizeHint();
        m_searchList->setFixedWidth(std::max(size.width(), m_searchField->sizeHint().width()));
        fillSearchList();
    }
    m_searchListAction->setVisible(m_searching);
    // A widget action's visibility does not reach its widget in a menu.
    m_searchList->setVisible(m_searching);
    if (m_popup->isVisible())
    {
        m_popup->resize(m_popup->sizeHint());
    }
}

bool MaterialComboBox::searchKey(int key)
{
    switch (key)
    {
        case Qt::Key_Escape:
            hidePopup();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        {
            const int row = m_searchList->currentRow();
            if (m_searching && row >= 0 && static_cast<std::size_t>(row) < m_searchRows.size())
            {
                const int index = m_searchRows.at(static_cast<std::size_t>(row));
                Guard::run([this, index] { choose(index); }, objectName());
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

void MaterialComboBox::choose(int index)
{
    hidePopup();  // Java: setSelectedItem() hides the popups first
    const Material* material = m_binding->materialAt(index);
    if (material == nullptr)
    {
        return;
    }
    // The binding's listeners may delete the dialog (with deleteLater(), or at once); the binding
    // copies nothing of the material before it sets it.
    const QPointer<MaterialComboBox>       self(this);
    const std::shared_ptr<MaterialBinding> binding = m_binding;
    const Material                         chosen  = *material;
    static_cast<void>(binding->select(chosen));
    if (!self.isNull())
    {
        refresh();  // whatever came of it: a dropped or refused material fires nothing
    }
}

void MaterialComboBox::activatedByUser(int index)
{
    const QPointer<MaterialComboBox> self(this);
    if (!Guard::run([this, index] { choose(index); }, objectName()) && !self.isNull())
    {
        refresh();
    }
}

void MaterialComboBox::showPopup()
{
    if (!isEnabled())
    {
        return;
    }
    refresh();
    m_searchField->clear();
    search(QString());
    fillGroups();
    m_popup->popup(mapToGlobal(QPoint(0, height())));
    m_searchField->setFocus(Qt::PopupFocusReason);
}

void MaterialComboBox::hidePopup()
{
    m_popup->hide();
}

bool MaterialComboBox::eventFilter(QObject* watched, QEvent* event)
{
    const auto* const key = dynamic_cast<const QKeyEvent*>(event);
    if (watched != m_popup || m_forwardingKey || key == nullptr ||
        event->type() != QEvent::KeyPress)
    {
        return QComboBox::eventFilter(watched, event);
    }
    const bool typed =
        key->key() == Qt::Key_Backspace || (!key->text().isEmpty() && key->text().at(0).isPrint());
    if (!typed)
    {
        return QComboBox::eventFilter(watched, event);
    }
    // A key the search field leaves goes on to the popup again: not back to the field.
    const QScopedValueRollback<bool> forwarding(m_forwardingKey, true);
    m_searchField->setFocus(Qt::PopupFocusReason);
    QCoreApplication::sendEvent(m_searchField, event);
    return true;
}

void MaterialComboBox::initStyleOption(QStyleOptionComboBox* option) const
{
    QComboBox::initStyleOption(option);
    if (currentIndex() < 0)
    {
        option->currentText = m_apartText;
    }
}

void MaterialComboBox::wheelEvent(QWheelEvent* event)
{
    if (!hasFocus())
    {
        event->ignore();  // on to the widgets around it: a scroll area scrolls
        return;
    }
    QComboBox::wheelEvent(event);
}

}  // namespace QtRocket::Gui
