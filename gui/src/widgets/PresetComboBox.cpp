#include "QtRocket/gui/widgets/PresetComboBox.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include <QComboBox>
#include <QEvent>
#include <QFont>
#include <QPaintEvent>
#include <QPalette>
#include <QPointer>
#include <QString>
#include <QStyle>
#include <QStyleOptionComboBox>
#include <QStylePainter>
#include <QWheelEvent>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/PresetBinding.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The rows of the list (Java: presetComboBox.setMaximumRowCount(25)).
constexpr int kMaximumRowCount = 25;

/// @p binding, which must not be null and must have a valid selection.
[[nodiscard]] std::shared_ptr<PresetBinding> checkedBinding(std::shared_ptr<PresetBinding> binding)
{
    if (binding == nullptr || !binding->isValid())
    {
        bug("A preset combo box needs a binding of a valid selection");
    }
    return binding;
}

/// @p font in italics.
[[nodiscard]] QFont italic(QFont font)
{
    font.setItalic(true);
    return font;
}

/// Whether @p item and @p value are equal presets (Java: ComponentPreset.equals(), the digest).
[[nodiscard]] bool samePreset(const ComponentPreset& item, const ComponentPreset& value)
{
    return &item == &value || item == value;
}

}  // namespace

PresetComboBox::PresetComboBox(std::shared_ptr<PresetBinding> binding, QWidget* parent)
  : QComboBox(parent),
    m_binding(checkedBinding(std::move(binding))),
    m_kind(m_binding->getSelection()->getMain().kind())
{
    // Not Qt::WheelFocus (QComboBox's): the wheel must not take the focus.
    setFocusPolicy(Qt::StrongFocus);
    setEditable(false);
    setMaxVisibleItems(kMaximumRowCount);
    //: PresetModel.combo.ttip
    setToolTip(
        tr("<html>Select a preset model from a list of favorites (selected in the component "
           "preset dialog), <br>or select 'Custom' when no preset is required.</html>"));
    m_connection =
        m_binding->addChangeListener([this] { Guard::run([this] { refresh(); }, objectName()); });
    connect(this, &QComboBox::activated, this, [this](int index) { activatedByUser(index); });
    refresh();
}

PresetComboBox::~PresetComboBox() = default;

QString PresetComboBox::customText()
{
    //: PresetModel.lbl.custompreset
    return tr("Custom");
}

QString PresetComboBox::undoDescription(ComponentKind kind)
{
    //: Hard-coded English in OpenRocket (PresetModel): "Use Preset " + getComponentName()
    return tr("Use Preset %1").arg(enumText(kind));
}

int PresetComboBox::itemOfValue() const
{
    const ComponentPreset* value = m_binding->value();
    if (value == nullptr)
    {
        return 0;
    }
    for (std::size_t index = 0; index < m_shownFavorites.size(); ++index)
    {
        if (samePreset(*m_shownFavorites.at(index), *value))
        {
            return static_cast<int>(index) + 1;
        }
    }
    return -1;
}

void PresetComboBox::setCustomFont()
{
    if (count() > 0)
    {
        setItemData(0, italic(font()), Qt::FontRole);
    }
}

void PresetComboBox::refresh()
{
    const PresetBinding& binding = *m_binding;
    if (!binding.isValid())
    {
        // The components are gone: the last state stays, and nothing can be chosen.
        hidePopup();
        setEnabled(false);
        return;
    }
    if (count() == 0 || binding.favorites() != m_shownFavorites)
    {
        // Rebuilt without a signal of the combo box's own: only the user's choices are acted on
        // (QComboBox::activated), and they never come from a refill.
        m_shownFavorites = binding.favorites();
        clear();
        addItem(customText());
        for (const PresetBinding::PresetPtr& preset : m_shownFavorites)
        {
            addItem(QtStrings::fromUtf8(preset->toString()));
        }
        setCustomFont();
    }
    const int current = itemOfValue();
    setCurrentIndex(current);
    m_apartText = current < 0 ? QtStrings::fromUtf8(binding.valueText()) : QString();
    update();
}

QString PresetComboBox::shownText() const
{
    return currentIndex() < 0 ? m_apartText : currentText();
}

QFont PresetComboBox::shownFont() const
{
    return currentIndex() == 0 ? italic(font()) : font();
}

bool PresetComboBox::choosePreset(const PresetBinding::PresetPtr& preset)
{
    // The binding's listeners may delete the dialog (with deleteLater(), or at once).
    const QPointer<PresetComboBox>       self(this);
    const std::shared_ptr<PresetBinding> binding = m_binding;
    if (!binding->choose(preset, QtStrings::toUtf8(undoDescription(m_kind))))
    {
        return false;
    }
    if (!self.isNull())
    {
        refresh();
        Q_EMIT presetChosen();
    }
    return true;
}

void PresetComboBox::chooseItem(int index)
{
    if (!m_binding->isValid() || index < 0 || std::cmp_greater(index, m_shownFavorites.size()))
    {
        return;
    }
    // Java: JComboBox.setSelectedItem() leaves the model alone for an item that equals() the one
    // it has selected (the component's preset, or "Custom" for none).
    const ComponentPreset* value = m_binding->value();
    if (index == 0)
    {
        if (value != nullptr)
        {
            static_cast<void>(m_binding->clear());
        }
        return;
    }
    const PresetBinding::PresetPtr preset =
        m_shownFavorites.at(static_cast<std::size_t>(index) - 1);
    if (value != nullptr && samePreset(*preset, *value))
    {
        return;
    }
    static_cast<void>(choosePreset(preset));
}

void PresetComboBox::activatedByUser(int index)
{
    const QPointer<PresetComboBox>       self(this);
    const std::shared_ptr<PresetBinding> keepAlive = m_binding;
    Guard::run([this, index] { chooseItem(index); }, objectName());
    if (!self.isNull())
    {
        refresh();  // whatever came of it: a refused or dropped choice fires nothing
    }
}

void PresetComboBox::changeEvent(QEvent* event)
{
    QComboBox::changeEvent(event);
    if (event->type() == QEvent::FontChange)
    {
        setCustomFont();
    }
}

void PresetComboBox::initStyleOption(QStyleOptionComboBox* option) const
{
    QComboBox::initStyleOption(option);
    if (currentIndex() < 0)
    {
        option->currentText = m_apartText;
    }
}

void PresetComboBox::paintEvent(QPaintEvent* /*event*/)
{
    // QComboBox::paintEvent(), with the label in shownFont() (Java's renderer writes
    // "<html><i>Custom</i></html>").
    QStylePainter painter(this);
    painter.setPen(palette().color(QPalette::Text));
    QStyleOptionComboBox option;
    initStyleOption(&option);
    painter.drawComplexControl(QStyle::CC_ComboBox, option);
    painter.setFont(shownFont());
    painter.drawControl(QStyle::CE_ComboBoxLabel, option);
}

void PresetComboBox::wheelEvent(QWheelEvent* event)
{
    if (!hasFocus())
    {
        event->ignore();  // on to the widgets around it: a scroll area scrolls
        return;
    }
    QComboBox::wheelEvent(event);
}

}  // namespace QtRocket::Gui
