#include "QtRocket/gui/widgets/DataTypeComboBox.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include <QComboBox>
#include <QFontMetrics>
#include <QMenu>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QStyle>
#include <QStyleOptionComboBox>
#include <QTextDocument>
#include <QWheelEvent>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/GroupedSearchPopup.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightDataTypeGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// @p types grouped as GroupableAndSearchableComboBox.constructItemGroupMapFromList() groups them:
/// group by group in priority order (a TreeMap by Group.getPriority()), each group's types sorted
/// by FlightDataType::compareTo() in a stable sort (List.sort()).
/// @throws BugError when one is null
[[nodiscard]] std::vector<const FlightDataType*> groupedTypes(
    const std::vector<const FlightDataType*>& types)
{
    if (std::ranges::any_of(types, [](const FlightDataType* type) { return type == nullptr; }))
    {
        bug("A data type combo box takes no null type");
    }
    std::vector<const FlightDataType*> grouped;
    grouped.reserve(types.size());
    for (const FlightDataTypeGroup group : kAllFlightDataTypeGroups)
    {
        std::vector<const FlightDataType*> members;
        std::ranges::copy_if(
            types, std::back_inserter(members),
            [group](const FlightDataType* type) { return type->getGroup() == group; });
        std::ranges::stable_sort(members,
                                 [](const FlightDataType* left, const FlightDataType* right) {
                                     return left->compareTo(*right) < 0;
                                 });
        grouped.insert(grouped.end(), members.begin(), members.end());
    }
    return grouped;
}

/// The capacity of a java.util.HashMap after @p count distinct keys were put into a new one: 16,
/// doubled whenever the size passes three quarters of it.
[[nodiscard]] std::uint32_t javaHashSetCapacity(std::size_t count)
{
    std::uint32_t capacity = 16;
    while (count > std::size_t{capacity} / 4 * 3)
    {
        capacity *= 2;
    }
    return capacity;
}

/// The bucket of @p type in a java.util.HashMap of @p capacity: HashMap.hash(), the hash code with
/// its high half folded into its low half, masked by the capacity.
[[nodiscard]] std::uint32_t javaHashSetBucket(const FlightDataType& type, std::uint32_t capacity)
{
    const auto hash = static_cast<std::uint32_t>(type.hashCode());
    return (hash ^ (hash >> 16U)) & (capacity - 1);
}

/// The types of the combo box's model, in its order: GroupableAndSearchableComboBox.
/// extractItemsFromMap() adds @p types group by group (groupedTypes()) to a new HashSet, which
/// keeps the first of equal types (FlightDataType.equals(): the names ignoring case) and iterates
/// in the order of its buckets, the types of a bucket in the order they were added.
/// @throws BugError when one is null
[[nodiscard]] std::vector<const FlightDataType*> modelTypes(
    const std::vector<const FlightDataType*>& types)
{
    std::vector<const FlightDataType*> distinct;
    for (const FlightDataType* const type : groupedTypes(types))
    {
        if (std::ranges::none_of(
                distinct, [type](const FlightDataType* other) { return other->equals(*type); }))
        {
            distinct.push_back(type);
        }
    }
    const std::uint32_t capacity = javaHashSetCapacity(distinct.size());
    std::ranges::stable_sort(
        distinct, [capacity](const FlightDataType* left, const FlightDataType* right) {
            return javaHashSetBucket(*left, capacity) < javaHashSetBucket(*right, capacity);
        });
    return distinct;
}

/// The index of @p group in kAllFlightDataTypeGroups.
[[nodiscard]] std::size_t groupIndex(FlightDataTypeGroup group)
{
    for (std::size_t index = 0; index < kAllFlightDataTypeGroups.size(); ++index)
    {
        if (kAllFlightDataTypeGroups.at(index) == group)
        {
            return index;
        }
    }
    QTROCKET_UNREACHABLE();
}

/// @p text as a tooltip shows it as plain text, as Swing shows a tooltip that does not start with
/// "<html>": Qt takes a text that looks like markup (Qt::mightBeRichText()) for rich text.
[[nodiscard]] QString plainToolTip(const QString& text)
{
    return Qt::mightBeRichText(text)
               ? QStringLiteral("<html>") + text.toHtmlEscaped() + QStringLiteral("</html>")
               : text;
}

}  // namespace

DataTypeComboBox::DataTypeComboBox(const std::vector<const FlightDataType*>& types, QWidget* parent)
  : QComboBox(parent), m_types(modelTypes(types)), m_popup(makePopup())
{
    // Not Qt::WheelFocus (QComboBox's): the wheel must not take the focus.
    setFocusPolicy(Qt::StrongFocus);
    setFixedWidth(kFixedWidth);
    for (const FlightDataType* const type : m_types)
    {
        addItem(flightDataTypeText(*type));
    }
    connect(this, &QComboBox::activated, this,
            [this](int index) { Guard::run([this, index] { choose(index); }, objectName()); });
    showSelection();
}

DataTypeComboBox::~DataTypeComboBox() = default;

GroupedSearchPopup* DataTypeComboBox::makePopup()
{
    std::vector<QString> titles;
    titles.reserve(kAllFlightDataTypeGroups.size());
    for (const FlightDataTypeGroup group : kAllFlightDataTypeGroups)
    {
        titles.push_back(enumText(group));
    }
    GroupedSearchPopup::Content content{
        .itemCount = [this] { return static_cast<int>(m_types.size()); },
        .text =
            [this](int index) {
                return flightDataTypeText(*m_types.at(static_cast<std::size_t>(index)));
            },
        // Java's SortedListModel compares toString(), the name.
        .sortKey =
            [this](int index) {
                return QtStrings::toUtf8(
                    flightDataTypeText(*m_types.at(static_cast<std::size_t>(index))));
            },
        .groups = [this] { return groups(); },
        // Java: the check mark of the item that is the selected object.
        .current =
            [this] {
                const auto found = std::ranges::find(m_types, m_selected);
                return m_selected == nullptr || found == m_types.end()
                           ? -1
                           : static_cast<int>(found - m_types.begin());
            },
        .currentGroup =
            [this] {
                return m_selected == nullptr
                           ? std::optional<std::size_t>()
                           : std::optional<std::size_t>(groupIndex(m_selected->getGroup()));
            },
        .choose = [this](int index) { choose(index); },
    };
    //: FlightDataComboBox.placeholder
    const QString placeholder = tr("Enter the data type");
    return makeChild<GroupedSearchPopup>(this, placeholder, titles, std::move(content));
}

std::vector<GroupedSearchPopup::Group> DataTypeComboBox::groups() const
{
    std::vector<GroupedSearchPopup::Group> groups;
    for (std::size_t group = 0; group < kAllFlightDataTypeGroups.size(); ++group)
    {
        std::vector<int> items;
        for (std::size_t index = 0; index < m_types.size(); ++index)
        {
            if (m_types.at(index)->getGroup() == kAllFlightDataTypeGroups.at(group))
            {
                items.push_back(static_cast<int>(index));
            }
        }
        if (items.empty())
        {
            continue;
        }
        // Java's ItemComparator: FlightDataType.compareTo(), in a stable List.sort() of the model's
        // types (contentsChanged() groups them again from the model once a type is selected).
        std::ranges::stable_sort(items, [this](int left, int right) {
            return m_types.at(static_cast<std::size_t>(left))
                       ->compareTo(*m_types.at(static_cast<std::size_t>(right))) < 0;
        });
        groups.push_back({.group = group, .items = std::move(items)});
    }
    return groups;
}

QMenu& DataTypeComboBox::groupMenu(FlightDataTypeGroup group) const
{
    return m_popup->groupMenu(groupIndex(group));
}

int DataTypeComboBox::selectedIndex() const
{
    if (m_selected == nullptr)
    {
        return -1;
    }
    for (std::size_t index = 0; index < m_types.size(); ++index)
    {
        if (m_types.at(index)->equals(*m_selected))
        {
            return static_cast<int>(index);
        }
    }
    return -1;
}

void DataTypeComboBox::setSelectedType(const FlightDataType* type)
{
    hidePopup();  // Java: setSelectedItem() hides the popups first
    // Java's DefaultComboBoxModel.setSelectedItem(): nothing when the new one equals the old.
    const bool same =
        m_selected == nullptr ? type == nullptr : type != nullptr && m_selected->equals(*type);
    if (same)
    {
        return;
    }
    m_selected = type;
    showSelection();
    Q_EMIT selectedTypeChanged();
}

void DataTypeComboBox::showSelection()
{
    setCurrentIndex(selectedIndex());
    // Java: updateTypeSelectorTooltip(), the whole name of the selected type.
    setToolTip(m_selected == nullptr ? QString() : plainToolTip(flightDataTypeText(*m_selected)));
    update();
}

QString DataTypeComboBox::shownText() const
{
    return m_selected == nullptr ? QString() : flightDataTypeText(*m_selected);
}

void DataTypeComboBox::choose(int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_types.size())
    {
        return;
    }
    const QPointer<DataTypeComboBox> self(this);
    setSelectedType(m_types.at(static_cast<std::size_t>(index)));
    if (!self.isNull())
    {
        showSelection();  // a choice equal to the selected type changes nothing: show it again
    }
}

void DataTypeComboBox::showPopup()
{
    if (!isEnabled())
    {
        return;
    }
    m_popup->popUpBelow(*this);
}

void DataTypeComboBox::hidePopup()
{
    m_popup->hide();
}

void DataTypeComboBox::initStyleOption(QStyleOptionComboBox* option) const
{
    QComboBox::initStyleOption(option);
    // Java's renderer, a JLabel, ends a text too long for the field with "...". The style draws
    // the text 1 px in from either side of the edit field (QCommonStyle's CE_ComboBoxLabel).
    const QRect field =
        style()->subControlRect(QStyle::CC_ComboBox, option, QStyle::SC_ComboBoxEditField, this);
    option->currentText =
        fontMetrics().elidedText(shownText(), Qt::ElideRight, std::max(field.width() - 2, 0));
}

void DataTypeComboBox::wheelEvent(QWheelEvent* event)
{
    if (!hasFocus())
    {
        event->ignore();  // on to the widgets around it: a scroll area scrolls
        return;
    }
    QComboBox::wheelEvent(event);
}

}  // namespace QtRocket::Gui
