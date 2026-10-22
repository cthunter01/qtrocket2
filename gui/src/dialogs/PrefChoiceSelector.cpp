#include "QtRocket/gui/dialogs/PrefChoiceSelector.h"

#include <string_view>

#include <QComboBox>
#include <QString>
#include <QStringList>
#include <QWheelEvent>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/preferences/Preferences.h"

namespace QtRocket::Gui
{

PrefChoiceSelector::PrefChoiceSelector(Preferences& preferences, std::string_view preference,
                                       const QStringList& descriptions, QWidget* parent)
  : QComboBox(parent), m_preferences(&preferences), m_preference(preference)
{
    setObjectName(QtStrings::fromUtf8(preference));
    // Not Qt::WheelFocus (QComboBox's): the wheel must not take the focus.
    setFocusPolicy(Qt::StrongFocus);
    addItems(descriptions);
    refresh();
    connect(this, &QComboBox::activated, this,
            guarded([this](int index) { activatedByUser(index); }, objectName()));
}

PrefChoiceSelector::~PrefChoiceSelector() = default;

int PrefChoiceSelector::storedChoice() const
{
    // The last index is count() - 1 (see the class comment).
    return m_preferences->getChoice(m_preference, count() - 1, 0);
}

void PrefChoiceSelector::refresh()
{
    setCurrentIndex(storedChoice());
}

void PrefChoiceSelector::wheelEvent(QWheelEvent* event)
{
    if (!hasFocus())
    {
        event->ignore();  // on to the widgets around it
        return;
    }
    QComboBox::wheelEvent(event);
}

void PrefChoiceSelector::activatedByUser(int index)
{
    if (index < 0)
    {
        return;
    }
    m_preferences->putChoice(m_preference, index);
    refresh();
}

}  // namespace QtRocket::Gui
