#include "QtRocket/gui/widgets/IconToggleButton.h"

#include <QAbstractButton>
#include <QIcon>
#include <QPaintEvent>
#include <QPainter>
#include <QSize>
#include <QSizeF>
#include <QWidget>
#include <Qt>

namespace QtRocket::Gui
{

IconToggleButton::IconToggleButton(QWidget* parent) : QAbstractButton(parent)
{
    initSettings();
}

IconToggleButton::IconToggleButton(const QIcon& icon, QWidget* parent) : QAbstractButton(parent)
{
    initSettings();
    setIcon(icon);
}

IconToggleButton::IconToggleButton(const QIcon& icon, bool selected, QWidget* parent)
  : IconToggleButton(icon, parent)
{
    setChecked(selected);
}

void IconToggleButton::initSettings()
{
    setCheckable(true);
    // Java: no border, no focus, no margin, no content area.
    setFocusPolicy(Qt::NoFocus);
}

void IconToggleButton::setSelectedIcon(const QIcon& icon)
{
    setIcon(icon);
    m_greyUnselected = true;
    update();
}

void IconToggleButton::setIconScale(double scale)
{
    m_iconScale = scale;
    updateGeometry();
    update();
}

QSize IconToggleButton::sizeHint() const
{
    return (QSizeF(iconSize()) * m_iconScale).toSize();
}

QSize IconToggleButton::minimumSizeHint() const
{
    return sizeHint();
}

void IconToggleButton::paintEvent(QPaintEvent* /*event*/)
{
    const bool        showSelected = isChecked() || isDown() || !m_greyUnselected;
    const QIcon::Mode mode         = isEnabled() && showSelected ? QIcon::Normal : QIcon::Disabled;
    QPainter          painter(this);
    icon().paint(&painter, rect(), Qt::AlignCenter, mode, isChecked() ? QIcon::On : QIcon::Off);
}

}  // namespace QtRocket::Gui
