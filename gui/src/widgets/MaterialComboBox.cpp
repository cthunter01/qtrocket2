#include "QtRocket/gui/widgets/MaterialComboBox.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <QComboBox>
#include <QList>
#include <QMenu>
#include <QPointer>
#include <QString>
#include <QStyleOptionComboBox>
#include <QWheelEvent>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/MaterialBinding.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/GroupedSearchPopup.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/util/BugError.h"

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
  : QComboBox(parent), m_binding(checkedBinding(std::move(binding))), m_popup(makePopup())
{
    // Not Qt::WheelFocus (QComboBox's): the wheel must not take the focus.
    setFocusPolicy(Qt::StrongFocus);
    m_connection =
        m_binding->addChangeListener([this] { Guard::run([this] { refresh(); }, objectName()); });
    connect(this, &QComboBox::activated, this, [this](int index) { activatedByUser(index); });
    refresh();
}

MaterialComboBox::~MaterialComboBox() = default;

GroupedSearchPopup* MaterialComboBox::makePopup()
{
    std::vector<QString> titles;
    titles.reserve(kAllMaterialGroups.size());
    for (const MaterialGroup group : kAllMaterialGroups)
    {
        titles.push_back(enumText(group));
    }
    GroupedSearchPopup::Content content{
        .itemCount = [this] { return m_binding->size(); },
        .text      = [this](int index) { return textAt(index); },
        .sortKey =
            [this](int index) {
                const Material* material = m_binding->materialAt(index);
                QTROCKET_ASSERT(material != nullptr);
                return material->toString();
            },
        .groups =
            [this] {
                std::vector<GroupedSearchPopup::Group> groups;
                for (const MaterialBinding::GroupItems& group : m_binding->groups())
                {
                    groups.push_back({.group = groupIndex(group.group), .items = group.indices});
                }
                return groups;
            },
        .current = [this] { return m_binding->indexOf(m_binding->value()); },
        .currentGroup =
            [this] {
                return std::optional<std::size_t>(groupIndex(m_binding->value().getGroup()));
            },
        .choose = [this](int index) { choose(index); },
    };
    //: MaterialPanel.MaterialComboBox.placeholder
    const QString placeholder = tr("Enter the material name");
    return makeChild<GroupedSearchPopup>(this, placeholder, titles, std::move(content));
}

QMenu& MaterialComboBox::groupMenu(MaterialGroup group) const
{
    return m_popup->groupMenu(groupIndex(group));
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
    else
    {
        m_popup->refill();
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
    m_popup->popUpBelow(*this);
}

void MaterialComboBox::hidePopup()
{
    m_popup->hide();
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
