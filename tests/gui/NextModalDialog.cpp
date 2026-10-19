#include "NextModalDialog.h"

#include <chrono>
#include <utility>

#include <QApplication>
#include <QObject>
#include <QTimer>
#include <QWidget>

#include <gtest/gtest.h>

namespace QtRocket::Gui::Test
{

NextModalDialog::NextModalDialog(Action action) : m_action(std::move(action))
{
    m_timer.setInterval(std::chrono::milliseconds(1));
    QObject::connect(&m_timer, &QTimer::timeout, &m_timer, [this] { poll(); });
    m_elapsed.start();
    m_timer.start();
}

NextModalDialog::~NextModalDialog()
{
    m_timer.stop();
    if (!m_acted)
    {
        ADD_FAILURE() << "no modal dialog was shown to answer";
    }
}

void NextModalDialog::poll()
{
    QWidget* const dialog = QApplication::activeModalWidget();
    if (dialog != nullptr && dialog->isVisible())
    {
        m_timer.stop();
        m_acted = true;
        m_action(*dialog);
        return;
    }
    if (m_elapsed.durationElapsed() > kTimeout)
    {
        m_timer.stop();
        m_acted = true;
        ADD_FAILURE() << "no modal dialog appeared within " << kTimeout.count() << " ms";
    }
}

}  // namespace QtRocket::Gui::Test
