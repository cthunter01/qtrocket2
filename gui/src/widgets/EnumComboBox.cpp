#include "QtRocket/gui/widgets/EnumComboBox.h"

#include <functional>
#include <memory>
#include <utility>

#include <QComboBox>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStyleOptionComboBox>
#include <QWheelEvent>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/Binding.h"
#include "QtRocket/gui/util/Guard.h"

namespace QtRocket::Gui
{

EnumComboBox::EnumComboBox(Model model, QWidget* parent)
  : QComboBox(parent), m_model(std::move(model))
{
    // Not Qt::WheelFocus (QComboBox's): the wheel must not take the focus.
    setFocusPolicy(Qt::StrongFocus);
    m_connection = m_model.binding->addChangeListener(
        [this] { Guard::run([this] { refresh(); }, objectName()); });
    connect(this, &QComboBox::activated, this, [this](int index) { activatedByUser(index); });
    refresh();
}

EnumComboBox::~EnumComboBox() = default;

void EnumComboBox::refresh()
{
    QList<QString> texts;
    const int      size = m_model.size();
    texts.reserve(size);
    for (int index = 0; index < size; ++index)
    {
        texts.append(m_model.textAt(index));
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
    const int current = m_model.currentIndex();
    setCurrentIndex(current);
    m_apartText = current < 0 ? m_model.valueText() : QString();
    setEnabled(m_fieldEnabled && m_model.isEnabled());
    update();
}

void EnumComboBox::setFieldEnabled(bool enabled)
{
    m_fieldEnabled = enabled;
    setEnabled(m_fieldEnabled && m_model.isEnabled());
}

QString EnumComboBox::shownText() const
{
    return currentIndex() < 0 ? m_apartText : currentText();
}

void EnumComboBox::activatedByUser(int index)
{
    // The binding's listeners may delete the dialog (with deleteLater(), or at once).
    const QPointer<EnumComboBox>   self(this);
    const std::shared_ptr<Binding> keepAlive = m_model.binding;
    const std::function<bool(int)> selectAt  = m_model.selectAt;
    Guard::run([&selectAt, index] { static_cast<void>(selectAt(index)); }, objectName());
    if (!self.isNull())
    {
        refresh();  // whatever came of it: a skipped, dropped or refused value fires nothing
    }
}

void EnumComboBox::initStyleOption(QStyleOptionComboBox* option) const
{
    QComboBox::initStyleOption(option);
    if (currentIndex() < 0)
    {
        option->currentText = m_apartText;
    }
}

void EnumComboBox::wheelEvent(QWheelEvent* event)
{
    if (!hasFocus())
    {
        event->ignore();  // on to the widgets around it: a scroll area scrolls
        return;
    }
    QComboBox::wheelEvent(event);
}

}  // namespace QtRocket::Gui
