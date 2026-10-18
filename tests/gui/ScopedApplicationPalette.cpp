#include "ScopedApplicationPalette.h"

#include <QApplication>
#include <QColor>
#include <QGuiApplication>
#include <QPalette>

namespace QtRocket::Gui::Test
{

ScopedApplicationPalette::ScopedApplicationPalette(const QPalette& palette)
  : m_previous(QGuiApplication::palette())
{
    QApplication::setPalette(palette);
}

ScopedApplicationPalette::~ScopedApplicationPalette()
{
    QApplication::setPalette(m_previous);
}

QPalette ScopedApplicationPalette::light()
{
    return {QColor(0xf0, 0xf0, 0xf0)};
}

QPalette ScopedApplicationPalette::dark()
{
    return {QColor(0x2b, 0x2b, 0x2b)};
}

}  // namespace QtRocket::Gui::Test
